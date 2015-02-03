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
#include <glib.h>
#include "drivers/cdrom/cdimpl.h"


static gboolean gdi_image_is_valid( FILE *f );
static gboolean gdi_image_read_toc( cdrom_disc_t disc, ERROR *err );

struct cdrom_disc_factory gdi_disc_factory = { "NullDC GD-Rom Image", "gdi",
        gdi_image_is_valid, NULL, gdi_image_read_toc };

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

static gboolean gdi_image_read_toc( cdrom_disc_t disc, ERROR *err )
{
    int i;
    uint32_t track_count;
    char line[512];
    int session = 1;
    gchar *dirname;

    FILE *f = cdrom_disc_get_base_file(disc);

    fseek(f, 0, SEEK_SET);

    if( fgets( line, sizeof(line), f ) == NULL ) {
        SET_ERROR( err, LX_ERR_FILE_INVALID, "Invalid GDI image" );
        return FALSE;
    }
    track_count = strtoul(line, NULL, 0);
    if( track_count == 0 || track_count > 99 ) {
        SET_ERROR( err, LX_ERR_FILE_INVALID, "Invalid GDI image" );
        return FALSE;
    }

    dirname = g_path_get_dirname(disc->name);
    disc->disc_type = CDROM_DISC_GDROM;
    disc->track_count = track_count;
    disc->session_count = 2;
    for( i=0; i<track_count; i++ ) {
        int track_no, start_lba, flags, size, offset;
        char filename[256];

        if( fgets( line, sizeof(line), f ) == NULL ) {
            cdrom_disc_unref(disc);
            SET_ERROR( err, LX_ERR_FILE_INVALID, "Invalid GDI image - unexpected end of file" );
            return FALSE;
        }
        sscanf( line, "%d %d %d %d %s %d", &track_no, &start_lba, &flags, &size,
                filename, &offset );
        if( start_lba >= 45000 ) {
            session = 2;
        }
        disc->track[i].sessionno = session;
        disc->track[i].lba = start_lba;
        disc->track[i].flags = (flags & 0x0F)<<4;

        sector_mode_t mode;
        if( disc->track[i].flags & TRACK_FLAG_DATA ) {
            /* Data track */
            switch(size) {
            case 0:    mode = SECTOR_MODE2_FORM1; break; /* Default */
            case 2048: mode = SECTOR_MODE2_FORM1; break;
            case 2324: mode = SECTOR_MODE2_FORM2; break;
            case 2336: mode = SECTOR_SEMIRAW_MODE2; break;
            case 2352: mode = SECTOR_RAW_XA; break;
            default:
                SET_ERROR( err, LX_ERR_FILE_INVALID, "Invalid sector size '%d' in GDI track %d", size, (i+1) );
                g_free(dirname);
                return FALSE;
            }
        } else {
            /* Audio track */
            mode = SECTOR_CDDA;
            if( size == 0 )
                size = 2352;
            else if( size != 2352 ) {
                SET_ERROR( err, LX_ERR_FILE_INVALID, "Invalid sector size '%d' for audio track %d", size, (i+1) );
                g_free(dirname);
                return FALSE;
            }
        }
        if( strcasecmp( filename, "none" ) == 0 ) {
            disc->track[i].source = null_sector_source_new( mode, 0 );
        } else {
            gchar *pathname = g_strdup_printf( "%s%c%s", dirname, G_DIR_SEPARATOR, filename );
            disc->track[i].source = file_sector_source_new_filename( pathname, mode,
                    offset, FILE_SECTOR_FULL_FILE );
            g_free(pathname);
            if( disc->track[i].source == NULL ) {
                /* Note: status is invalid because it's a failure of the GDI file */
                SET_ERROR( err, LX_ERR_FILE_INVALID, "GDI track file '%s' could not be opened (%s)", filename, strerror(errno) );
                g_free(dirname);
                return FALSE;
            }
        }
    }
    g_free(dirname);
    return TRUE;
}
