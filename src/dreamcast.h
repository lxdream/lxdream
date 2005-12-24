/**
 * $Id: dreamcast.h,v 1.7 2005-12-24 08:02:14 nkeynes Exp $
 *
 * Public interface for dreamcast.c -
 * Central switchboard for the system. This pulls all the individual modules
 * together into some kind of coherent structure. This is also where you'd
 * add Naomi support, if I ever get a board to play with...
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

#ifndef dreamcast_H
#define dreamcast_H 1

#include <stdio.h>
#include <glib/gtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TIMESLICE_LENGTH 1000 /* microseconds */

void dreamcast_init(void);
void dreamcast_reset(void);
void dreamcast_run(void);
void dreamcast_stop(void);

int dreamcast_save_state( const gchar *filename );
int dreamcast_load_state( const gchar *filename );

#ifdef __cplusplus
}
#endif

#endif /* !dreamcast_H */
