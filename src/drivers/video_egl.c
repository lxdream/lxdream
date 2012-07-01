/**
 * $Id$
 *
 * Window management using EGL.
 *
 * Copyright (c) 2012 Nathan Keynes.
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

#include "lxdream.h"
#include "display.h"
#include "video_egl.h"
#include "video_gl.h"
#include "pvr2/pvr2.h"
#include "pvr2/glutil.h"

static const char *getEGLErrorString( EGLint code )
{
    switch( code ) {
    case EGL_SUCCESS: return "OK";
    case EGL_NOT_INITIALIZED: return "EGL not initialized";
    case EGL_BAD_ACCESS: return "Bad access";
    case EGL_BAD_ALLOC: return "Allocation failed";
    case EGL_BAD_ATTRIBUTE: return "Bad attribute";
    case EGL_BAD_CONTEXT: return "Bad context";
    case EGL_BAD_CONFIG: return "Bad config";
    case EGL_BAD_CURRENT_SURFACE: return "Bad current surface";
    case EGL_BAD_DISPLAY: return "Bad display";
    case EGL_BAD_MATCH: return "Bad match";
    case EGL_BAD_PARAMETER: return "Bad parameter";
    case EGL_BAD_NATIVE_PIXMAP: return "Bad native pixmap";
    case EGL_BAD_NATIVE_WINDOW: return "Bad native window";
    default: return "Unknown error";
    }
}


static void logEGLError(const char *msg)
{
    EGLint error = eglGetError();
    const char *errorStr = getEGLErrorString(error);

    ERROR( "%s: %s (%x)", msg, errorStr, error );
}

static const EGLint RGB888_attributes[] = {
        EGL_RED_SIZE,       8,
        EGL_GREEN_SIZE,     8,
        EGL_BLUE_SIZE,      8,
        EGL_DEPTH_SIZE,     16,
        EGL_STENCIL_SIZE,   8,
        EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
        EGL_NONE, EGL_NONE };

static const EGLint RGB565_attributes[] = {
        EGL_RED_SIZE,       5,
        EGL_GREEN_SIZE,     6,
        EGL_BLUE_SIZE,      5,
        EGL_DEPTH_SIZE,     16,
        EGL_STENCIL_SIZE,   8,
        EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
        EGL_NONE, EGL_NONE };

static const EGLint alt_attributes[] = {
        EGL_DEPTH_SIZE,     16,
        EGL_STENCIL_SIZE,   8,
        EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
        EGL_NONE, EGL_NONE };

static const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE, EGL_NONE };

static EGLDisplay display = EGL_NO_DISPLAY;
static EGLContext context = EGL_NO_CONTEXT;
static EGLSurface surface = EGL_NO_SURFACE;
static gboolean fbo_created = FALSE;

static void video_egl_swap_buffers();
static int video_egl_major = 0, video_egl_minor = 0;

gboolean video_egl_init()
{
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if( eglInitialize(display, &video_egl_major, &video_egl_minor) != EGL_TRUE ) {
        logEGLError( "Unable to initialise EGL display" );
        return FALSE;
    }
    return TRUE;
}

gboolean video_egl_init_context( EGLNativeWindowType window, int format )
{
    EGLConfig config;
    EGLint num_config;
    const EGLint *attribute_list;

    if( format == COLFMT_RGB565 || format == COLFMT_BGRA1555 ) {
        attribute_list = RGB565_attributes;
    } else {
        attribute_list = RGB888_attributes;
    }
    eglChooseConfig(display, attribute_list, &config, 1, &num_config);

    surface = eglCreateWindowSurface(display, config, window, NULL);
    if( surface == EGL_NO_SURFACE ) {
        /* Try alternate config in case of failure. This provides a workaround for
         * the Nokia N900 where eglCreateWindowSurface fails when color attributes
         * are specified. (bug report: https://bugs.maemo.org/show_bug.cgi?id=9335)
         */
        eglChooseConfig(display, alt_attributes, &config, 1, &num_config);
        surface = eglCreateWindowSurface(display, config, window, NULL);

        if( surface == EGL_NO_SURFACE ) {
            logEGLError( "Unable to create EGL surface" );
            video_egl_shutdown();
            return FALSE;
        }
    }

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    if( context == EGL_NO_CONTEXT ) {
        logEGLError( "Unable to create EGL context" );
        video_egl_shutdown();
        return FALSE;
    }



    if( eglMakeCurrent( display, surface, surface, context ) == EGL_FALSE ) {
        logEGLError( "Unable to make EGL context current" );
        video_egl_shutdown();
        return FALSE;
    }

    return TRUE;
}

gboolean video_egl_init_driver( display_driver_t driver )
{
    driver->swap_buffers = video_egl_swap_buffers;
    driver->capabilities.depth_bits = 16; /* TODO: get from config info */
    if( !gl_init_driver(driver, TRUE) ) {
        video_egl_shutdown();
        return FALSE;
    }
    fbo_created = TRUE;
    return TRUE;
}

void video_egl_shutdown()
{
    if( fbo_created ) {
        pvr2_shutdown_gl_context();
        gl_fbo_shutdown();
        fbo_created = FALSE;
    }
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if( surface != EGL_NO_SURFACE )  {
        eglDestroySurface(display, surface);
        surface = EGL_NO_SURFACE;
    }
    if( context != EGL_NO_CONTEXT ) {
        eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
    }
    if( display != EGL_NO_DISPLAY ) {
        eglTerminate(display);
        display = EGL_NO_DISPLAY;
    }
}

gboolean video_egl_set_window(EGLNativeWindowType window, int width, int height, int format)
{
    if( ! video_egl_init() ||
            ! video_egl_init_context( window, format ) ||
            ! video_egl_init_driver( &display_egl_driver ) ) {
        return FALSE;
    }
    gl_set_video_size(width, height, 0);
    pvr2_setup_gl_context();
    INFO( "Initialised EGL %d.%d", video_egl_major, video_egl_minor );
    return TRUE;
}

void video_egl_clear_window()
{
    video_egl_shutdown();
    INFO( "Terminated EGL" );
}

static void video_egl_swap_buffers()
{
    eglSwapBuffers(display, surface);
}


/**
 * Minimal init and shutdown. The real work is done from set_window
 */
struct display_driver display_egl_driver = {
        "egl", N_("OpenGLES driver"), NULL, NULL,
        NULL, NULL, NULL,
        NULL, NULL, NULL, NULL,
        gl_load_frame_buffer, gl_display_render_buffer, gl_display_blank,
        video_egl_swap_buffers, gl_read_render_buffer, NULL, NULL
};
