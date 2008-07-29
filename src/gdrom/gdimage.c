/**
 * $Id$
 *
 * GD-Rom image-file common functions. 
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "gdrom/gddriver.h"
#include "gdrom/packet.h"
#include "ecc.h"
#include "bootstrap.h"

static void gdrom_image_destroy( gdrom_disc_t disc );
static gdrom_error_t gdrom_image_read_sector( gdrom_disc_t disc, uint32_t lba, int mode, 
                                              unsigned char *buf, uint32_t *readlength );
static gdrom_error_t gdrom_image_read_toc( gdrom_disc_t disc, unsigned char *buf );
static gdrom_error_t gdrom_image_read_session( gdrom_disc_t disc, int session, unsigned char *buf );
static gdrom_error_t gdrom_image_read_position( gdrom_disc_t disc, uint32_t lba, unsigned char *buf );
static int gdrom_image_drive_status( gdrom_disc_t disc );

static uint8_t gdrom_default_sync[12] = { 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0 };

#define SECTOR_HEADER_SIZE 16
#define SECTOR_SUBHEADER_SIZE 8

/* Data offset (from start of raw sector) by sector mode */
static int gdrom_data_offset[] = { 16, 16, 16, 24, 24, 0, 8, 0, 0 };



struct cdrom_sector_header {
    uint8_t sync[12];
    uint8_t msf[3];
    uint8_t mode;
    uint8_t subhead[8]; // Mode-2 XA sectors only
};

/**
 * Initialize a gdrom_disc structure with the gdrom_image_* methods
 */
void gdrom_image_init( gdrom_disc_t disc )
{
    memset( disc, 0, sizeof(struct gdrom_disc) ); /* safety */
    disc->read_sector = gdrom_image_read_sector;
    disc->read_toc = gdrom_image_read_toc;
    disc->read_session = gdrom_image_read_session;
    disc->read_position = gdrom_image_read_position;
    disc->drive_status = gdrom_image_drive_status;
    disc->play_audio = NULL; /* not supported yet */
    disc->run_time_slice = NULL; /* not needed */
    disc->close = gdrom_image_destroy;
}

gdrom_disc_t gdrom_image_new( const gchar *filename, FILE *f )
{
    gdrom_image_t image = (gdrom_image_t)g_malloc0(sizeof(struct gdrom_image));
    if( image == NULL ) {
        return NULL;
    }
    image->disc_type = IDE_DISC_CDROMXA;
    image->file = f;
    gdrom_disc_t disc = (gdrom_disc_t)image;
    gdrom_image_init(disc);
    if( filename == NULL ) {
        disc->name = NULL;
    } else {
        disc->name = g_strdup(filename);
    }

    return disc;
}

static void gdrom_image_destroy( gdrom_disc_t disc )
{
    int i;
    FILE *lastfile = NULL;
    gdrom_image_t img = (gdrom_image_t)disc;
    if( img->file != NULL ) {
        fclose(img->file);
        img->file = NULL;
    }
    for( i=0; i<img->track_count; i++ ) {
        if( img->track[i].file != NULL && img->track[i].file != lastfile ) {
            lastfile = img->track[i].file;
            fclose(lastfile);
            img->track[i].file = NULL;
        }
    }
    if( disc->name != NULL ) {
        g_free( (gpointer)disc->name );
        disc->name = NULL;
    }
    free( disc );
}

void gdrom_image_destroy_no_close( gdrom_disc_t disc )
{
    int i;
    FILE *lastfile = NULL;
    gdrom_image_t img = (gdrom_image_t)disc;
    if( img->file != NULL ) {
        img->file = NULL;
    }
    for( i=0; i<img->track_count; i++ ) {
        if( img->track[i].file != NULL && img->track[i].file != lastfile ) {
            lastfile = img->track[i].file;
            fclose(lastfile);
            img->track[i].file = NULL;
        }
    }
    if( disc->name != NULL ) {
        g_free( (gpointer)disc->name );
        disc->name = NULL;
    }
    free( disc );
}

int gdrom_image_get_track_by_lba( gdrom_image_t image, uint32_t lba )
{
    int i;
    for( i=0; i<image->track_count; i++ ) {
        if( image->track[i].lba <= lba && 
                lba < (image->track[i].lba + image->track[i].sector_count) ) {
            return i+1;
        }
    }
    return -1;
}

/**
 * Read a block from an image file, handling negative file offsets
 * with 0-fill.
 */
static gboolean gdrom_read_block( unsigned char *buf, int file_offset, int length, FILE *f )
{
    if( file_offset < 0 ) {
        int size = -file_offset;
        if( size >= length ) {
            memset( buf, 0, length );
            return TRUE;
        } else {
            memset( buf, 0, size );
            file_offset = 0;
            length -= size;
        }
    }
    fseek( f, file_offset, SEEK_SET );
    return fread( buf, length, 1, f ) == 1;
}

static void gdrom_build_sector_header( unsigned char *buf, uint32_t lba, 
                                       gdrom_track_mode_t sector_mode )
{
    memcpy( buf, gdrom_default_sync, 12 );
    cd_build_address( buf, sector_mode, lba );
}

/**
 * Return TRUE if the given read mode + track modes are compatible,
 * otherwise FALSE.
 * @param track_mode one of the GDROM_MODE* constants
 * @param read_mode the READ_CD_MODE from the read request
 */
static gboolean gdrom_is_compatible_read_mode( int track_mode, int read_mode )
{
    switch( read_mode ) {
    case READ_CD_MODE_ANY:
        return TRUE;
    case READ_CD_MODE_CDDA:
        return track_mode == GDROM_CDDA;
    case READ_CD_MODE_1:
        return track_mode == GDROM_MODE1 || track_mode == GDROM_MODE2_FORM1;
    case READ_CD_MODE_2_FORM_1:
        return track_mode == GDROM_MODE1 || track_mode == GDROM_MODE2_FORM1;
    case READ_CD_MODE_2_FORM_2:
        return track_mode == GDROM_MODE2_FORM2;
    case READ_CD_MODE_2:
        return track_mode == GDROM_MODE2_FORMLESS;
    default:
        return FALSE;
    }
}

/**
 * Determine the start position in a raw sector, and the amount of data to read
 * in bytes, for a given combination of sector mode and read mode.
 */ 
static void gdrom_get_read_bounds( int sector_mode, int read_mode, int *start, int *size )
{
    if( READ_CD_RAW(read_mode) ) {
        // whole sector
        *start = 0;
        *size = 2352;
    } else {
        *size = 0;
        if( READ_CD_DATA(read_mode) ) {
            *start = gdrom_data_offset[sector_mode];
            *size = gdrom_sector_size[sector_mode];
        }

        if( READ_CD_SUBHEAD(read_mode) && 
                (sector_mode == GDROM_MODE2_FORM1 || sector_mode == GDROM_MODE2_FORM2) ) {
            *start = SECTOR_HEADER_SIZE;
            *size += SECTOR_SUBHEADER_SIZE;
        }

        if( READ_CD_HEADER(read_mode) ) {
            *size += SECTOR_HEADER_SIZE;
            *start = 0;
        }

    }
}

void gdrom_extract_raw_data_sector( char *sector_data, int channels, unsigned char *buf, uint32_t *length )
{
    int sector_mode;
    int start, size;
    struct cdrom_sector_header *secthead = (struct cdrom_sector_header *)sector_data;
    if( secthead->mode == 1 ) {
        sector_mode = GDROM_MODE1;
    } else {
        sector_mode = ((secthead->subhead[2] & 0x20) == 0 ) ? GDROM_MODE2_FORM1 : GDROM_MODE2_FORM2;
    }
    gdrom_get_read_bounds( sector_mode, channels, &start, &size );
    
    memcpy( buf, sector_data+start, size );
    *length = size;
}

/**
 * Read a single sector from a disc image. If you thought this would be simple, 
 * I have just one thing to say to you: Bwahahahahahahahah.
 *
 * Once we've decided that there's a real sector at the requested lba, there's 
 * really two things we need to care about:
 *   1. Is the sector mode compatible with the requested read mode
 *   2. Which parts of the sector do we need to return? 
 *      (header/subhead/data/raw sector)
 *
 * Also note that the disc image may supply us with just the data (most common 
 * case), or may have the full raw sector. In the former case we may need to 
 * generate the missing data on the fly, for which we use libedc to compute the
 * data correction codes.
 */
static gdrom_error_t gdrom_image_read_sector( gdrom_disc_t disc, uint32_t lba,
                                              int mode, unsigned char *buf, uint32_t *length )
{
    gdrom_image_t image = (gdrom_image_t)disc;
    struct cdrom_sector_header secthead;
    int file_offset, read_len, track_no;

    FILE *f;

    track_no = gdrom_image_get_track_by_lba( image, lba );
    if( track_no == -1 ) {
        return PKT_ERR_BADREAD;
    }
    struct gdrom_track *track = &image->track[track_no-1];
    file_offset = track->offset + track->sector_size * (lba - track->lba);
    read_len = track->sector_size;
    if( track->file != NULL ) {
        f = track->file;
    } else {
        f = image->file;
    }

    /* First figure out what the real sector mode is for raw/semiraw sectors */
    int sector_mode;
    switch( track->mode ) {
    case GDROM_RAW_NONXA:
        gdrom_read_block( (unsigned char *)(&secthead), file_offset, sizeof(secthead), f );
        sector_mode = (secthead.mode == 1) ? GDROM_MODE1 : GDROM_MODE2_FORMLESS;
        break;
    case GDROM_RAW_XA:
        gdrom_read_block( (unsigned char *)(&secthead), file_offset, sizeof(secthead), f );
        if( secthead.mode == 1 ) {
            sector_mode = GDROM_MODE1;
        } else {
            sector_mode = ((secthead.subhead[2] & 0x20) == 0 ) ? GDROM_MODE2_FORM1 : GDROM_MODE2_FORM2;
        }
        break;
    case GDROM_SEMIRAW_MODE2:
        gdrom_read_block( secthead.subhead, file_offset, 8, f );
        sector_mode = ((secthead.subhead[2] & 0x20) == 0 ) ? GDROM_MODE2_FORM1 : GDROM_MODE2_FORM2;
        break;
    default:
        /* In the other cases, the track mode completely defines the sector mode */
        sector_mode = track->mode;
        break;
    }

    if( !gdrom_is_compatible_read_mode(sector_mode, READ_CD_MODE(mode)) ) {
        return PKT_ERR_BADREADMODE;
    }

    /* Ok, we've got a valid sector, check what parts of the sector we need to
     * return - header | subhead | data | everything
     */
    int channels = READ_CD_CHANNELS(mode);

    if( channels == 0 ) {
        // legal, if weird
        *length = 0;
        return PKT_ERR_OK;
    } else if( channels == 0xA0 && 
            (sector_mode == GDROM_MODE2_FORM1 || sector_mode == GDROM_MODE2_FORM2 )) {
        // caller requested a non-contiguous region
        return PKT_ERR_BADFIELD;
    } else if( READ_CD_RAW(channels) ) {
        channels = 0xF0; // implies everything
    }

    read_len = 0;
    int start, size;
    switch( track->mode ) {
    case GDROM_CDDA:
        // audio is nice and simple (assume perfect reads for now)
        *length = 2352;
        gdrom_read_block( buf, file_offset, track->sector_size, f );
        return PKT_ERR_OK;
    case GDROM_RAW_XA:
    case GDROM_RAW_NONXA:
        gdrom_get_read_bounds( sector_mode, channels, &start, &size );
        gdrom_read_block( buf, file_offset+start, size, f );
        read_len = size;
        break;
    case GDROM_SEMIRAW_MODE2:
        gdrom_get_read_bounds( sector_mode, channels, &start, &size );
        if( READ_CD_HEADER(channels) ) {
            gdrom_build_sector_header( buf, lba, sector_mode );
            read_len += SECTOR_HEADER_SIZE;
            size -= SECTOR_HEADER_SIZE;
        } else {
            start -= SECTOR_HEADER_SIZE;
        }
        gdrom_read_block( buf + read_len, file_offset+start, size, f );
        read_len += size;
        break;
    default: // Data track w/ data only in file
        if( READ_CD_RAW(channels) ) {
            gdrom_read_block( buf + gdrom_data_offset[track->mode], file_offset, 
                    track->sector_size, f );
            do_encode_L2( buf, sector_mode, lba );
            read_len = 2352;
        } else {
            if( READ_CD_HEADER(channels) ) {
                gdrom_build_sector_header( buf, lba, sector_mode );
                read_len += SECTOR_HEADER_SIZE;
            }
            if( READ_CD_SUBHEAD(channels) && 
                    (sector_mode == GDROM_MODE2_FORM1 || sector_mode == GDROM_MODE2_FORM2) ) {
                if( sector_mode == GDROM_MODE2_FORM1 ) {
                    *((uint32_t *)(buf+read_len)) = 0;
                    *((uint32_t *)(buf+read_len+4)) = 0;
                } else {
                    *((uint32_t *)(buf+read_len)) = 0x00200000;
                    *((uint32_t *)(buf+read_len+4)) = 0x00200000;
                }
                read_len += 8;
            }
            if( READ_CD_DATA(channels) ) {
                gdrom_read_block( buf+read_len, file_offset, track->sector_size, f );
                read_len += track->sector_size;
            }
        }
    }
    *length = read_len;
    return PKT_ERR_OK;
}

static gdrom_error_t gdrom_image_read_toc( gdrom_disc_t disc, unsigned char *buf ) 
{
    gdrom_image_t image = (gdrom_image_t)disc;
    struct gdrom_toc *toc = (struct gdrom_toc *)buf;
    int i;

    for( i=0; i<image->track_count; i++ ) {
        toc->track[i] = htonl( image->track[i].lba ) | image->track[i].flags;
    }
    toc->first = 0x0100 | image->track[0].flags;
    toc->last = (image->track_count<<8) | image->track[i-1].flags;
    toc->leadout = htonl(image->track[i-1].lba + image->track[i-1].sector_count) |
    image->track[i-1].flags;
    for( ;i<99; i++ )
        toc->track[i] = 0xFFFFFFFF;
    return PKT_ERR_OK;
}

static gdrom_error_t gdrom_image_read_session( gdrom_disc_t disc, int session, unsigned char *buf )
{
    gdrom_image_t image = (gdrom_image_t)disc;
    struct gdrom_track *last_track = &image->track[image->track_count-1];
    unsigned int end_of_disc = last_track->lba + last_track->sector_count;
    int i;
    buf[0] = 0x01; /* Disc status? */
    buf[1] = 0;

    if( session == 0 ) {
        buf[2] = last_track->session+1; /* last session */
        buf[3] = (end_of_disc >> 16) & 0xFF;
        buf[4] = (end_of_disc >> 8) & 0xFF;
        buf[5] = end_of_disc & 0xFF;
        return PKT_ERR_OK;
    } else {
        session--;
        for( i=0; i<image->track_count; i++ ) {
            if( image->track[i].session == session ) {
                buf[2] = i+1; /* first track of session */
                buf[3] = (image->track[i].lba >> 16) & 0xFF;
                buf[4] = (image->track[i].lba >> 8) & 0xFF;
                buf[5] = image->track[i].lba & 0xFF;
                return PKT_ERR_OK;
            }
        }
        return PKT_ERR_BADFIELD; /* No such session */
    }
}

static gdrom_error_t gdrom_image_read_position( gdrom_disc_t disc, uint32_t lba, unsigned char *buf )
{
    gdrom_image_t image = (gdrom_image_t)disc;
    int track_no = gdrom_image_get_track_by_lba( image, lba );
    if( track_no == -1 ) {
        track_no = 1;
        lba = 150;
    }
    struct gdrom_track *track = &image->track[track_no-1];
    uint32_t offset = lba - track->lba;
    buf[4] = track->flags;
    buf[5] = track_no;
    buf[6] = 0x01; /* ?? */
    buf[7] = (offset >> 16) & 0xFF;
    buf[8] = (offset >> 8) & 0xFF;
    buf[9] = offset & 0xFF;
    buf[10] = 0;
    buf[11] = (lba >> 16) & 0xFF;
    buf[12] = (lba >> 8) & 0xFF;
    buf[13] = lba & 0xFF;
    return PKT_ERR_OK;
}

static int gdrom_image_drive_status( gdrom_disc_t disc ) 
{
    gdrom_image_t image = (gdrom_image_t)disc;
    if( image->disc_type == IDE_DISC_NONE ) {
        return IDE_DISC_NONE;
    } else {
        return image->disc_type | IDE_DISC_READY;
    }
}

void gdrom_image_dump_info( gdrom_disc_t d ) {
    gdrom_image_t disc = (gdrom_image_t)d;
    int i;
    int last_session = disc->track[disc->track_count-1].session;
    gboolean is_bootable = FALSE;

    INFO( "Disc ID: %s, %d tracks in %d sessions", disc->mcn, disc->track_count, 
          disc->track[disc->track_count-1].session + 1 );
    if( last_session > 0 ) {
        /* Boot track is the first data track of the last session, provided that it 
         * cannot be a single-session disc.
         */
        int boot_track = -1;
        for( i=disc->track_count-1; i>=0 && disc->track[i].session == last_session; i-- ) {
            if( disc->track[i].flags & TRACK_DATA ) {
                boot_track = i;
            }
        }
        if( boot_track != -1 ) {
            unsigned char boot_sector[MAX_SECTOR_SIZE];
            uint32_t length = sizeof(boot_sector);
            if( d->read_sector( d, disc->track[boot_track].lba, 0x28,
                    boot_sector, &length ) == PKT_ERR_OK ) {
                bootstrap_dump(boot_sector, FALSE);
                is_bootable = TRUE;
            }
        }
    }
    if( !is_bootable ) {
        WARN( "Disc does not appear to be bootable" );
    }
}

gdrom_device_t gdrom_device_new( const gchar *name, const gchar *dev_name )
{
    struct gdrom_device *dev = g_malloc0( sizeof(struct gdrom_device) );
    dev->name = g_strdup(name);
    dev->device_name = g_strdup(dev_name);
    return dev;
}

void gdrom_device_destroy( gdrom_device_t dev )
{
    if( dev->name != NULL ) {
        g_free( dev->name );
        dev->name = NULL;
    }
    if( dev->device_name != NULL ) {
        g_free( dev->device_name );
        dev->device_name = NULL;
    }
    g_free( dev );
}
