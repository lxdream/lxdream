/**
 * $Id: gl_common.h,v 1.4 2007-10-13 04:01:02 nkeynes Exp $
 *
 * Parent for all X11 display drivers.
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

#ifndef video_gl_common_H
#define video_gl_common_H

#include "display.h"

/**
 * Test if a specific extension is supported. From opengl.org
 * @param extension extension name to check for
 * @return TRUE if supported, otherwise FALSE.
 */
gboolean isGLExtensionSupported( const char *extension );

gboolean hasRequiredGLExtensions();

/**
 * Generic GL routine to draw the given frame buffer in the display view.
 */
gboolean gl_display_frame_buffer( frame_buffer_t frame );

/**
 * Generic GL routine to blank the display view with the specified colour.
 */
gboolean gl_display_blank( uint32_t colour );

/**
 * Copy the frame buffer contents to the specified texture id
 */
void gl_frame_buffer_to_tex_rectangle( frame_buffer_t frame, GLuint texid );

/**
 * Write a rectangular texture (GL_TEXTURE_RECTANGLE_ARB) to the display frame
 */
void gl_display_tex_rectangle( GLuint texid, uint32_t texwidth, uint32_t texheight, gboolean invert );

/**
 * Redisplay the last frame.
 */
void gl_redisplay_last();

/**
 * Generic GL read_render_buffer. This function assumes that the caller
 * has already set the appropriate glReadBuffer(); in other words, unless
 * there's only one buffer this needs to be wrapped.
 */
gboolean gl_read_render_buffer( render_buffer_t buffer, unsigned char *target );


/****** FBO handling (gl_fbo.c) ******/
gboolean gl_fbo_is_supported();
void gl_fbo_shutdown();
void gl_fbo_init( display_driver_t driver );

/****** Shader handling (gl_sl.c) *****/
gboolean glsl_is_supported(void);
gboolean glsl_load_shaders( const char *vert_shader, const char *frag_shader );
void glsl_unload_shaders(void);

extern const char *glsl_vertex_shader_src;
extern const char *glsl_fragment_shader_src;

#endif /* !video_gl_common_H */
