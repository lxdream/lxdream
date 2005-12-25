/**
 * $Id: dream.h,v 1.6 2005-12-25 08:24:07 nkeynes Exp $
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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <glib/gtypes.h>

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

#define EMIT_FATAL 0
#define EMIT_ERR 1
#define EMIT_WARN 2
#define EMIT_INFO 3
#define EMIT_DEBUG 4
#define EMIT_TRACE 5

#ifndef MODULE_ID
#define MODULE_ID 0
#endif

void emit( void *, int level, int source, const char *msg, ... );

#define FATAL( ... ) emit( NULL, EMIT_FATAL, MODULE_ID, __VA_ARGS__ )
#define ERROR( ... ) emit( NULL, EMIT_ERR, MODULE_ID, __VA_ARGS__ )
#define WARN( ... ) emit( NULL, EMIT_WARN, MODULE_ID, __VA_ARGS__ )
#define INFO( ... ) emit( NULL, EMIT_INFO, MODULE_ID, __VA_ARGS__ )
#define DEBUG( ... ) emit( NULL, EMIT_DEBUG, MODULE_ID, __VA_ARGS__ )
#define TRACE( ... ) emit( NULL, EMIT_TRACE, MODULE_ID, __VA_ARGS__ )

#define BIOS_PATH "../bios"

#ifdef __cplusplus
}
#endif
#endif
