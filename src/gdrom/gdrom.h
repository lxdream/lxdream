/**
 * $Id: gdrom.h,v 1.1 2006-03-22 14:29:02 nkeynes Exp $
 *
 * This file defines the structures and functions used by the GD-Rom
 * disc driver. (ie, the modules that supply a CD image to be used by the
 * system).
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

#ifndef dream_gdrom_H
#define dream_gdrom_H 1

#include "dream.h"

typedef struct gdrom_toc {
    uint32_t tracks[99];
    uint32_t first, last, leadout;
} *gdrom_toc_t;


typedef struct gdrom_disc {
    
    gboolean (*read_toc)( gdrom_toc_t toc );

    gboolean (*read_data_sectors)( uint32_t lba, uint32_t sector_count,
				   char *buf );
} *gdrom_disc_t;

void gdrom_mount( gdrom_disc_t disc );

void gdrom_unmount( void );

#endif
