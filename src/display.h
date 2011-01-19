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

#ifndef lxdream_display_H
#define lxdream_display_H 1

#define GL_GLEXT_PROTOTYPES 1

#include <stdint.h>
#include <stdio.h>
#include <glib.h>
#include "lxdream.h"
#include "gettext.h"
#include "config.h"
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

/**
 * The standard display size (for the purposes of mouse inputs, etc, is 640x480 -
 * events should be adjusted accordingly if this is not the actual window size.
 */ 
#define DISPLAY_WIDTH 640
#define DISPLAY_HEIGHT 480
    
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
    uint32_t colour_format;
    sh4addr_t address; /* Address buffer was rendered to, or -1 for unrendered */
    uint32_t size; /* Size of buffer in bytes, must be width*height*bpp */
    gboolean inverted;/* True if the buffer is upside down */
    uint32_t scale;
    GLuint tex_id;    /* texture bound to render buffer, if any (0 = none). 
                       * The render buffer does not own the texture */
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
    uint32_t colour_format;
    sh4addr_t address;
    uint32_t size; /* Size of buffer in bytes, must be width*height*bpp */
    gboolean inverted;/* True if the buffer is upside down */
    unsigned char *data;
};

struct display_capabilities {
    gboolean has_gl;
    int stencil_bits; /* 0 = no stencil buffer */
};

struct vertex_buffer {
    /**
     * Map the buffer into the host address space (if necessary) in preparation
     * for filling the buffer. This also implies a fence operation.
     * @param buf previously allocated buffer
     * @param size number of bytes of the buffer actually to be used. The buffer
     * will be resized if necessary.
     * @return a pointer to the start of the buffer.
     */
    void *(*map)(vertex_buffer_t buf, uint32_t size);

    /**
     * Unmap the buffer, after the vertex data is written.
     * @return the buffer base to use for gl*Pointer calls
     */
    void *(*unmap)(vertex_buffer_t buf);

    /**
     * Mark the buffer as finished, indicating that the vertex data is no
     * longer required (ie rendering is complete).
     */
    void (*finished)(vertex_buffer_t buf);

    /**
     * Delete the buffer and all associated resources.
     */
    void (*destroy)(vertex_buffer_t buf);

    /* Private data */
    void *data;
    GLuint id;
    uint32_t capacity;
    uint32_t mapped_size;
    GLuint fence;
};


/**
 * Core video driver - exports function to setup a GL context, as well as handle
 * keyboard input and display resultant output.
 */
typedef struct display_driver {
    char *name;
    /**
     * Short (<60 chars) description of the driver. This should be marked for
     * localization.
     */
    char *description;
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
     * Given a device-specific event code, return the corresponding keysym.
     * The string should be newly allocated (caller will free)
     */
    gchar *(*get_keysym_for_keycode)( uint16_t keycode );

    /**
     * Create a render target with the given width and height. If a texture ID
     * is supplied (non-zero), the render buffer writes its output to that texture.
     * @param tex_id 0 or a valid GL texture name which must have been initialized to
     * the correct dimensions. 
     */
    render_buffer_t (*create_render_buffer)( uint32_t width, uint32_t height, GLuint tex_id );

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
     * Complete rendering and clear the current rendering target. If the 
     * buffer has a bound texture, it will be updated if necessary.
     */
    void (*finish_render)( render_buffer_t buffer );
    
    /**
     * Load the supplied frame buffer into the given render buffer.
     * Included here to allow driver-specific optimizations.
     */
    void (*load_frame_buffer)( frame_buffer_t frame, render_buffer_t render );

    /**
     * Display a single frame using a previously rendered GL buffer.
     */
    void (*display_render_buffer)( render_buffer_t buffer );

    /**
     * Display a single blanked frame using a fixed colour for the
     * entire frame (specified in BGR888 format). 
     */
    void (*display_blank)( uint32_t rgb );

    /**
     * Swap front/back window buffers
     */
    void (*swap_buffers)();

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

    /**
     * Create a new vertex buffer
     */
    vertex_buffer_t (*create_vertex_buffer)( );

    /**
     * Dump driver-specific information about the implementation to the given stream
     */
    void (*print_info)( FILE *out );

    struct display_capabilities capabilities;

} *display_driver_t;

/**
 * Print the configured video drivers to the output stream, one to a line.
 */
void print_display_drivers( FILE *out );
display_driver_t get_display_driver_by_name( const char *name );
gboolean display_set_driver( display_driver_t driver );

extern uint32_t pvr2_frame_counter;

extern display_driver_t display_driver;

extern struct display_driver display_gtk_driver;
extern struct display_driver display_osx_driver;
extern struct display_driver display_null_driver;

/****************** Input methods **********************/

#define MAX_MOUSE_BUTTONS 32

/* Pressure is 0..127  (allowing a joystick to be defined as two half-axes of 7- bits each) */
#define MAX_PRESSURE 0x7F

typedef key_binding_t input_key_callback_t;

/**
 * Callback to receive mouse input events
 * @param data pointer passed in at the time the hook was registered
 * @param buttons bitmask of button states, where bit 0 is button 0 (left), bit 1 is button
 * 1 (middle), bit 2 is button 2 (right) and so forth.
 * @param x Horizontal movement since the last invocation (in relative mode) or window position 
 * (in absolute mode).
 * @param y Vertical movement since the last invocation (in relative mode) or window position
 * (in absolute mode).
 * @param absolute If TRUE, x and y are the current window coordinates 
 *  of the mouse cursor. Otherwise, x and y are deltas from the previous mouse position.
 */
typedef void (*input_mouse_callback_t)( void *data, uint32_t buttons, int32_t x, int32_t y, gboolean absolute );

gboolean input_register_key( const gchar *keysym, input_key_callback_t callback,
                             void *data, uint32_t value );

void input_unregister_key( const gchar *keysym, input_key_callback_t callback,
                           void *data, uint32_t value );

gboolean input_register_keygroup( lxdream_config_group_t group );
void input_unregister_keygroup( lxdream_config_group_t group );
gboolean input_keygroup_changed( void *data, lxdream_config_group_t group, unsigned key,
                             const gchar *oldval, const gchar *newval );

/**
 * Register a hook to receive all keyboard input events
 */
gboolean input_register_keyboard_hook( input_key_callback_t callback, void *data );
void input_unregister_keyboard_hook( input_key_callback_t callback, void *data );

/**
 * Register a mouse event hook.
 * @param relative TRUE if the caller wants relative mouse movement, FALSE for
 * absolute mouse positioning. It's not generally possible to receive both at the same time.
 */
gboolean input_register_mouse_hook( gboolean relative, input_mouse_callback_t callback, void *data );
void input_unregister_mouse_hook( input_mouse_callback_t callback, void *data );

gboolean input_is_key_valid( const gchar *keysym );

gboolean input_is_key_registered( const gchar *keysym );

uint16_t input_keycode_to_dckeysym( uint16_t keycode );

/********************** Display/Input methods ***********************/

/**
 * Auxilliary input driver - provides input separate to and in addition to the
 * core UI/display. (primarily used for joystick devices)
 */
typedef struct input_driver {
    const char *id; /* Short identifier to display in the UI for the device (eg "JS0" ) */

    /**
     * Given a particular keysym, return the keycode associated with it.
     * @param keysym The keysym to be resolved, ie "Tab"
     * @return the display-specific keycode, or 0 if the keysym cannot
     * be resolved.
     */
    uint16_t (*resolve_keysym)( struct input_driver *driver, const gchar *keysym );

    /**
     * Given a device-specific event code, convert it to a dreamcast keyboard code.
     * This is only required for actual keyboard devices, other devices should just
     * leave this method NULL.
     */
    uint16_t (*convert_to_dckeysym)( struct input_driver *driver, uint16_t keycode );

    /**
     * Given a device-specific event code, return the corresponding keysym.
     * The string should be newly allocated (caller will free)
     */
    gchar *(*get_keysym_for_keycode)( struct input_driver *driver, uint16_t keycode );

    /**
     * Destroy the input driver.
     */
    void (*destroy)( struct input_driver *driver );

} *input_driver_t;       

extern struct input_driver system_mouse_driver;

/**
 * Register a new input driver (which must have a unique name)
 * @param driver the driver to register
 * @param max_keycode the highest possible keycode reported by the device
 * @return TRUE on success, FALSE on failure (eg driver already registed).
 */
gboolean input_register_device( input_driver_t driver, uint16_t max_keycode );

/**
 * Determine if the system has an input driver with the given unique ID.
 * @param id driver id to check
 * @return TRUE if the device exists, otherwise FALSE
 */
gboolean input_has_device( const gchar *id );

/**
 * Unregister an input driver.
 * @param driver the driver to unregister
 * If the driver is not in fact registered, this function has no effect.
 */
void input_unregister_device( input_driver_t driver );

/**
 * Called from the UI to indicate that the emulation window is focused (ie
 * able to receive input). This method is used to gate non-UI input devices -
 * when the display is not focused, all input events will be silently ignored.
 */
void display_set_focused( gboolean has_focus );

/**
 * Fire a keydown event on the specified device
 * @param input The input device source generating the event, or NULL for the 
 *        default GUI device
 * @param keycode The device-specific keycode
 * @param pressure The pressure of the key (0 to 127), where 0 is unpressed and
 *        127 is maximum pressure. Devices without pressure sensitivity should 
 *        always use MAX_PRESSURE (127) 
 */
void input_event_keydown( input_driver_t input, uint16_t keycode, uint32_t pressure );

void input_event_keyup( input_driver_t input, uint16_t keycode );

/**
 * Receive an input mouse down event. Normally these should be absolute events when
 * the mouse is not grabbed, and relative when it is.
 * @param button the button pressed, where 0 == first button
 * @param x_axis The relative or absolute position of the mouse cursor on the X axis
 * @param y_axis The relative or absolute position of the mouse cursor on the Y axis
 * @param absolute If TRUE, x_axis and y_axis are the current window coordinates 
 *  of the mouse cursor. Otherwise, x_axis and y_axis are deltas from the previous mouse position.
 */
void input_event_mousedown( uint16_t button, int32_t x_axis, int32_t y_axis, gboolean absolute );

/**
 * Receive an input mouse up event. Normally these should be absolute events when
 * the mouse is not grabbed, and relative when it is.
 * @param button the button released, where 0 == first button
 * @param x_axis The relative or absolute position of the mouse cursor on the X axis
 * @param y_axis The relative or absolute position of the mouse cursor on the Y axis
 * @param absolute If TRUE, x_axis and y_axis are the current window coordinates 
 *  of the mouse cursor. Otherwise, x_axis and y_axis are deltas from the previous mouse position.
 */
void input_event_mouseup( uint16_t button, int32_t x_axis, int32_t y_axis, gboolean absolute );

/**
 * Receive an input mouse motion event. Normally these should be absolute events when
 * the mouse is not grabbed, and relative when it is.
 * @param x_axis The relative or absolute position of the mouse cursor on the X axis
 * @param y_axis The relative or absolute position of the mouse cursor on the Y axis
 * @param absolute If TRUE, x_axis and y_axis are the current window coordinates 
 *  of the mouse cursor. Otherwise, x_axis and y_axis are deltas from the previous mouse position.
 */
void input_event_mousemove( int32_t x_axis, int32_t y_axis, gboolean absolute );

/**
 * Given a keycode and the originating input driver, return the corresponding 
 * keysym. The caller is responsible for freeing the string.
 * @return a newly allocated string, or NULL of the keycode is unresolvable.
 */
gchar *input_keycode_to_keysym( input_driver_t driver, uint16_t keycode );

typedef void (*display_keysym_callback_t)( void *data, const gchar *keysym );

/**
 * Set the keysym hook function (normally used by the UI to receive non-UI
 * input events during configuration.
 */
void input_set_keysym_hook( display_keysym_callback_t hook, void *data );



#ifdef __cplusplus
}
#endif
#endif /* !lxdream_display_H */
