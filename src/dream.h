/*
 * Application-wide declarations
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
