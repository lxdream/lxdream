/**
 * $Id$
 *
 * GL-based support functions 
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

#ifndef lxdream_glutil_H
#define lxdream_glutil_H

#include <stdio.h>
#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Test if a specific extension is supported. From opengl.org
 * @param extension extension name to check for
 * @return TRUE if supported, otherwise FALSE.
 */
gboolean isGLExtensionSupported( const char *extension );

/**
 * Dump GL information to the output stream, usually for debugging purposes
 */
void glPrintInfo( FILE *out );

/**
 * Check for a GL error and print a message if there is one
 * @param context If not null, a string to be printed along side an error message
 * @return FALSE if there was an error, otherwise TRUE
 */
gboolean gl_check_error( const char *context );
/**
 * Test if secondary color (GL_COLOR_SUM) is supported.
 */
gboolean isGLSecondaryColorSupported();

gboolean isGLVertexBufferSupported();
gboolean isGLVertexRangeSupported();
gboolean isGLPixelBufferSupported();
gboolean isGLMultitextureSupported();
gboolean isGLMirroredTextureSupported();

/****** Extension variant wrangling *****/




/****** Shader handling (gl_sl.c) *****/
gboolean glsl_is_supported(void);
const char *glsl_get_version(void);
gboolean glsl_load_shaders( );
void glsl_unload_shaders(void);
void glsl_clear_shader();

/* Convenience formatting function for driver use */
void fprint_extensions( FILE *out, const char *extensions );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_glutil_H */
