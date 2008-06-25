/**
 * $Id$
 *
 * The "null" cdrom device driver. Just provides a couple of empty stubs.
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

#include "gdrom/gddriver.h"

static gboolean cdnone_image_is_valid( FILE *f );
static gdrom_disc_t cdnone_open_device( const gchar *filename, FILE *f );

struct gdrom_image_class cdrom_device_class = { "None", NULL,
						cdnone_image_is_valid, cdnone_open_device };

GList *cdrom_get_native_devices(void)
{
    return NULL;
}

gdrom_disc_t cdrom_open_device( const gchar *method, const gchar *path )
{
    return NULL;
}



static gboolean cdnone_image_is_valid( FILE *f )
{
    return FALSE;
}

static gdrom_disc_t cdnone_open_device( const gchar *filename, FILE *f )
{
    return NULL;
}
