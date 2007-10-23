/**
 * $Id: dreamcast.h,v 1.19 2007-10-23 10:48:24 nkeynes Exp $
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

#define DEFAULT_TIMESLICE_LENGTH 1000000 /* nanoseconds */

#define MB *1024*1024
#define KB *1024

#define XLAT_NEW_CACHE_SIZE 32 MB
#define XLAT_TEMP_CACHE_SIZE 2 MB
#define XLAT_OLD_CACHE_SIZE 8 MB

void dreamcast_configure(void);
void dreamcast_configure_aica_only(void);
void dreamcast_init(void);
void dreamcast_reset(void);
void dreamcast_run(void);
void dreamcast_run_for( unsigned int seconds, unsigned int nanosecs );
void dreamcast_stop(void);
void dreamcast_shutdown(void);
void dreamcast_config_changed(void);
gboolean dreamcast_is_running(void);

#define DREAMCAST_SAVE_MAGIC "%!-lxDream!Save\0"
#define DREAMCAST_SAVE_VERSION 0x00010000

int dreamcast_save_state( const gchar *filename );
int dreamcast_load_state( const gchar *filename );

#define SCENE_SAVE_MAGIC "%!-lxDream!Scene"
#define SCENE_SAVE_VERSION 0x00010000

#ifdef __cplusplus
}
#endif

#endif /* !dreamcast_H */
