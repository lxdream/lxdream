/**
 * $Id: display.h,v 1.3 2007-01-25 11:46:35 nkeynes Exp $
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

typedef struct video_buffer {
    uint32_t hres;
    uint32_t vres;
    uint32_t rowstride;
    int colour_format;
    char *data;
} *video_buffer_t;

/**
 * Core video driver - expected to directly support an OpenGL context
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
     * Set the current display format to the specified values. This is
     * called immediately prior to any display frame call where the
     * parameters have changed from the previous frame
     */
    gboolean (*set_display_format)( uint32_t hres, uint32_t vres, 
				    int colour_fmt );

    /**
     * Set the current rendering format to the specified values. This is
     * called immediately prior to starting rendering of a frame where the
     * parameters have changed from the previous frame. Note that the driver
     * is not required to precisely support the requested colour format.
     *
     * This method is also responsible for setting up an appropriate GL
     * context for the main engine to render into.
     *
     * @param hres The horizontal resolution (ie 640)
     * @param vres The vertical resolution (ie 480)
     * @param colour_fmt The colour format of the buffer (ie COLFMT_ARGB4444)
     * @param texture Flag indicating that the frame being rendered is a
     * texture, rather than a display frame. 
     */
    gboolean (*set_render_format)( uint32_t hres, uint32_t vres,
				   int colour_fmt, gboolean texture );
    /**
     * Display a single frame using the supplied pixmap data. Is assumed to
     * invalidate the current GL front buffer (but not the back buffer).
     */
    gboolean (*display_frame)( video_buffer_t buffer );

    /**
     * Display a single blanked frame using a fixed colour for the
     * entire frame (specified in RGB888 format). Is assumed to invalidate
     * the current GL front buffer (but not the back buffer).
     */
    gboolean (*display_blank_frame)( uint32_t rgb );

    /**
     * Promote the current render back buffer to the front buffer
     */
    void (*display_back_buffer)( void );
} *display_driver_t;

void video_open( void );
void video_update_frame( void );
void video_update_size( int, int, int );

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
