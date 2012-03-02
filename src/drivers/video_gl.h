/**
 * $Id$
 *
 * Common GL code that doesn't depend on a specific implementation
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

#ifndef lxdream_video_gl_H
#define lxdream_video_gl_H 1

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t video_width, video_height;

/**
 * Set the video size used by the frame blits
 */
void gl_set_video_size( uint32_t width, uint32_t height );

/**
 * Generic GL routine to draw the given frame buffer into a texture
 */
gboolean gl_load_frame_buffer( frame_buffer_t frame, int tex_id );

/**
 * Reset the GL state to its initial values
 */
void gl_reset_state();

/**
 * Generic GL routine to blank the display view with the specified colour.
 */
void gl_display_blank( uint32_t colour );

/**
 * Write a rectangular texture (GL_TEXTURE_2D) to the display frame
 */
void gl_display_render_buffer( render_buffer_t buffer );

/**
 * Write a rectangular texture (GL_TEXTURE_2D) to the display frame
 */
void gl_texture_window( int width, int height, int tex_id, gboolean inverted );

/**
 * Convert window coordinates to dreamcast device coords (640x480) using the 
 * same viewable area as gl_texture_window.
 * If the coordinates are outside the viewable area, the result is -1,-1.
 */ 
void gl_window_to_system_coords( int *x, int *y );

/**
 * Generic GL read_render_buffer. This function assumes that the caller
 * has already set the appropriate glReadBuffer(); in other words, unless
 * there's only one buffer this needs to be wrapped.
 */
gboolean gl_read_render_buffer( unsigned char *target, render_buffer_t buffer, 
                                int rowstride, int colour_format );


void gl_framebuffer_setup(void);
void gl_framebuffer_cleanup(void);
/****** FBO handling (gl_fbo.c) ******/
gboolean gl_fbo_is_supported();
void gl_fbo_shutdown();
void gl_fbo_init( display_driver_t driver );
void gl_fbo_detach();

/** VBO/VAR handling (gl_vbo.c) ******/
void gl_vbo_init( display_driver_t driver );

/* Set the fallback vertex buffer handling, for drivers with no GL */
void gl_vbo_fallback_init( display_driver_t driver );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_video_gl_H */
