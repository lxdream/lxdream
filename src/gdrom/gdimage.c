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

#include <netinet/in.h>

#include "gdrom/gdrom.h"
#include "gdrom/packet.h"
#include "bootstrap.h"

static void gdrom_image_destroy( gdrom_disc_t disc );
static gdrom_error_t gdrom_image_read_sector( gdrom_disc_t disc, uint32_t lba, int mode, 
					      unsigned char *buf, uint32_t *readlength );
static gdrom_error_t gdrom_image_read_toc( gdrom_disc_t disc, unsigned char *buf );
static gdrom_error_t gdrom_image_read_session( gdrom_disc_t disc, int session, unsigned char *buf );
static gdrom_error_t gdrom_image_read_position( gdrom_disc_t disc, uint32_t lba, unsigned char *buf );
static int gdrom_image_drive_status( gdrom_disc_t disc );

struct cdrom_sector_header {
    uint8_t sync[12];
    uint8_t msf[3];
    uint8_t mode;
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
    gdrom_image_t image = (gdrom_image_t)calloc(sizeof(struct gdrom_image), 1);
    if( image == NULL ) {
	return NULL;
    }
    image->disc_type = IDE_DISC_CDROM;
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

static int gdrom_image_get_track_by_lba( gdrom_image_t image, uint32_t lba )
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
static void gdrom_read_block( unsigned char *buf, int file_offset, int length, FILE *f )
{
    if( file_offset < 0 ) {
	int size = -file_offset;
	if( size >= length ) {
	    memset( buf, 0, length );
	    return;
	} else {
	    memset( buf, 0, size );
	    file_offset = 0;
	    length -= size;
	}
    }
    fseek( f, file_offset, SEEK_SET );
    fread( buf, length, 1, f );
}

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

    

    switch( mode ) {
    case 0x20: /* Actually, read anything, but for now... */
    case 0x24:
    case 0x28:
	read_len = 2048;
	switch( track->mode ) {
	case GDROM_MODE1:
	case GDROM_MODE2_XA1:
	    gdrom_read_block( buf, file_offset, track->sector_size, f );
	    break;
	case GDROM_MODE2:
	    file_offset += 8; /* skip the subheader */
	    gdrom_read_block( buf, file_offset, 2048, f );
	    break;
	case GDROM_RAW:
	    gdrom_read_block( (unsigned char *)(&secthead), file_offset, sizeof(secthead), f );
	    switch( secthead.mode ) {
	    case 1:
		file_offset += 16;
		break;
	    case 2:
		file_offset += 24;
		break;
	    default:
		return PKT_ERR_BADREADMODE;
	    }
	    gdrom_read_block( buf, file_offset, 2048, f );
	    break;
	default:
	    return PKT_ERR_BADREADMODE;
	}
	break;
    default:
	return PKT_ERR_BADREADMODE;
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
    return image->disc_type | IDE_DISC_READY;
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

