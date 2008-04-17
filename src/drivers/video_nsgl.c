/**
 * $Id$
 *
 * Cocoa (NSOpenGL) video driver
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

#include <AppKit/NSOpenGL.h>
#include "drivers/video_nsgl.h"
#include "drivers/video_gl.h"
#include "pvr2/glutil.h"

static NSOpenGLContext *nsgl_context;

gboolean video_nsgl_init_driver( NSView *view, display_driver_t driver )
{
	NSOpenGLPixelFormatAttribute attributes[] = {
			NSOpenGLPFAWindow,
			NSOpenGLPFADoubleBuffer,
			NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)24,
			(NSOpenGLPixelFormatAttribute)nil };
	
	NSOpenGLPixelFormat *pixelFormat = 
		[[[NSOpenGLPixelFormat alloc] initWithAttributes: attributes] autorelease];
	nsgl_context = 
		[[NSOpenGLContext alloc] initWithFormat: pixelFormat shareContext: nil];
	[nsgl_context setView: view];
	[nsgl_context makeCurrentContext];
	
	if( gl_fbo_is_supported() ) {
		gl_fbo_init(driver);
	} else {
		ERROR( "FBO not supported" );
		return FALSE;
	}
	
    if( glsl_is_supported() ) {
    	if( !glsl_load_shaders( glsl_vertex_shader_src, NULL ) ) {
            WARN( "Unable to load GL shaders" );
        }
    }
	return TRUE;
}

void video_nsgl_shutdown()
{
	if( nsgl_context != nil ) {
		[NSOpenGLContext clearCurrentContext];
		[nsgl_context release];
		nsgl_context = nil;
	}
}
