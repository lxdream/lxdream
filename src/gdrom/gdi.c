/**
 * $Id$
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
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <glib/gutils.h>
#include "gdrom/gddriver.h"


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
    struct stat st;
    char line[512];
    int session = 0;
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
    disc->disc_type = IDE_DISC_GDROM;
    disc->track_count = track_count;
    for( i=0; i<track_count; i++ ) {
        int track_no, start_lba, flags, size, offset;
        char filename[256];

        if( fgets( line, sizeof(line), f ) == NULL ) {
            disc->destroy(disc,FALSE);
            return NULL;
        }
        sscanf( line, "%d %d %d %d %s %d", &track_no, &start_lba, &flags, &size,
                filename, &offset );
        if( start_lba >= 45000 ) {
            session = 1;
        }
        disc->track[i].session = session;
        disc->track[i].lba = start_lba + 150; // 2-second offset
        disc->track[i].flags = (flags & 0x0F)<<4;
        disc->track[i].sector_size = size;
        if( strcasecmp( filename, "none" ) == 0 ) {
            disc->track[i].file = NULL;
            disc->track[i].sector_count = 0;
            disc->track[i].mode = GDROM_MODE1;
        } else {
            gchar *pathname = g_strdup_printf( "%s%c%s", dirname, G_DIR_SEPARATOR, filename );
            disc->track[i].file = fopen( pathname, "ro" );
            g_free(pathname);
            if( disc->track[i].file == NULL ) {
                disc->destroy(disc,FALSE);
                g_free(dirname);
                return NULL;
            }
            fstat( fileno(disc->track[i].file), &st );
            disc->track[i].sector_count = st.st_size / size;
            if( disc->track[i].flags & TRACK_DATA ) {
                /* Data track */
                switch(size) {
                case 2048: disc->track[i].mode = GDROM_MODE1; break;
                case 2336: disc->track[i].mode = GDROM_SEMIRAW_MODE2; break;
                case 2352: disc->track[i].mode = GDROM_RAW_XA; break;
                default:
                    disc->destroy(disc,FALSE);
                    g_free(dirname);
                    return NULL;
                }
            } else {
                /* Audio track */
                disc->track[i].mode = GDROM_CDDA;
                if( size != 2352 ) {
                    disc->destroy(disc,FALSE);
                    g_free(dirname);
                    return NULL;
                }
            }
        }
        disc->track[i].offset = offset;
    }
    g_free(dirname);
    return disc;
}
