/**
 * $Id: dream.h,v 1.7 2005-12-26 03:54:52 nkeynes Exp $
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

#ifndef dream_H
#define dream_H 1

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <glib/gtypes.h>

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
     * memory required, etc). Only called once during DreamOn startup
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
extern struct dreamcast_module unknown_module;

/*************************** Logging **************************/

#define EMIT_FATAL 0
#define EMIT_ERR 1
#define EMIT_WARN 2
#define EMIT_INFO 3
#define EMIT_DEBUG 4
#define EMIT_TRACE 5

#ifndef MODULE
#define MODULE unknown_module
#endif

void emit( void *, int level, const char *source, const char *msg, ... );

#define FATAL( ... ) emit( NULL, EMIT_FATAL, MODULE.name, __VA_ARGS__ )
#define ERROR( ... ) emit( NULL, EMIT_ERR, MODULE.name, __VA_ARGS__ )
#define WARN( ... ) emit( NULL, EMIT_WARN, MODULE.name, __VA_ARGS__ )
#define INFO( ... ) emit( NULL, EMIT_INFO, MODULE.name, __VA_ARGS__ )
#define DEBUG( ... ) emit( NULL, EMIT_DEBUG, MODULE.name, __VA_ARGS__ )
#define TRACE( ... ) emit( NULL, EMIT_TRACE, MODULE.name, __VA_ARGS__ )

#define BIOS_PATH "../bios"

void fwrite_string( char *s, FILE *f );
int fread_string( char *s, int maxlen, FILE *f );

#ifdef __cplusplus
}
#endif
#endif
