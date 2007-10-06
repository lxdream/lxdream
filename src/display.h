/**
 * $Id: display.h,v 1.7 2007-10-06 08:59:42 nkeynes Exp $
 *
 * The PC side of the video support (responsible for actually displaying / 
 * rendering frames)
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

#ifndef dream_video_H
#define dream_video_H

#include <stdint.h>
#include <glib.h>
#include <GL/gl.h>
#include "dream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Supported colour formats. Note that ARGB4444 is only ever used for texture
 * rendering (it's not valid for display purposes).
 */
#define COLFMT_ARGB1555  0
#define COLFMT_RGB565    1
#define COLFMT_ARGB4444  2
#define COLFMT_YUV422    3 /* 8-bit YUV (texture source only) */
#define COLFMT_RGB888    4 /* 24-bit RGB */
#define COLFMT_ARGB8888  5
#define COLFMT_INDEX4    6 /* 4 bit indexed colour (texture source only) */
#define COLFMT_INDEX8    7 /* 8-bit indexed colour (texture source only) */
#define COLFMT_RGB0888   8 /* 32-bit RGB */

struct colour_format {
    GLint type, format, int_format;
    int bpp;
};
extern struct colour_format colour_formats[];

extern int colour_format_bytes[];

/**
 * Structure to hold pixel data held in GL buffers.
 */
typedef struct render_buffer {
    uint32_t width;
    uint32_t height;
    uint32_t rowstride;
    int colour_format;
    sh4addr_t address; /* Address buffer was rendered to, or -1 for unrendered */
    uint32_t size; /* Size of buffer in bytes, must be width*height*bpp */
    int scale;
    int buf_id; /* driver-specific buffer id, if applicable */
    gboolean flushed; /* True if the buffer has been flushed to vram */
} *render_buffer_t;

/**
 * Structure to hold pixel data stored in pvr2 vram, as opposed to data in
 * GL buffers.
 */
typedef struct frame_buffer {
    uint32_t width;
    uint32_t height;
    uint32_t rowstride;
    int colour_format;
    sh4addr_t address;
    uint32_t size; /* Size of buffer in bytes, must be width*height*bpp */
    char *data;
} * frame_buffer_t;

/**
 * Core video driver - exports function to setup a GL context, as well as handle
 * keyboard input and display resultant output.
 */
typedef struct display_driver {
    char *name;
    /**
     * Initialize the driver. This is called only once at startup time, and
     * is guaranteed to be called before any other methods.
     * @return TRUE if the driver was successfully initialized, otherwise
     * FALSE.
     */
    gboolean (*init_driver)(void);

    /**
     * Cleanly shutdown the driver. Normally only called at system shutdown
     * time.
     */
    void (*shutdown_driver)(void);

    /**
     * Given a particular keysym, return the keycode associated with it.
     * @param keysym The keysym to be resolved, ie "Tab"
     * @return the display-specific keycode, or 0 if the keysym cannot
     * be resolved.
     */
    uint16_t (*resolve_keysym)( const gchar *keysym );

    /**
     * Create a render target with the given width and height.
     */
    render_buffer_t (*create_render_buffer)( uint32_t width, uint32_t height );

    /**
     * Destroy the specified render buffer and release any associated
     * resources.
     */
    void (*destroy_render_buffer)( render_buffer_t buffer );

    /**
     * Set the current rendering target to the specified buffer.
     */
    gboolean (*set_render_target)( render_buffer_t buffer );

    /**
     * Display a single frame using the supplied pixmap data.
     */
    gboolean (*display_frame_buffer)( frame_buffer_t buffer );

    /**
     * Display a single frame using a previously rendered GL buffer.
     */
    gboolean (*display_render_buffer)( render_buffer_t buffer );

    /**
     * Display a single blanked frame using a fixed colour for the
     * entire frame (specified in RGB888 format). 
     */
    gboolean (*display_blank)( uint32_t rgb );

    /**
     * Copy the image data from the GL buffer to the target memory buffer,
     * using the format etc from the buffer. This may force a glFinish()
     * but does not invalidate the buffer.
     */
    gboolean (*read_render_buffer)( render_buffer_t buffer, char *target );

} *display_driver_t;

gboolean display_set_driver( display_driver_t driver );

extern uint32_t pvr2_frame_counter;

extern display_driver_t display_driver;

extern struct display_driver display_gtk_driver;
extern struct display_driver display_null_driver;

/****************** Input methods **********************/

typedef void (*input_key_callback_t)( void *data, uint32_t value, gboolean isKeyDown );

gboolean input_register_key( const gchar *keysym, input_key_callback_t callback,
			     void *data, uint32_t value );

void input_unregister_key( const gchar *keysym );

gboolean input_is_key_valid( const gchar *keysym );

gboolean input_is_key_registered( const gchar *keysym );

void input_event_keydown( uint16_t keycode );

void input_event_keyup( uint16_t keycode );



#ifdef __cplusplus
}
#endif
#endif
