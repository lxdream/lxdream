/**
 * $Id$
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

#ifndef lxdream_dreamcast_H
#define lxdream_dreamcast_H 1

#include <stdio.h>
#include "lxdream.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TIMESLICE_LENGTH 1000000 /* nanoseconds */

#define XLAT_NEW_CACHE_SIZE 40 MB
#define XLAT_TEMP_CACHE_SIZE 2 MB
#define XLAT_OLD_CACHE_SIZE 8 MB

void dreamcast_configure(void);
void dreamcast_configure_aica_only(void);
void dreamcast_init(void);
void dreamcast_reset(void);
void dreamcast_run(void);
void dreamcast_set_run_time( unsigned int seconds, unsigned int nanosecs );
void dreamcast_set_exit_on_stop( gboolean flag );
void dreamcast_stop(void);
void dreamcast_shutdown(void);
void dreamcast_config_changed(void);
gboolean dreamcast_is_running(void);

/**
 * Return if it's possible to start the VM - currently this requires 
 * a) A configured system
 * b) Some code to run (either a user program or a ROM)
 */
gboolean dreamcast_can_run(void);

/**
 * Notify the VM that a program (ELF or other binary) has been loaded.
 * 
 */
void dreamcast_program_loaded( const gchar *name, sh4addr_t entry_point );

#define DREAMCAST_SAVE_MAGIC "%!-lxDream!Save\0"
#define DREAMCAST_SAVE_VERSION 0x00010006

int dreamcast_save_state( const gchar *filename );
int dreamcast_load_state( const gchar *filename );

/**
 * Load the front-buffer image from the specified file.
 * If the file is not a valid save state, returns NULL. Otherwise,
 * returns a newly allocated frame_buffer that should be freed
 * by the caller. (The data buffer is contained within the
 * allocation and does not need to be freed separately)
 */
frame_buffer_t dreamcast_load_preview( const gchar *filename );

#define SCENE_SAVE_MAGIC "%!-lxDream!Scene"
#define SCENE_SAVE_VERSION 0x00010000

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_dreamcast_H */
