/**
 * $Id$
 *
 * OSX native cd-rom driver.
 *
 * Copyright (c) 2005 Nathan Keynes.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <IOKit/storage/IOCDTypes.h>
#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <paths.h>
#include "drivers/osx_iokit.h"
#include "drivers/cdrom/cdimpl.h"
#include "drivers/cdrom/drive.h"

#define MAXTOCENTRIES 600  /* This is a fairly generous overestimate really */
#define MAXTOCSIZE 4 + (MAXTOCENTRIES*11)

static gboolean cdrom_osx_image_is_valid( FILE *f );
static cdrom_disc_t cdrom_osx_open_file( FILE *f, const gchar *filename, ERROR *err );
static gboolean cdrom_osx_read_toc( cdrom_disc_t disc, ERROR *err );
static cdrom_error_t cdrom_osx_read_sectors( sector_source_t disc, cdrom_lba_t sector, cdrom_count_t count,
                                             cdrom_read_mode_t mode, unsigned char *buf, size_t *length );

/* Note: We don't support opening OS X devices by filename, so no disc factory */

#define OSX_DRIVE(disc) ( (osx_cdrom_drive_t)(((cdrom_disc_t)disc)->impl_data) )

static void cdrom_osx_destroy( sector_source_t disc )
{
    osx_cdrom_close_drive( OSX_DRIVE(disc) );
    default_cdrom_disc_destroy( disc );
}

static void cdrom_osx_media_changed( osx_cdrom_drive_t drive, gboolean present,
                                     void *user_data )
{
    cdrom_disc_t disc = (cdrom_disc_t)user_data;
    if( present ) {
        cdrom_osx_read_toc( disc, NULL );
    } else {
        disc->disc_type = CDROM_DISC_NONE;
        disc->track_count = 0;        
    }
}


static cdrom_disc_t cdrom_osx_new( const char *name, osx_cdrom_drive_t drive, ERROR *err )
{
    cdrom_disc_t disc = cdrom_disc_new(name, err);
    disc->impl_data = drive;

    disc->source.read_sectors = cdrom_osx_read_sectors;
    disc->source.destroy = cdrom_osx_destroy;
    disc->read_toc = cdrom_osx_read_toc;
    cdrom_disc_read_toc(disc, err);
    osx_cdrom_set_media_changed_callback( drive, cdrom_osx_media_changed, disc );
    return (cdrom_disc_t)disc;
}

static cdrom_disc_t cdrom_osx_open( cdrom_drive_t drive, ERROR *err )
{
    cdrom_disc_t result = NULL;

    osx_cdrom_drive_t osx_drive = osx_cdrom_open_drive(drive->name);
    if( osx_drive == NULL ) {
        SET_ERROR( err, LX_ERR_FILE_NOOPEN, "Unable to open CDROM drive" );
        return NULL;
    } else {
        return cdrom_osx_new( drive->name, osx_drive, err );
    }
}



static gboolean cdrom_enum_callback( io_object_t object, char *vendor, char *product, char *iopath, void *ptr )
{
    char tmp1[sizeof(io_string_t) + 6];
    char tmp2[512];
    snprintf( tmp1, sizeof(tmp1), "dvd://%s", iopath );
    snprintf( tmp2, sizeof(tmp2), "%s %s", vendor, product );
    cdrom_drive_add( iopath, tmp2, cdrom_osx_open );
    return FALSE;
}

void cdrom_drive_scan(void)
{
    find_cdrom_drive(cdrom_enum_callback, NULL);
    osx_register_iokit_notifications();
}

static gboolean cdrom_osx_image_is_valid( FILE *f )
{
    return FALSE;
}

static cdrom_disc_t cdrom_osx_open_file( FILE *f, const gchar *filename, ERROR *err )
{
    return NULL; /* Not supported */
}

static gboolean cdrom_osx_read_toc( cdrom_disc_t disc, ERROR *err )
{
    osx_cdrom_drive_t drive = OSX_DRIVE(disc);

    int fh = osx_cdrom_get_media_handle(drive);
    if( fh == -1 ) {
        disc->disc_type = CDROM_DISC_NONE;
        disc->track_count = 0;
        return FALSE;
    } else {
        unsigned char buf[MAXTOCSIZE];
        dk_cd_read_toc_t readtoc;
        memset( &readtoc, 0, sizeof(readtoc) );
        readtoc.format = 2;
        readtoc.formatAsTime = 0;
        readtoc.address.session = 0;
        readtoc.bufferLength = sizeof(buf);
        readtoc.buffer = buf;

        if( ioctl(fh, DKIOCCDREADTOC, &readtoc ) == -1 ) {
            WARN( "Failed to read TOC (%s)", strerror(errno) );
            disc->disc_type = CDROM_DISC_NONE;
            disc->track_count = 0;
            return FALSE;
        } else {
            mmc_parse_toc2( disc, buf );
        }
    }
    return TRUE;
}

static cdrom_error_t cdrom_osx_read_sectors( sector_source_t source, cdrom_lba_t lba, cdrom_count_t count,
                                            cdrom_read_mode_t mode, unsigned char *buf, size_t *length )
{
    cdrom_disc_t disc = (cdrom_disc_t)source;
    int sector_size = 2352;
    char data[CDROM_MAX_SECTOR_SIZE];
    osx_cdrom_drive_t drive = OSX_DRIVE(disc);
    
    int fh = osx_cdrom_get_media_handle(drive);
    if( fh == -1 ) {
        return CDROM_ERROR_NODISC;
    } else {
        dk_cd_read_t readcd;
        memset( &readcd, 0, sizeof(readcd) );
        readcd.buffer = buf;
        readcd.sectorType = CDROM_READ_TYPE(mode);

        cdrom_track_t track = cdrom_disc_get_track_by_lba(disc,lba);
        if( track == NULL ) {
            return CDROM_ERROR_BADREAD;
        }

        /* This is complicated by needing to know the exact read size. Gah.
         * For now, anything other than a data-only read of known size is
         * executed by a raw read + extraction.
         */
        if( (CDROM_READ_FIELDS(mode) == CDROM_READ_DATA && CDROM_READ_TYPE(mode) != CDROM_READ_ANY) ||
            ((track->flags & TRACK_FLAG_DATA) == 0 && CDROM_READ_FIELDS(mode) != CDROM_READ_NONE) ) {
            switch( CDROM_READ_TYPE(mode) ) {
            case CDROM_READ_ANY:
            case CDROM_READ_CDDA:
                sector_size = 2352;
                break;
            case CDROM_READ_MODE1:
            case CDROM_READ_MODE2_FORM1:
                sector_size = 2048;
                break;
            case CDROM_READ_MODE2:
                sector_size = 2336;
                break;
            case CDROM_READ_MODE2_FORM2:
                sector_size = 2324;
                break;
            }
            readcd.sectorArea = kCDSectorAreaUser;
            readcd.offset = sector_size * lba;
            readcd.bufferLength = sector_size * count;
            if( ioctl( fh, DKIOCCDREAD, &readcd ) == -1 ) {
                return CDROM_ERROR_BADREAD;
            }
            *length = sector_size * count;
        } else {
            /* Sector could be anything - need to do a raw read and then parse
             * the requested data out ourselves
             */
            sector_size = 2352;
            size_t tmplen, len = 0;

            readcd.offset = sector_size * lba;
            readcd.sectorArea = 0xf8;
            readcd.buffer = data;
            readcd.bufferLength = sector_size;
            while( count > 0 ) {
                if( ioctl( fh, DKIOCCDREAD, &readcd ) == -1 ) {
                    return CDROM_ERROR_BADREAD;
                }
                cdrom_error_t err = sector_extract_from_raw( data, mode, &buf[len], &tmplen );
                if( err != CDROM_ERROR_OK )
                    return err;
                len += tmplen;
                readcd.offset += sector_size;
            }
            *length = len;
        }

        return CDROM_ERROR_OK;
    }
}

