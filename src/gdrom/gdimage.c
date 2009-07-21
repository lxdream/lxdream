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
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>

#include "gdrom/gddriver.h"
#include "gdrom/packet.h"
#include "ecc.h"

static gboolean gdrom_null_check_status( gdrom_disc_t disc );
static gdrom_error_t gdrom_image_read_sector( gdrom_disc_t disc, uint32_t lba, int mode, 
                                              unsigned char *buf, uint32_t *readlength );

static uint8_t gdrom_default_sync[12] = { 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0 };

#define SECTOR_HEADER_SIZE 16
#define SECTOR_SUBHEADER_SIZE 8

/* Data offset (from start of raw sector) by sector mode */
static int gdrom_data_offset[] = { 16, 16, 16, 24, 24, 0, 8, 0, 0 };

gdrom_image_class_t gdrom_image_classes[] = { &cdrom_device_class, 
        &nrg_image_class, 
        &cdi_image_class, 
        &gdi_image_class, 
        NULL };

struct cdrom_sector_header {
    uint8_t sync[12];
    uint8_t msf[3];
    uint8_t mode;
    uint8_t subhead[8]; // Mode-2 XA sectors only
};

gdrom_disc_t gdrom_disc_new( const gchar *filename, FILE *f )
{
    gdrom_disc_t disc = (gdrom_disc_t)g_malloc0(sizeof(struct gdrom_disc));
    if( disc == NULL ) {
        return NULL;
    }
    disc->disc_type = IDE_DISC_NONE;
    disc->file = f;
    if( filename == NULL ) {
        disc->name = NULL;
    } else {
        disc->name = g_strdup(filename);
    }

	disc->check_status = gdrom_null_check_status;
	disc->destroy = gdrom_disc_destroy;
    return disc;
}

void gdrom_disc_destroy( gdrom_disc_t disc, gboolean close_fh )
{
    int i;
    FILE *lastfile = NULL;
    if( disc->file != NULL ) {
    	if( close_fh ) {
        	fclose(disc->file);
    	}
        disc->file = NULL;
    }
    for( i=0; i<disc->track_count; i++ ) {
        if( disc->track[i].file != NULL && disc->track[i].file != lastfile ) {
            lastfile = disc->track[i].file;
            /* Track files (if any) are closed regardless of the value of close_fh */
            fclose(lastfile);
            disc->track[i].file = NULL;
        }
    }
    if( disc->name != NULL ) {
        g_free( (gpointer)disc->name );
        disc->name = NULL;
    }
    if( disc->display_name != NULL ) {
    	g_free( (gpointer)disc->name );
    	disc->display_name = NULL;
    }
    free( disc );
}

/**
 * Construct a new gdrom_disc_t and initalize the vtable to the gdrom image
 * default functions.
 */
gdrom_disc_t gdrom_image_new( const gchar *filename, FILE *f )
{
	gdrom_disc_t disc = gdrom_disc_new( filename, f );
	if( disc != NULL ) {
	    disc->read_sector = gdrom_image_read_sector;
	    disc->play_audio = NULL; /* not supported yet */
	    disc->run_time_slice = NULL; /* not needed */
	}
	return disc;
}


gdrom_disc_t gdrom_image_open( const gchar *inFilename )
{
    const gchar *filename = inFilename;
    const gchar *ext = strrchr(filename, '.');
    gdrom_disc_t disc = NULL;
    int fd;
    FILE *f;
    int i;
    gdrom_image_class_t extclz = NULL;

    // Check for a url-style filename.
    char *lizard_lips = strstr( filename, "://" );
    if( lizard_lips != NULL ) {
        gchar *path = lizard_lips + 3;
        int method_len = (lizard_lips-filename);
        gchar method[method_len + 1];
        memcpy( method, filename, method_len );
        method[method_len] = '\0';

        if( strcasecmp( method, "file" ) == 0 ) {
            filename = path;
        } else if( strcasecmp( method, "dvd" ) == 0 ||
                strcasecmp( method, "cd" ) == 0 ||
                strcasecmp( method, "cdrom" ) ) {
            return cdrom_open_device( method, path );
        } else {
            ERROR( "Unrecognized URL method '%s' in filename '%s'", method, filename );
            return NULL;
        }
    }

    fd = open( filename, O_RDONLY | O_NONBLOCK );
    if( fd == -1 ) {
        return NULL;
    }

    f = fdopen(fd, "ro");


    /* try extensions */
    if( ext != NULL ) {
        ext++; /* Skip the '.' */
        for( i=0; gdrom_image_classes[i] != NULL; i++ ) {
            if( gdrom_image_classes[i]->extension != NULL &&
                    strcasecmp( gdrom_image_classes[i]->extension, ext ) == 0 ) {
                extclz = gdrom_image_classes[i];
                if( extclz->is_valid_file(f) ) {
                    disc = extclz->open_image_file(filename, f);
                    if( disc != NULL )
                        return disc;
                }
                break;
            }
        }
    }

    /* Okay, fall back to magic */
    gboolean recognized = FALSE;
    for( i=0; gdrom_image_classes[i] != NULL; i++ ) {
        if( gdrom_image_classes[i] != extclz &&
                gdrom_image_classes[i]->is_valid_file(f) ) {
            recognized = TRUE;
            disc = gdrom_image_classes[i]->open_image_file(filename, f);
            if( disc != NULL )
                return disc;
        }
    }

    fclose(f);
    return NULL;
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

void gdrom_set_disc_type( gdrom_disc_t disc ) 
{
    int type = IDE_DISC_NONE, i;
    for( i=0; i<disc->track_count; i++ ) {
        if( disc->track[i].mode == GDROM_CDDA ) {
            if( type == IDE_DISC_NONE )
                type = IDE_DISC_AUDIO;
        } else if( disc->track[i].mode == GDROM_MODE1 || disc->track[i].mode == GDROM_RAW_NONXA ) {
            if( type != IDE_DISC_CDROMXA )
                type = IDE_DISC_CDROM;
        } else {
            type = IDE_DISC_CDROMXA;
            break;
        }
    }
    disc->disc_type = type;
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
 * Default check media status that does nothing and always returns
 * false (unchanged).
 */
static gboolean gdrom_null_check_status( gdrom_disc_t disc )
{
	return FALSE;
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
    struct cdrom_sector_header secthead;
    int file_offset, read_len, track_no;

    FILE *f;

    track_no = gdrom_disc_get_track_by_lba( disc, lba );
    if( track_no == -1 ) {
        return PKT_ERR_BADREAD;
    }
    struct gdrom_track *track = &disc->track[track_no-1];
    file_offset = track->offset + track->sector_size * (lba - track->lba);
    read_len = track->sector_size;
    if( track->file != NULL ) {
        f = track->file;
    } else {
        f = disc->file;
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

