/**
 * $Id$
 *
 * Generic GL vertex buffer/vertex array support
 *
 * Copyright (c) 2011 Nathan Keynes.
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

#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "lxdream.h"
#include "display.h"
#include "drivers/video_gl.h"
#include "pvr2/glutil.h"

#define MIN_VERTEX_ARRAY_SIZE (1024*1024)

vertex_buffer_t vertex_buffer_new( vertex_buffer_t vtable )
{
    vertex_buffer_t buf = g_malloc(sizeof(struct vertex_buffer));
    memcpy( buf, vtable, sizeof(struct vertex_buffer));
    buf->data = 0;
    buf->id = 0;
    buf->mapped_size = buf->capacity = 0;
    buf->fence = 0;
    return buf;
}

/******************************* Default ***********************************/

static void *def_map( vertex_buffer_t buf, uint32_t size )
{
    buf->mapped_size = size;
    if( size < MIN_VERTEX_ARRAY_SIZE )
        size = MIN_VERTEX_ARRAY_SIZE;
    if( size > buf->capacity ) {
        g_free(buf->data);
        buf->data = g_malloc(size);
        buf->capacity = size;
    }
    return buf->data;
}

static void *def_unmap( vertex_buffer_t buf )
{
    return buf->data;
}

static void def_finished( vertex_buffer_t buf )
{
}

static void def_destroy( vertex_buffer_t buf )
{
    g_free(buf->data);
    buf->data = NULL;
    g_free(buf);
}

static struct vertex_buffer def_vtable = { def_map, def_unmap, def_finished, def_destroy };

static vertex_buffer_t def_create_buffer( )
{
    return vertex_buffer_new( &def_vtable );
}

/************************** vertex_array_range *****************************/

/**
 * VAR extensions like the buffer to be allocated on page boundaries.
 */
static void var_alloc_pages( vertex_buffer_t buf, uint32_t size )
{
    if( size < MIN_VERTEX_ARRAY_SIZE )
        size = MIN_VERTEX_ARRAY_SIZE;
    if( size > buf->capacity ) {
        size = (size + 4096-1) & (~(4096-1));
        if( buf->data != NULL ) {
            munmap( buf->data, buf->capacity );
        }
        buf->data = mmap( NULL, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0 );
        assert( buf->data != MAP_FAILED );
        buf->capacity = size;
    }
}

#ifdef APPLE_BUILD

static void *apple_map( vertex_buffer_t buf, uint32_t size )
{
    glFinishFenceAPPLE(buf->fence);
    var_alloc_pages( buf, size );
    glVertexArrayRangeAPPLE(size, buf->data);
    buf->mapped_size = size;
    return buf->data;
}

static void *apple_unmap( vertex_buffer_t buf )
{
    glFlushVertexArrayRangeAPPLE(buf->mapped_size, buf->data);
    return buf->data;
}

static void apple_finished( vertex_buffer_t buf )
{
    glSetFenceAPPLE(buf->fence);
}

static void apple_destroy( vertex_buffer_t buf )
{
    glVertexArrayRangeAPPLE(0,0);
    glDeleteFencesAPPLE(1, &buf->fence);
    munmap( buf->data, buf->capacity );
    g_free(buf);
}
static struct vertex_buffer apple_vtable = { apple_map, apple_unmap, apple_finished, apple_destroy };

static vertex_buffer_t apple_create_buffer( uint32_t size )
{
    vertex_buffer_t buf = vertex_buffer_new( &apple_vtable );
    glGenFencesAPPLE(1, &buf->fence);
    return buf;
}

#endif

#ifdef GL_VERTEX_ARRAY_RANGE_NV

static void *nv_map( vertex_buffer_t buf, uint32_t size )
{
    glFinishFenceNV(buf->fence);
    var_alloc_pages( buf, size );
    glVertexArrayRangeNV(size, buf->data);
    buf->mapped_size = size;
    return buf->data;
}
static void *nv_unmap( vertex_buffer_t buf )
{
    glFlushVertexArrayRangeNV();
    return buf->data;
}

static void nv_finished( vertex_buffer_t buf )
{
    glSetFenceNV(buf->fence, GL_ALL_COMPLETED_NV);
}

static void nv_destroy( vertex_buffer_t buf )
{
    glVertexArrayRangeNV(0,0);
    glDeleteFencesNV(1, &buf->fence);
    munmap( buf->data, buf->capacity );
    g_free(buf);
}

static struct vertex_buffer nv_vtable = { nv_map, nv_unmap, nv_finished, nv_destroy };

static vertex_buffer_t nv_create_buffer( uint32_t size )
{
    vertex_buffer_t buf = vertex_buffer_new( &nv_vtable );
    glGenFencesNV(1, &buf->fence);
    return buf;
}

#endif /* !GL_VERTEX_ARRAY_RANGE_NV */

/************************** vertex_buffer_object *****************************/

#ifdef GL_ARRAY_BUFFER_ARB

static void *vbo_map( vertex_buffer_t buf, uint32_t size )
{
    glBindBufferARB( GL_ARRAY_BUFFER_ARB, buf->id );
     if( size > buf->capacity ) {
         glBufferDataARB( GL_ARRAY_BUFFER_ARB, size, NULL, GL_STREAM_DRAW_ARB );
         assert( gl_check_error("Allocating vbo data") );
         buf->capacity = size;
    }
    buf->data = glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
    buf->mapped_size = buf->capacity;
    return buf->data;
}

static void *vbo_unmap( vertex_buffer_t buf )
{
    glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
    return NULL;
}

static void vbo_finished( vertex_buffer_t buf )
{
    glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

static void vbo_destroy( vertex_buffer_t buf )
{
    glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
    glDeleteBuffersARB( 1, &buf->id );
}

static struct vertex_buffer vbo_vtable = { vbo_map, vbo_unmap, vbo_finished, vbo_destroy };

static vertex_buffer_t vbo_create_buffer( uint32_t size )
{
    vertex_buffer_t buf = vertex_buffer_new( &vbo_vtable );
    glGenBuffersARB( 1, &buf->id );
    return buf;
}

#endif

/**
 * Auto-detect the supported vertex buffer types, and select between them.
 * Use vertex_buffer_object if available, otherwise vertex_array_range,
 * otherwise just pure host buffers.
 */
void gl_vbo_init( display_driver_t driver ) {
/* VBOs are disabled for now as they won't work with the triangle sorting,
 * plus they seem to be slower than the other options anyway.
 */
#ifdef ENABLE_VBO
#ifdef GL_ARRAY_BUFFER_ARB
    if( isGLVertexBufferSupported() ) {
        driver->create_vertex_buffer = vbo_create_buffer;
        return;
    }
#endif
#endif

#ifdef APPLE_BUILD
    if( isGLExtensionSupported("GL_APPLE_vertex_array_range") &&
            isGLExtensionSupported("GL_APPLE_fence") ) {
        glEnableClientState( GL_VERTEX_ARRAY_RANGE_APPLE );
        driver->create_vertex_buffer = apple_create_buffer;
        return;
    }
#endif

#ifdef GL_VERTEX_ARRAY_RANGE_NV
    if( isGLExtensionSupported("GL_NV_vertex_array_range") &&
            isGLExtensionSupported("GL_NV_fence") ) {
        glEnableClientState( GL_VERTEX_ARRAY_RANGE_NV );
        driver->create_vertex_buffer = nv_create_buffer;
        return;
    }
#endif
    driver->create_vertex_buffer = def_create_buffer;
}

void gl_vbo_fallback_init( display_driver_t driver ) {
    driver->create_vertex_buffer = def_create_buffer;
}
