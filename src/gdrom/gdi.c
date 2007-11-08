/**
 * $Id: gdi.c,v 1.2 2007-11-08 10:48:41 nkeynes Exp $
 *
 * NullDC GDI image format
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <glib/gutils.h>
#include "gdrom/gdrom.h"


static gboolean gdi_image_is_valid( FILE *f );
static gdrom_disc_t gdi_image_open( const gchar *filename, FILE *f );

struct gdrom_image_class gdi_image_class = { "NullDC GD-Rom Image", "gdi", 
					     gdi_image_is_valid, gdi_image_open };

static gboolean gdi_image_is_valid( FILE *f )
{
    char line[512];
    uint32_t track_count;
   
    fseek(f, 0, SEEK_SET);
    if( fgets( line, sizeof(line), f ) == NULL ) {
	return FALSE;
    }
    track_count = strtoul(line, NULL, 0);
    if( track_count == 0 || track_count > 99 ) {
	return FALSE;
    }
    return TRUE;
}

static gdrom_disc_t gdi_image_open( const gchar *filename, FILE *f )
{
    int i;
    uint32_t track_count;
    gdrom_disc_t disc;
    gdrom_image_t image;
    struct stat st;
    char line[512];
    gchar *dirname;

    fseek(f, 0, SEEK_SET);
    
    if( fgets( line, sizeof(line), f ) == NULL ) {
	return FALSE;
    }
    track_count = strtoul(line, NULL, 0);
    if( track_count == 0 || track_count > 99 ) {
	return NULL;
    }

    disc = gdrom_image_new(filename, f);
    if( disc == NULL ) {
	ERROR("Unable to allocate memory!");
	return NULL;
    }
    dirname = g_path_get_dirname(filename);
    image = (gdrom_image_t)disc;
    image->disc_type = IDE_DISC_GDROM;
    image->track_count = track_count;
    for( i=0; i<track_count; i++ ) {
	int track_no, start_lba, flags, size, offset;
	char filename[256];

	if( fgets( line, sizeof(line), f ) == NULL ) {
	    gdrom_image_destroy_no_close(disc);
	    return NULL;
	}
	sscanf( line, "%d %d %d %d %s %d", &track_no, &start_lba, &flags, &size,
		filename, &offset );
	if( start_lba >= 45000 ) {
	    image->track[i].session = 1;
	} else {
	    image->track[i].session = 0;
	}
	image->track[i].lba = start_lba + 150; // 2-second offset
	image->track[i].flags = (flags & 0x0F)<<4;
	image->track[i].sector_size = size;
	if( strcasecmp( filename, "none" ) == 0 ) {
	    image->track[i].file = NULL;
	    image->track[i].sector_count = 0;
	    image->track[i].mode = GDROM_MODE1;
	} else {
	    gchar *pathname = g_strdup_printf( "%s%c%s", dirname, G_DIR_SEPARATOR, filename );
	    image->track[i].file = fopen( pathname, "ro" );
	    g_free(pathname);
	    if( image->track[i].file == NULL ) {
		gdrom_image_destroy_no_close(disc);
		g_free(dirname);
		return NULL;
	    }
	    fstat( fileno(image->track[i].file), &st );
	    image->track[i].sector_count = st.st_size / size;
	    if( image->track[i].flags & TRACK_DATA ) {
		/* Data track */
		switch(size) {
		case 2048: image->track[i].mode = GDROM_MODE1; break;
		case 2336: image->track[i].mode = GDROM_GD; break;
		case 2352: image->track[i].mode = GDROM_RAW; break;
		default:
		    gdrom_image_destroy_no_close(disc);
		    g_free(dirname);
		    return NULL;
		}
	    } else {
		/* Audio track */
		image->track[i].mode = GDROM_CDDA;
		if( size != 2352 ) {
		    gdrom_image_destroy_no_close(disc);
		    g_free(dirname);
		    return NULL;
		}
	    }
	}
	image->track[i].offset = offset;
    }
    g_free(dirname);
    return disc;
}
