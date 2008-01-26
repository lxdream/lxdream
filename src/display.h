/**
 * $Id$
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
#include "lxdream.h"
#ifdef APPLE_BUILD
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Supported colour formats. Note that BGRA4444 is only ever used for texture
 * rendering (it's not valid for display purposes).
 */
#define COLFMT_BGRA1555  0
#define COLFMT_RGB565    1
#define COLFMT_BGRA4444  2
#define COLFMT_YUV422    3 /* 8-bit YUV (texture source only) */
#define COLFMT_BGR888    4 /* 24-bit BGR */
#define COLFMT_BGRA8888  5
#define COLFMT_INDEX4    6 /* 4 bit indexed colour (texture source only) */
#define COLFMT_INDEX8    7 /* 8-bit indexed colour (texture source only) */
#define COLFMT_BGR0888   8 /* 32-bit BGR */
#define COLFMT_RGB888    9 /* 24-bit RGB (ie GL native) */

struct colour_format {
    GLint type, format, int_format;
    int bpp;
};
extern struct colour_format colour_formats[];

extern int colour_format_bytes[];

/**
 * Structure to hold pixel data held in GL buffers.
 */
struct render_buffer {
    uint32_t width;
    uint32_t height;
    uint32_t rowstride;
    int colour_format;
    sh4addr_t address; /* Address buffer was rendered to, or -1 for unrendered */
    uint32_t size; /* Size of buffer in bytes, must be width*height*bpp */
    gboolean inverted;/* True if the buffer is upside down */
    int scale;
    unsigned int buf_id; /* driver-specific buffer id, if applicable */
    gboolean flushed; /* True if the buffer has been flushed to vram */
};

/**
 * Structure to hold pixel data stored in pvr2 vram, as opposed to data in
 * GL buffers.
 */
struct frame_buffer {
    uint32_t width;
    uint32_t height;
    uint32_t rowstride;
    int colour_format;
    sh4addr_t address;
    uint32_t size; /* Size of buffer in bytes, must be width*height*bpp */
    gboolean inverted;/* True if the buffer is upside down */
    unsigned char *data;
};

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
     * Given a native system keycode, convert it to a dreamcast keyboard code.
     */
    uint16_t (*convert_to_dckeysym)( uint16_t keycode );

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
     * Load the supplied frame buffer into the given render buffer.
     * Included here to allow driver-specific optimizations.
     */
    void (*load_frame_buffer)( frame_buffer_t frame, render_buffer_t render );

    /**
     * Display a single frame using a previously rendered GL buffer.
     */
    gboolean (*display_render_buffer)( render_buffer_t buffer );

    /**
     * Display a single blanked frame using a fixed colour for the
     * entire frame (specified in BGR888 format). 
     */
    gboolean (*display_blank)( uint32_t rgb );

    /**
     * Copy the image data from the GL buffer to the target memory buffer,
     * using the format etc from the buffer. This may force a glFinish()
     * but does not invalidate the buffer.
     * @param target buffer to fill with image data, which must be large enough
     *  to accomodate the image.
     * @param buffer Render buffer to read from.
     * @param rowstride rowstride of the target data
     * @param format colour format to output the data in.
     */
    gboolean (*read_render_buffer)( unsigned char *target, render_buffer_t buffer,
				    int rowstride, int format );

} *display_driver_t;

display_driver_t get_display_driver_by_name( const char *name );
gboolean display_set_driver( display_driver_t driver );

extern uint32_t pvr2_frame_counter;

extern display_driver_t display_driver;

extern struct display_driver display_agl_driver;
extern struct display_driver display_gtk_driver;
extern struct display_driver display_null_driver;

/****************** Input methods **********************/

typedef void (*input_key_callback_t)( void *data, uint32_t value, gboolean isKeyDown );

/**
 * Callback to receive mouse input events
 * @param data pointer passed in at the time the hook was registered
 * @param buttons bitmask of button states, where bit 0 is button 0 (left), bit 1 is button
 * 1 (middle), bit 2 is button 2 (right) and so forth.
 * @param x Horizontal movement since the last invocation (in relative mode) or window position 
 * (in absolute mode).
 * @param y Vertical movement since the last invocation (in relative mode) or window position
 * (in absolute mode).
 */
typedef void (*input_mouse_callback_t)( void *data, uint32_t buttons, int32_t x, int32_t y );

gboolean input_register_key( const gchar *keysym, input_key_callback_t callback,
			     void *data, uint32_t value );

void input_unregister_key( const gchar *keysym, input_key_callback_t callback,
			   void *data, uint32_t value );

/**
 * Register a hook to receive all input events
 */
gboolean input_register_hook( input_key_callback_t callback, void *data );
void input_unregister_hook( input_key_callback_t callback, void *data );

/**
 * Register a mouse event hook.
 * @param relative TRUE if the caller wants relative mouse movement, FALSE for
 * absolute mouse positioning. It's not generally possible to receive both at the same time.
 */
gboolean input_register_mouse_hook( gboolean relative, input_mouse_callback_t callback, void *data );
void input_unregister_mouse_hook( input_mouse_callback_t callback, void *data );

gboolean input_is_key_valid( const gchar *keysym );

gboolean input_is_key_registered( const gchar *keysym );

void input_event_keydown( uint16_t keycode );

void input_event_keyup( uint16_t keycode );

void input_event_mouse( uint32_t buttons, int32_t x_axis, int32_t y_axis );

uint16_t input_keycode_to_dckeysym( uint16_t keycode );

#ifdef __cplusplus
}
#endif
#endif
