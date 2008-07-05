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
#include "gdrom/gddriver.h"
#include "gdrom/packet.h"
#include "drivers/osx_iokit.h"

#define MAXTOCENTRIES 600  /* This is a fairly generous overestimate really */
#define MAXTOCSIZE 4 + (MAXTOCENTRIES*11)

static gboolean cdrom_osx_image_is_valid( FILE *f );
static gdrom_disc_t cdrom_osx_open_device( const gchar *filename, FILE *f );
static gdrom_error_t cdrom_osx_read_toc( gdrom_image_t disc );
static gdrom_error_t cdrom_osx_read_sector( gdrom_disc_t disc, uint32_t sector,
                    int mode, unsigned char *buf, uint32_t *length );

struct gdrom_image_class cdrom_device_class = { "osx", NULL,
                        cdrom_osx_image_is_valid, cdrom_osx_open_device };

#define OSX_DRIVE(disc) ( (osx_cdrom_drive_t)(((gdrom_image_t)disc)->private) )

static void cdrom_osx_destroy( gdrom_disc_t disc )
{
    osx_cdrom_close_drive( OSX_DRIVE(disc) );
    gdrom_image_destroy_no_close( disc );
}

static void cdrom_osx_media_changed( osx_cdrom_drive_t drive, gboolean present,
                                     void *user_data )
{
    gdrom_image_t image = (gdrom_image_t)user_data;
    if( present ) {
        cdrom_osx_read_toc( image );
    } else {
        image->disc_type = IDE_DISC_NONE;
        image->track_count = 0;        
    }
}


static gdrom_disc_t cdrom_osx_new( const char *name, osx_cdrom_drive_t drive )
{
    char tmp[strlen(name)+7];
    sprintf( tmp, "dvd://%s", name );
    gdrom_image_t image = (gdrom_image_t)gdrom_image_new(tmp, NULL);
    image->private = drive;
    
    image->disc.read_sector = cdrom_osx_read_sector;
    image->disc.close = cdrom_osx_destroy;
    cdrom_osx_read_toc(image);
    osx_cdrom_set_media_changed_callback( drive, cdrom_osx_media_changed, image );
    return (gdrom_disc_t)image;
}

gdrom_disc_t cdrom_open_device( const gchar *method, const gchar *path )
{
    gdrom_disc_t result = NULL;
    
    osx_cdrom_drive_t drive = osx_cdrom_open_drive(path);
    if( drive == NULL ) {
        return NULL;
    } else {
        return cdrom_osx_new( path, drive );
    }
}



static gboolean cdrom_enum_callback( io_object_t object, char *vendor, char *product, char *iopath, void *ptr )
{
    GList **list = (GList **)ptr;
    char tmp1[sizeof(io_string_t) + 6];
    char tmp2[512];
    snprintf( tmp1, sizeof(tmp1), "dvd://%s", iopath );
    snprintf( tmp2, sizeof(tmp2), "%s %s", vendor, product );
    *list = g_list_append( *list, gdrom_device_new( tmp1, tmp2 ) );
    return FALSE;
}

GList *cdrom_get_native_devices(void)
{
    GList *list = NULL;
    find_cdrom_drive(cdrom_enum_callback, &list);
    
    osx_register_iokit_notifications();
    return list;
}



static gboolean cdrom_osx_image_is_valid( FILE *f )
{
    return FALSE;
}

static gdrom_disc_t cdrom_osx_open_device( const gchar *filename, FILE *f )
{
    return NULL;
}

static gdrom_error_t cdrom_osx_read_toc( gdrom_image_t image )
{
    osx_cdrom_drive_t drive = OSX_DRIVE(image);
    
    int fh = osx_cdrom_get_media_handle(drive);
    if( fh == -1 ) {
        image->disc_type = IDE_DISC_NONE;
        image->track_count = 0;
        return -1;
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
            ERROR( "Failed to read TOC: %s", strerror(errno) );
            image->disc_type = IDE_DISC_NONE;
            image->track_count = 0;
            return -1;
        } else {
            mmc_parse_toc2( image, buf );
        }
    }
    return 0;
}

static gdrom_error_t cdrom_osx_read_sector( gdrom_disc_t disc, uint32_t sector,
                    int mode, unsigned char *buf, uint32_t *length ) 
{
    osx_cdrom_drive_t drive = OSX_DRIVE(disc);
    int fh = osx_cdrom_get_media_handle(drive);
    if( fh == -1 ) {
        return PKT_ERR_NODISC;
    } else {
        dk_cd_read_t readcd;

        memset( &readcd, 0, sizeof(readcd) );
        // This is complicated by needing to know the exact read size. Gah.
        if( READ_CD_RAW(mode) ) {
            *length = 2352;
            readcd.sectorArea = 0xF8; 
        } else {
            // This is incomplete...
            if( READ_CD_DATA(mode) ) {
                readcd.sectorArea = kCDSectorAreaUser;
                switch( READ_CD_MODE(mode) ) {
                case READ_CD_MODE_CDDA:
                    *length = 2352;
                    break;
                case READ_CD_MODE_1:
                case READ_CD_MODE_2_FORM_1:
                    *length = 2048;
                    break;
                case READ_CD_MODE_2:
                    *length = 2336;
                    break;
                case READ_CD_MODE_2_FORM_2:
                    *length = 2324;
                    break;
                }
            }
        }
        
        readcd.offset = *length * (sector - 150);
        readcd.sectorType = READ_CD_MODE(mode)>>1;
        readcd.bufferLength = *length;
        readcd.buffer = buf;
        if( ioctl( fh, DKIOCCDREAD, &readcd ) == -1 ) {
            ERROR( "Failed to read CD: %s", strerror(errno) );
            return -1;
        } else {
            return 0;
        }
    }
}

