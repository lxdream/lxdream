/**
 * $Id: lxdream.h,v 1.4 2007-11-10 04:44:51 nkeynes Exp $
 *
 * Common type definitions and forward declarations
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

#ifndef lxdream_common_H
#define lxdream_common_H 1

#include <stdint.h>
#include <glib/gtypes.h>

#include "../config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_NAME "lxDream"
#define APP_VERSION "0.8.1"

#define MB *1024*1024
#define KB *1024

#ifndef max
#define max(a,b) ( (a) > (b) ? (a) : (b) )
#endif

/**
 * A 32-bit address in SH4 space
 */
typedef uint32_t sh4addr_t;
/**
 * A direct pointer into SH4 memory
 */
typedef unsigned char *sh4ptr_t;

/******************* Forward type declarations ******************/

typedef struct render_buffer *render_buffer_t;
typedef struct frame_buffer *frame_buffer_t;

/*************************** Logging ****************************/

#define EMIT_FATAL 0
#define EMIT_ERR 1
#define EMIT_WARN 2
#define EMIT_INFO 3
#define EMIT_DEBUG 4
#define EMIT_TRACE 5

#ifdef MODULE
#define MODULE_NAME MODULE.name
#else
#define MODULE_NAME "*****"
#endif

void log_message( void *, int level, const char *source, const char *msg, ... );

#define FATAL( ... ) log_message( NULL, EMIT_FATAL, MODULE_NAME, __VA_ARGS__ )
#define ERROR( ... ) log_message( NULL, EMIT_ERR, MODULE_NAME, __VA_ARGS__ )
#define WARN( ... ) log_message( NULL, EMIT_WARN, MODULE_NAME, __VA_ARGS__ )
#define INFO( ... ) log_message( NULL, EMIT_INFO, MODULE_NAME, __VA_ARGS__ )
#define DEBUG( ... ) log_message( NULL, EMIT_DEBUG, MODULE_NAME, __VA_ARGS__ )
#define TRACE( ... ) log_message( NULL, EMIT_TRACE, MODULE_NAME, __VA_ARGS__ )



#ifdef __cplusplus
}
#endif

#endif /* !lxdream_common_H */
