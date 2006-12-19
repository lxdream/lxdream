/**
 * $Id: gdrom.c,v 1.11 2006-12-19 09:52:56 nkeynes Exp $
 *
 * GD-Rom  access functions.
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

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include "gdrom/ide.h"
#include "gdrom/gdrom.h"
#include "gdrom/packet.h"
#include "dream.h"

static void gdrom_image_destroy( gdrom_disc_t );
static gdrom_error_t gdrom_image_read_sectors( gdrom_disc_t, uint32_t, uint32_t, int, char *, uint32_t * );

gdrom_image_class_t gdrom_image_classes[] = { &linux_device_class, &nrg_image_class, &cdi_image_class, NULL };

gdrom_disc_t gdrom_disc = NULL;

char *gdrom_mode_names[] = { "Mode1", "Mode2", "XA 1", "XA2", "Audio", "GD-Rom" };
uint32_t gdrom_sector_size[] = { 2048, 2336, 2048, 2324, 2352, 2336 };

gdrom_disc_t gdrom_image_open( const gchar *filename )
{
    const gchar *ext = strrchr(filename, '.');
    gdrom_disc_t disc = NULL;

    int fd = open( filename, O_RDONLY | O_NONBLOCK );
    FILE *f;
    int i,j;
    gdrom_image_class_t extclz = NULL;

    if( fd == -1 ) {
	ERROR("Unable to open file '%s': %s", filename, strerror(errno));
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

    if( !recognized ) {
	ERROR( "Unable to open disc %s: Unsupported format", filename );
    }
    fclose(f);
    return NULL;
}


gdrom_disc_t gdrom_image_new( FILE *file )
{
    struct gdrom_disc *disc = (struct gdrom_disc *)calloc(1, sizeof(struct gdrom_disc));
    if( disc == NULL )
	return NULL;
    disc->read_sectors = gdrom_image_read_sectors;
    disc->close = gdrom_image_destroy;
    disc->disc_type = IDE_DISC_CDROM;
    disc->file = file;
    return disc;
}

static void gdrom_image_destroy( gdrom_disc_t disc )
{
    if( disc->file != NULL ) {
	fclose(disc->file);
	disc->file = NULL;
    }
    free( disc );
}

static gdrom_error_t gdrom_image_read_sectors( gdrom_disc_t disc, uint32_t sector,
					       uint32_t sector_count, int mode, char *buf,
					       uint32_t *length )
{
    int i, file_offset, read_len;
    struct gdrom_track *track = NULL;

    for( i=0; i<disc->track_count; i++ ) {
	if( disc->track[i].lba <= sector && 
	    (sector + sector_count) <= (disc->track[i].lba + disc->track[i].sector_count) ) {
	    track = &disc->track[i];
	    break;
	}
    }
    if( track == NULL )
	return PKT_ERR_BADREAD;

    file_offset = track->offset + track->sector_size * (sector - track->lba);
    read_len = track->sector_size * sector_count;

    switch( mode ) {
    case GDROM_GD:
	// Temporarily comment this out - it's wrong, but...
	//	if( track->mode != GDROM_GD ) 
	//    return PKT_ERR_BADREADMODE;
	// break;
    case GDROM_MODE1:
    case GDROM_MODE2_XA1:
	switch( track->mode ) {
	case GDROM_MODE1:
	case GDROM_MODE2_XA1:
	    fseek( disc->file, file_offset, SEEK_SET );
	    fread( buf, track->sector_size, sector_count, disc->file );
	    break;
	case GDROM_MODE2:
	    read_len = sector_count * 2048;
	    file_offset += 8; /* skip the subheader */
	    while( sector_count > 0 ) {
		fseek( disc->file, file_offset, SEEK_SET );
		fread( buf, 2048, 1, disc->file );
		file_offset += track->sector_size;
		buf += 2048;
		sector_count--;
	    }
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

uint32_t gdrom_read_sectors( uint32_t sector, uint32_t sector_count,
			     int mode, char *buf, uint32_t *length )
{
    if( gdrom_disc == NULL )
	return PKT_ERR_NODISC; /* No media */
    return gdrom_disc->read_sectors( gdrom_disc, sector, sector_count, mode, buf, length );
}


void gdrom_dump_disc_info( gdrom_disc_t disc ) {
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
	    char boot_sector[2048];
	    uint32_t length = sizeof(boot_sector);
	    if( disc->read_sectors( disc, disc->track[boot_track].lba, 1, GDROM_MODE1,
				    boot_sector, &length ) == PKT_ERR_OK ) {
		bootstrap_dump(boot_sector, FALSE);
		is_bootable = TRUE;
	    }
	}
    }
    if( !is_bootable )
	WARN( "Disc does not appear to be bootable" );
}

gdrom_error_t gdrom_get_toc( char *buf ) 
{
    struct gdrom_toc *toc = (struct gdrom_toc *)buf;
    int i;

    if( gdrom_disc == NULL )
	return PKT_ERR_NODISC;

    for( i=0; i<gdrom_disc->track_count; i++ ) {
	toc->track[i] = htonl( gdrom_disc->track[i].lba ) | gdrom_disc->track[i].flags;
    }
    toc->first = 0x0100 | gdrom_disc->track[0].flags;
    toc->last = (gdrom_disc->track_count<<8) | gdrom_disc->track[i-1].flags;
    toc->leadout = htonl(gdrom_disc->track[i-1].lba + gdrom_disc->track[i-1].sector_count) |
	gdrom_disc->track[i-1].flags;
    for( ;i<99; i++ )
	toc->track[i] = 0xFFFFFFFF;
    return PKT_ERR_OK;
}

gdrom_error_t gdrom_get_info( char *buf, int session )
{
    if( gdrom_disc == NULL )
	return PKT_ERR_NODISC;
    struct gdrom_track *last_track = &gdrom_disc->track[gdrom_disc->track_count-1];
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
	for( i=0; i<gdrom_disc->track_count; i++ ) {
	    if( gdrom_disc->track[i].session == session ) {
		buf[2] = i+1; /* first track of session */
		buf[3] = (gdrom_disc->track[i].lba >> 16) & 0xFF;
		buf[4] = (gdrom_disc->track[i].lba >> 8) & 0xFF;
		buf[5] = gdrom_disc->track[i].lba & 0xFF;
		return PKT_ERR_OK;
	    }
	}
	return PKT_ERR_BADFIELD; /* No such session */
    }
	
}

gdrom_track_t gdrom_get_track( int trackno ) {
    if( gdrom_disc == NULL || trackno < 1 || trackno > 99 ) {
	return NULL;
    } else {
	return &gdrom_disc->track[trackno-1];
    }
}

uint8_t gdrom_get_track_no_by_lba( uint32_t lba ) {
    int i;
    if( gdrom_disc != NULL ) {
	for( i=0; i<gdrom_disc->track_count; i++ ) {
	    if( gdrom_disc->track[i].lba <= lba && 
		lba <= (gdrom_disc->track[i].lba + gdrom_disc->track[i].sector_count) ) {
		return i+1;
	    }
	}
    }
    return -1;
}

void gdrom_mount_disc( gdrom_disc_t disc ) 
{
    gdrom_unmount_disc();
    gdrom_disc = disc;
    idereg.disc = disc->disc_type | IDE_DISC_READY;
    gdrom_dump_disc_info( disc );
}

gdrom_disc_t gdrom_mount_image( const gchar *filename )
{
    gdrom_disc_t disc = gdrom_image_open(filename);
    if( disc != NULL )
	gdrom_mount_disc( disc );
    return disc;
}

void gdrom_unmount_disc( ) 
{
    if( gdrom_disc != NULL ) {
	gdrom_disc->close(gdrom_disc);
    }
    gdrom_disc = NULL;
    idereg.disc = IDE_DISC_NONE;
}

gboolean gdrom_is_mounted( void ) 
{
    return gdrom_disc != NULL;
}
