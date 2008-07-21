
/**
 * $Id$
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
#include <glib/gutils.h>
#include "gdrom/ide.h"
#include "gdrom/gdrom.h"
#include "gdrom/gddriver.h"
#include "gdrom/packet.h"
#include "dream.h"

extern gdrom_disc_t gdrom_disc;

DEFINE_HOOK( gdrom_disc_change_hook, gdrom_disc_change_hook_t )

static void gdrom_fire_disc_changed( gdrom_disc_t disc )
{
    CALL_HOOKS( gdrom_disc_change_hook, disc, disc == NULL ? NULL : disc->name );
}

gdrom_image_class_t gdrom_image_classes[] = { &cdrom_device_class, 
        &nrg_image_class, 
        &cdi_image_class, 
        &gdi_image_class, 
        NULL };

char *gdrom_mode_names[] = { "Mode 0", "Mode 1", "Mode 2", "Mode 2 Form 1", "Mode 2 Form 2", "Audio", 
        "Mode 2 semiraw", "XA Raw", "Non-XA Raw" };
uint32_t gdrom_sector_size[] = { 0, 2048, 2336, 2048, 2324, 2352, 2336, 2352, 2352 };

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

void gdrom_mount_disc( gdrom_disc_t disc ) 
{
    if( disc != gdrom_disc ) {
        if( gdrom_disc != NULL ) {
            gdrom_disc->close(gdrom_disc);
        }
        gdrom_disc = disc;
        gdrom_image_dump_info( disc );
        gdrom_fire_disc_changed( disc );
    }
}

gboolean gdrom_mount_image( const gchar *filename )
{
    gdrom_disc_t disc = gdrom_image_open(filename);
    if( disc != NULL ) {         
        gdrom_mount_disc( disc );
        return TRUE;
    }
    return FALSE;
}

void gdrom_unmount_disc( ) 
{
    if( gdrom_disc != NULL ) {
        gdrom_disc->close(gdrom_disc);
        gdrom_fire_disc_changed(NULL);
        gdrom_disc = NULL;
    }
}

gdrom_disc_t gdrom_get_current_disc()
{
    return gdrom_disc;
}

const gchar *gdrom_get_current_disc_name()
{
    if( gdrom_disc == NULL ) {
        return NULL;
    } else {
        return gdrom_disc->name;
    }
}

gchar *gdrom_get_relative_filename( const gchar *base_name, const gchar *rel_name )
{
    gchar *dirname = g_path_get_dirname(base_name);
    gchar *pathname = g_strdup_printf( "%s%c%s", dirname, G_DIR_SEPARATOR, rel_name );
    g_free(dirname);
    return pathname;
}
