/**
 * $Id$
 *
 * GL framebuffer-based driver shell. This requires the EXT_framebuffer_object
 * extension, but is much nicer/faster/etc than pbuffers when it's available.
 * This is (optionally) used indirectly by the top-level GLX driver.
 *
 * Strategy-wise, we maintain 2 framebuffers with up to 4 target colour
 * buffers a piece. Effectively this reserves one fb for display output,
 * and the other for texture output (each fb has a single size).
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

#define GL_GLEXT_PROTOTYPES 1

#include <stdlib.h>
#include "lxdream.h"
#include "display.h"
#include "drivers/video_gl.h"
#include "pvr2/glutil.h"

#ifdef HAVE_OPENGL_FBO

#define MAX_FRAMEBUFFERS 2
#define MAX_TEXTURES_PER_FB 16

static render_buffer_t gl_fbo_create_render_buffer( uint32_t width, uint32_t height, GLuint tex_id );
static void gl_fbo_destroy_render_buffer( render_buffer_t buffer );
static gboolean gl_fbo_set_render_target( render_buffer_t buffer );
static void gl_fbo_finish_render( render_buffer_t buffer );
static void gl_fbo_display_render_buffer( render_buffer_t buffer );
static void gl_fbo_load_frame_buffer( frame_buffer_t frame, render_buffer_t buffer );
static void gl_fbo_display_blank( uint32_t colour );
static gboolean gl_fbo_read_render_buffer( unsigned char *target, render_buffer_t buffer, int rowstride, int format );

extern uint32_t video_width, video_height;

/**
 * Framebuffer info structure
 */
struct gl_fbo_info {
    GLuint fb_id;
    GLuint depth_id;
    GLuint stencil_id;
    GLuint tex_ids[MAX_TEXTURES_PER_FB];
    int width, height;
};

static GLint gl_fbo_max_attachments = 0;
static struct gl_fbo_info fbo[MAX_FRAMEBUFFERS];

#define ATTACHMENT_POINT(n) (GL_COLOR_ATTACHMENT0_EXT+(n))
static int last_used_fbo;

gboolean gl_fbo_is_supported()
{
    return isGLExtensionSupported("GL_EXT_framebuffer_object");
}

/**
 * Construct the initial frame buffers and allocate ids for everything.
 * The render handling driver methods are set to the fbo versions.
 */
void gl_fbo_init( display_driver_t driver ) 
{
    int i,j;
    GLuint fbids[MAX_FRAMEBUFFERS];
    GLuint rbids[MAX_FRAMEBUFFERS*2]; /* depth buffer, stencil buffer per fb */

    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &gl_fbo_max_attachments);
    glGenFramebuffersEXT( MAX_FRAMEBUFFERS, &fbids[0] );
    glGenRenderbuffersEXT( MAX_FRAMEBUFFERS*2, &rbids[0] );
    for( i=0; i<MAX_FRAMEBUFFERS; i++ ) {
        fbo[i].fb_id = fbids[i];
        fbo[i].depth_id = rbids[i*2];
        fbo[i].stencil_id = rbids[i*2+1];
        fbo[i].width = -1;
        fbo[i].height = -1;
        for( j=0; j<MAX_TEXTURES_PER_FB; j++ ) {
            fbo[i].tex_ids[j] = -1;
        }
    }
    last_used_fbo = 0;

    driver->create_render_buffer = gl_fbo_create_render_buffer;
    driver->destroy_render_buffer = gl_fbo_destroy_render_buffer;
    driver->set_render_target = gl_fbo_set_render_target;
    driver->finish_render = gl_fbo_finish_render;
    driver->display_render_buffer = gl_fbo_display_render_buffer;
    driver->load_frame_buffer = gl_fbo_load_frame_buffer;
    driver->display_blank = gl_fbo_display_blank;
    driver->read_render_buffer = gl_fbo_read_render_buffer;

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glDrawBuffer(GL_FRONT);
    glReadBuffer(GL_FRONT);
}

void gl_fbo_shutdown()
{
    int i;
    glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
    for( i=0; i<MAX_FRAMEBUFFERS; i++ ) {
        glDeleteFramebuffersEXT( 1, &fbo[i].fb_id );
        glDeleteRenderbuffersEXT( 2, &fbo[i].depth_id );
    }
}

void gl_fbo_setup_framebuffer( int bufno, int width, int height )
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo[bufno].fb_id);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, fbo[bufno].depth_id);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT, fbo[bufno].depth_id);
    /* Stencil doesn't work on ATI, and we're not using it at the moment anyway, so...
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, fbo[bufno].stencil_id);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_STENCIL_INDEX, width, height);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT,
				 GL_RENDERBUFFER_EXT, fbo[bufno].stencil_id);
     */
    fbo[bufno].width = width;
    fbo[bufno].height = height;
}

int gl_fbo_get_framebuffer( int width, int height ) 
{
    int bufno = -1, i;
    /* find a compatible framebuffer context */
    for( i=0; i<MAX_FRAMEBUFFERS; i++ ) {
        if( fbo[i].width == -1 && bufno == -1 ) {
            bufno = i;
        } else if( fbo[i].width == width && fbo[i].height == height ) {
            bufno = i;
            break;
        }
    }
    if( bufno == -1 ) {
        bufno = last_used_fbo + 1;
        if( bufno > MAX_FRAMEBUFFERS ) {
            bufno = 0;
        }
        last_used_fbo = bufno;
    }
    if( fbo[bufno].width == width && fbo[bufno].height == height ) {
        glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo[bufno].fb_id );
    } else {
        gl_fbo_setup_framebuffer( bufno, width, height );
    } 
    return bufno;
}

/**
 * Attach a texture to the framebuffer. The texture must already be initialized
 * to the correct dimensions etc.
 */
static GLint gl_fbo_attach_texture( int fbo_no, GLint tex_id ) {
    int attach = -1, i;
    for( i=0; i<gl_fbo_max_attachments; i++ ) {
        if( fbo[fbo_no].tex_ids[i] == tex_id ) {
            glDrawBuffer(ATTACHMENT_POINT(i));
            glReadBuffer(ATTACHMENT_POINT(i)); 
            return ATTACHMENT_POINT(i); // already attached
        } else if( fbo[fbo_no].tex_ids[i] == -1 && attach == -1 ) {
            attach = i;
        }
    }
    if( attach == -1 ) {
        attach = 0;
    }
    fbo[fbo_no].tex_ids[attach] = tex_id;
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, 0 ); // Ensure the output texture is unbound
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, ATTACHMENT_POINT(attach), 
                              GL_TEXTURE_RECTANGLE_ARB, tex_id, 0 );
    /* Set draw/read buffers by default */
    glDrawBuffer(ATTACHMENT_POINT(attach));
    glReadBuffer(ATTACHMENT_POINT(attach)); 


    GLint status = glGetError();
    if( status != GL_NO_ERROR ) {
        ERROR( "GL error setting render target (%x)!", status );
    }
    status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if( status != GL_FRAMEBUFFER_COMPLETE_EXT ) {
        ERROR( "Framebuffer failure: %x", status );
        exit(1);
    }

    return ATTACHMENT_POINT(attach);
}

static render_buffer_t gl_fbo_create_render_buffer( uint32_t width, uint32_t height, GLuint tex_id )
{
    render_buffer_t buffer = calloc( sizeof(struct render_buffer), 1 );
    buffer->width = width;
    buffer->height = height;
    buffer->tex_id = tex_id;
    if( tex_id == 0 ) {
        GLuint tex;
        glGenTextures( 1, &tex );
        buffer->buf_id = tex;
        glBindTexture( GL_TEXTURE_RECTANGLE_ARB, tex );
        glTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
    } else {
        buffer->buf_id = tex_id;
        glBindTexture( GL_TEXTURE_RECTANGLE_ARB, tex_id );
    }
    glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    return buffer;
}

/**
 * Ensure the texture in the given render buffer is not attached to a 
 * framebuffer (ie, so we can safely use it as a texture during the rendering
 * cycle, or before deletion).
 */
static void gl_fbo_detach_render_buffer( render_buffer_t buffer )
{
    int i,j;
    for( i=0; i<MAX_FRAMEBUFFERS; i++ ) {
        if( fbo[i].width == buffer->width && fbo[i].height == buffer->height ) {
            for( j=0; j<gl_fbo_max_attachments; j++ ) {
                if( fbo[i].tex_ids[j] == buffer->buf_id ) {
                    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo[i].fb_id);
                    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, ATTACHMENT_POINT(j), 
                                              GL_TEXTURE_RECTANGLE_ARB, GL_NONE, 0 );
                    fbo[i].tex_ids[j] = -1;
                    return;
                }                    
            }
            break;
        }
    }
}

static void gl_fbo_destroy_render_buffer( render_buffer_t buffer )
{
    int i,j;

    gl_fbo_detach_render_buffer( buffer );

    if( buffer->buf_id != buffer->tex_id ) {
        // If tex_id was set at buffer creation, we don't own the texture.
        // Otherwise, delete it now.
        GLuint tex = buffer->buf_id; 
        glDeleteTextures( 1, &tex );
    }
    buffer->buf_id = 0;
    buffer->tex_id = 0;
    free( buffer );
}

static gboolean gl_fbo_set_render_target( render_buffer_t buffer )
{
    glFinish();
    glGetError();
    int fb = gl_fbo_get_framebuffer( buffer->width, buffer->height );
    gl_fbo_attach_texture( fb, buffer->buf_id );
    /* setup the gl context */
    glViewport( 0, 0, buffer->width, buffer->height );

    return TRUE;
}

static void gl_fbo_finish_render( render_buffer_t buffer )
{
    glFinish();
    glGetError();
    gl_fbo_detach_render_buffer(buffer);
}

/**
 * Render the texture holding the given buffer to the front window
 * buffer.
 */
static void gl_fbo_display_render_buffer( render_buffer_t buffer )
{
    glFinish();
    gl_fbo_detach();
    gl_display_render_buffer( buffer );
}

static void gl_fbo_load_frame_buffer( frame_buffer_t frame, render_buffer_t buffer )
{
    glFinish();
    gl_fbo_detach();
    gl_load_frame_buffer( frame, buffer->buf_id );
}

static void gl_fbo_display_blank( uint32_t colour )
{
    glFinish();
    gl_fbo_detach();
    gl_display_blank( colour );
}

void gl_fbo_detach()
{
    glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
    glDrawBuffer( GL_FRONT );
    glReadBuffer( GL_FRONT );
}    

static gboolean gl_fbo_read_render_buffer( unsigned char *target, render_buffer_t buffer, 
                                           int rowstride, int format )
{
    glGetError();
    int fb = gl_fbo_get_framebuffer( buffer->width, buffer->height );
    gl_fbo_attach_texture( fb, buffer->buf_id );
    return gl_read_render_buffer( target, buffer, rowstride, format );
}

#else
gboolean gl_fbo_is_supported()
{
    return FALSE;
}

void gl_fbo_init( display_driver_t driver ) 
{
}

#endif
