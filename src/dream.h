/*
 * Application-wide declarations
 */
#ifndef dream_H
#define dream_H 1

#include <stdint.h>

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

void emit( int level, int source, char *msg, ... );

#define FATAL( ... ) emit( EMIT_FATAL, MODULE_ID, __VA_ARGS__ )
#define ERROR( ... ) emit( EMIT_ERR, MODULE_ID, __VA_ARGS__ )
#define WARN( ... ) emit( EMIT_WARN, MODULE_ID, __VA_ARGS__ )
#define INFO( ... ) emit( EMIT_INFO, MODULE_ID, __VA_ARGS__ )
#define DEBUG( ... ) emit( EMIT_DEBUG, MODULE_ID, __VA_ARGS__ )
#define TRACE( ... ) emit( EMIT_TRACE, MODULE_ID, __VA_ARGS__ )

#define BIOS_PATH "../bios"

#ifdef __cplusplus
}
#endif
#endif
