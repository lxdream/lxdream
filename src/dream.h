/**
 * $Id$
 *
 * Miscellaneous application-wide declarations (mainly logging atm)
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

#ifndef lxdream_dream_H
#define lxdream_dream_H 1

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "lxdream.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************ Modules ********************************/
/**
 * Basic module structure defining the common operations across all
 * modules, ie start, stop, reset, etc. 
 */
typedef struct dreamcast_module {
    char *name;
    /**
     * Perform all initial module setup (ie register / allocate any
     * memory required, etc). Only called once during system startup
     */
    void (*init)();
    /**
     * Reset the module into it's initial system boot state. Will be called
     * once after init(), as well as whenever the user requests a reset.
     */
    void (*reset)();
    /**
     * Set the module into a running state (may be NULL)
     */
    void (*start)();
    /**
     * Execute one time-slice worth of operations, for the given number of
     * nanoseconds.
     * @return Number of nanoseconds actually executed
     */
    uint32_t (*run_time_slice)( uint32_t nanosecs );
    /**
     * Set the module into a stopped state (may be NULL)
     */
    void (*stop)();
    /**
     * Save the module state to the FILE stream. May be NULL, in which case
     * the module is considered to have no state.
     */
    void (*save)(FILE *);
    /**
     * Load the saved module state from the FILE stream. May be NULL, in which
     * case reset() will be called instead.
     * @return 0 on success, nonzero on failure.
     */
    int (*load)(FILE *);
} *dreamcast_module_t;

void dreamcast_register_module( dreamcast_module_t );

extern struct dreamcast_module mem_module;
extern struct dreamcast_module sh4_module;
extern struct dreamcast_module asic_module;
extern struct dreamcast_module pvr2_module;
extern struct dreamcast_module aica_module;
extern struct dreamcast_module ide_module;
extern struct dreamcast_module maple_module;
extern struct dreamcast_module pvr2_module;
extern struct dreamcast_module gui_module;
extern struct dreamcast_module eventq_module;
extern struct dreamcast_module unknown_module;

void fwrite_string( const char *s, FILE *f );
int fread_string( char *s, int maxlen, FILE *f );
void fwrite_gzip( void *p, size_t size, size_t num, FILE *f );
int fread_gzip( void *p, size_t size, size_t num, FILE *f );
void fwrite_dump( unsigned char *buf, unsigned int length, FILE *f );
void fwrite_dump32( unsigned int *buf, unsigned int length, FILE *f );
void fwrite_dump32v( unsigned int *buf, unsigned int length, int wordsPerLine, FILE *f );

void install_crash_handler(void);

gboolean write_png_to_stream( FILE *f, frame_buffer_t );
frame_buffer_t read_png_from_stream( FILE *f );

#ifdef __cplusplus
}
#endif
#endif /* !lxdream_dream_H */
