
/**
 * $Id: gdrom.c,v 1.14 2007-10-09 08:45:00 nkeynes Exp $
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

extern gdrom_disc_t gdrom_disc;

gdrom_image_class_t gdrom_image_classes[] = { &linux_device_class, &nrg_image_class, &cdi_image_class, NULL };

char *gdrom_mode_names[] = { "Mode1", "Mode2", "XA 1", "XA2", "Audio", "GD-Rom" };
uint32_t gdrom_sector_size[] = { 2048, 2336, 2048, 2324, 2352, 2336 };

gdrom_disc_t gdrom_image_open( const gchar *filename )
{
    const gchar *ext = strrchr(filename, '.');
    gdrom_disc_t disc = NULL;

    int fd = open( filename, O_RDONLY | O_NONBLOCK );
    FILE *f;
    int i;
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

void gdrom_mount_disc( gdrom_disc_t disc ) 
{
    gdrom_unmount_disc();
    gdrom_disc = disc;
    gdrom_image_dump_info( disc );
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
    }
    gdrom_disc = NULL;

}

