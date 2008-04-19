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

#include "display.h"

/**
 * Test if a specific extension is supported. From opengl.org
 * @param extension extension name to check for
 * @return TRUE if supported, otherwise FALSE.
 */
gboolean isGLExtensionSupported( const char *extension );

/**
 * Test if secondary color (GL_COLOR_SUM) is supported.
 */
gboolean isGLSecondaryColorSupported();

gboolean isGLVertexBufferSupported();
gboolean isGLPixelBufferSupported();
gboolean isGLMirroredTextureSupported();

/****** Shader handling (gl_sl.c) *****/
gboolean glsl_is_supported(void);
gboolean glsl_load_shaders( const char *vert_shader, const char *frag_shader );
void glsl_unload_shaders(void);

extern const char *glsl_vertex_shader_src;
extern const char *glsl_fragment_shader_src;

#endif /* !lxdream_glutil_H */
