/**
 * $Id: video.h,v 1.5 2006-03-13 12:39:03 nkeynes Exp $
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Supported colour formats. Note that ARGB4444 is only ever used for texture
 * rendering (it's not valid for display purposes).
 */
#define COLFMT_RGB565    1
#define COLFMT_RGB888    4
#define COLFMT_ARGB1555  0
#define COLFMT_ARGB8888  5
#define COLFMT_ARGB4444  2
#define COLFMT_YUV422    3 /* 8-bit YUV (texture source only) */
#define COLFMT_INDEX4    6 /* 4 bit indexed colour (texture source only) */
#define COLFMT_INDEX8    7 /* 8-bit indexed colour (texture source only) */

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
typedef struct video_driver {
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
} *video_driver_t;

void video_open( void );
void video_update_frame( void );
void video_update_size( int, int, int );

extern uint32_t pvr2_frame_counter;

extern struct video_driver video_gtk_driver;

#ifdef __cplusplus
}
#endif
#endif
