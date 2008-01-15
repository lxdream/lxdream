/**
 * $Id$
 *
 * CDI CD-image file support
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

#ifndef cdi_H
#define cdi_H 1

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

#include <stdio.h>

typedef struct cdi_handle *cdi_t;

cdi_t cdi_open( char *filename );

#ifdef __cplusplus
}
#endif

#endif
