/**
 * $Id: cdnone.c,v 1.1 2007-11-04 05:07:49 nkeynes Exp $
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

#include "gdrom/gdrom.h"

static gboolean cdnone_image_is_valid( FILE *f );
static gdrom_disc_t cdnone_open_device( const gchar *filename, FILE *f );

struct gdrom_image_class cdrom_device_class = { "None", NULL,
						cdnone_image_is_valid, cdnone_open_device };

GList *gdrom_get_native_devices(void)
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
