/**
 * $Id$
 *
 * Standard OpenGL rendering engine. 
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

#include <assert.h>
#include <sys/time.h>
#include "display.h"
#include "pvr2/pvr2.h"
#include "pvr2/pvr2mmio.h"
#include "pvr2/scene.h"
#include "pvr2/glutil.h"

int pvr2_poly_depthmode[8] = { GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL,
        GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, 
        GL_ALWAYS };
int pvr2_poly_srcblend[8] = { 
        GL_ZERO, GL_ONE, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, 
        GL_ONE_MINUS_DST_ALPHA };
int pvr2_poly_dstblend[8] = {
        GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
        GL_ONE_MINUS_DST_ALPHA };
int pvr2_poly_texblend[4] = {
        GL_REPLACE, 
        GL_MODULATE,  
        GL_DECAL, 
        GL_MODULATE 
};
int pvr2_render_colour_format[8] = {
        COLFMT_BGRA1555, COLFMT_RGB565, COLFMT_BGRA4444, COLFMT_BGRA1555,
        COLFMT_BGR888, COLFMT_BGRA8888, COLFMT_BGRA8888, COLFMT_BGRA4444 };


/**
 * Clip the tile bounds to the clipping plane. 
 * @return TRUE if the tile was not clipped completely.
 */
static gboolean clip_tile_bounds( uint32_t *tile, float *clip )
{
    if( tile[0] < clip[0] ) tile[0] = clip[0];
    if( tile[1] > clip[1] ) tile[1] = clip[1];
    if( tile[2] < clip[2] ) tile[2] = clip[2];
    if( tile[3] > clip[3] ) tile[3] = clip[3];
    return tile[0] < tile[1] && tile[2] < tile[3];
}

void pvr2_scene_load_textures()
{
    int i;
    for( i=0; i < pvr2_scene.poly_count; i++ ) {
        struct polygon_struct *poly = &pvr2_scene.poly_array[i];
        if( POLY1_TEXTURED(poly->context[0]) ) {
            poly->tex_id = texcache_get_texture( poly->context[2],
                    POLY2_TEX_WIDTH(poly->context[1]),
                    POLY2_TEX_HEIGHT(poly->context[1]) );
            if( poly->mod_vertex_index != -1 ) {
                poly->mod_tex_id = texcache_get_texture( poly->context[4],
                        POLY2_TEX_WIDTH(poly->context[3]),
                        POLY2_TEX_HEIGHT(poly->context[3]) );
            }
        } else {
            poly->tex_id = -1;
            poly->mod_tex_id = -1;
        }
    }
}



/**
 * Once-off call to setup the OpenGL context.
 */
void pvr2_setup_gl_context()
{

    if( glsl_is_supported() ) {
        if( !glsl_load_shaders( glsl_vertex_shader_src, NULL ) ) {
            WARN( "Unable to load GL shaders" );
        }
    }

    texcache_gl_init(); // Allocate texture IDs
    glCullFace( GL_BACK );
    glEnable( GL_BLEND );
    glEnable( GL_DEPTH_TEST );
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

#ifdef HAVE_OPENGL_CLAMP_COLOR
    if( isGLExtensionSupported("GL_ARB_color_buffer_float") ) {
        glClampColorARB(GL_CLAMP_VERTEX_COLOR_ARB, GL_FALSE );
        glClampColorARB(GL_CLAMP_FRAGMENT_COLOR_ARB, GL_FALSE );
    }
#endif

    glEnableClientState( GL_COLOR_ARRAY );
    glEnableClientState( GL_VERTEX_ARRAY );
    glEnableClientState( GL_TEXTURE_COORD_ARRAY );
    glEnableClientState( GL_SECONDARY_COLOR_ARRAY );

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(0);
    glClearStencil(0);
}

/**
 * Setup the GL context for the supplied polygon context.
 * @param context pointer to 3 or 5 words of polygon context
 * @param modified boolean flag indicating that the modified
 *  version should be used, rather than the normal version.
 */
void render_set_context( uint32_t *context, int render_mode )
{
    uint32_t poly1 = context[0], poly2, texture;
    if( render_mode == RENDER_FULLMOD ) {
        poly2 = context[3];
        texture = context[4];
    } else {
        poly2 = context[1];
        texture = context[2];
    }

    glDepthFunc( POLY1_DEPTH_MODE(poly1) );
    glDepthMask( POLY1_DEPTH_WRITE(poly1) ? GL_TRUE : GL_FALSE );

    switch( POLY1_CULL_MODE(poly1) ) {
    case CULL_NONE:
    case CULL_SMALL:
        glDisable( GL_CULL_FACE );
        break;
    case CULL_CCW:
        glEnable( GL_CULL_FACE );
        glFrontFace( GL_CW );
        break;
    case CULL_CW:
        glEnable( GL_CULL_FACE );
        glFrontFace( GL_CCW );
        break;
    }

    if( POLY1_SPECULAR(poly1) ) {
        glEnable(GL_COLOR_SUM);
    } else {
        glDisable(GL_COLOR_SUM);
    }


    if( POLY1_TEXTURED(poly1) ) {
        glEnable(GL_TEXTURE_2D);
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, pvr2_poly_texblend[POLY2_TEX_BLEND(poly2)] );
        if( POLY2_TEX_CLAMP_U(poly2) ) {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
        } else if( POLY2_TEX_MIRROR_U(poly2) ) {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT_ARB );
        } else {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        }	    
        if( POLY2_TEX_CLAMP_V(poly2) ) {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
        } else if( POLY2_TEX_MIRROR_V(poly2) ) {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT_ARB );
        } else {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
        }
    } else {
        glDisable( GL_TEXTURE_2D );
    }

    glShadeModel( POLY1_SHADE_MODEL(poly1) );

    int srcblend = POLY2_SRC_BLEND(poly2);
    int destblend = POLY2_DEST_BLEND(poly2);
    glBlendFunc( srcblend, destblend );

    if( POLY2_SRC_BLEND_TARGET(poly2) || POLY2_DEST_BLEND_TARGET(poly2) ) {
        ERROR( "Accumulation buffer not supported" );
    }

}


static void gl_render_poly( struct polygon_struct *poly )
{
    if( poly->tex_id != -1 ) {
        glBindTexture(GL_TEXTURE_2D, poly->tex_id);
    }
    render_set_context( poly->context, RENDER_NORMAL );
    glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index, poly->vertex_count );
}


static void gl_render_bkgnd( struct polygon_struct *poly )
{
    if( poly->tex_id != -1 ) {
        glBindTexture(GL_TEXTURE_2D, poly->tex_id);
    }
    render_set_context( poly->context, RENDER_NORMAL );
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_CULL_FACE );
    glBlendFunc( GL_ONE, GL_ZERO );
    glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index, poly->vertex_count );
    glEnable( GL_CULL_FACE );
    glEnable( GL_DEPTH_TEST );
}



void gl_render_tilelist( pvraddr_t tile_entry )
{
    uint32_t *tile_list = (uint32_t *)(video_base+tile_entry);
    int strip_count;
    struct polygon_struct *poly;

    while(1) {
        uint32_t entry = *tile_list++;
        switch( entry >> 28 ) {
        case 0x0F:
            return; // End-of-list
        case 0x0E:
            tile_list = (uint32_t *)(video_base + (entry&0x007FFFFF));
            break;
        case 0x08: case 0x09: case 0x0A: case 0x0B:
            strip_count = ((entry >> 25) & 0x0F)+1;
            poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
            while( strip_count > 0 ) {
                assert( poly != NULL );
                gl_render_poly( poly );
                poly = poly->next;
                strip_count--;
            }
            break;
        default:
            if( entry & 0x7E000000 ) {
                poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
                gl_render_poly( poly );
            }
        }
    }	    
}


/**
 * Render the currently defined scene in pvr2_scene
 */
void pvr2_scene_render( render_buffer_t buffer )
{
    /* Scene setup */
    struct timeval start_tv, tex_tv, end_tv;

    gettimeofday(&start_tv, NULL);
    display_driver->set_render_target(buffer);
    pvr2_check_palette_changed();
    pvr2_scene_load_textures();

    gettimeofday( &tex_tv, NULL );
    uint32_t ms = (tex_tv.tv_sec - start_tv.tv_sec) * 1000 +
    (tex_tv.tv_usec - start_tv.tv_usec)/1000;
    DEBUG( "Scene setup in %dms", ms );

    /* Setup view projection matrix */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float nearz = pvr2_scene.bounds[4];
    float farz = pvr2_scene.bounds[5];
    if( nearz == farz ) {
        farz*= 4.0;
    }
    glOrtho( 0, pvr2_scene.buffer_width, pvr2_scene.buffer_height, 0, 
             -farz, -nearz );
    float alphaRef = ((float)(MMIO_READ(PVR2, RENDER_ALPHA_REF)&0xFF)+1)/256.0;
    glAlphaFunc( GL_GEQUAL, alphaRef );

    /* Clear the buffer (FIXME: May not want always want to do this) */
    glDisable( GL_SCISSOR_TEST );
    glDepthMask( GL_TRUE );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    /* Setup vertex array pointers */
    glVertexPointer(3, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].x);
    glColorPointer(4, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].rgba[0]);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].u);
    glSecondaryColorPointerEXT(3, GL_FLOAT, sizeof(struct vertex_struct), pvr2_scene.vertex_array[0].offset_rgba );

    /* Turn on the shaders (if available) */
    glsl_enable_shaders(TRUE);

    /* Render the background */
    gl_render_bkgnd( pvr2_scene.bkgnd_poly );

    glEnable( GL_SCISSOR_TEST );

    /* Process the segment list */
    struct tile_segment *segment = pvr2_scene.segment_list;
    do {
        int tilex = SEGMENT_X(segment->control);
        int tiley = SEGMENT_Y(segment->control);

        uint32_t tile_bounds[4] = { tilex << 5, (tilex+1)<<5, tiley<<5, (tiley+1)<<5 };
        if( !clip_tile_bounds(tile_bounds, pvr2_scene.bounds) ) {
            continue; // fully clipped, skip tile
        }

        /* Clip to the visible part of the tile */
        glScissor( tile_bounds[0], pvr2_scene.buffer_height-tile_bounds[3], 
                   tile_bounds[1]-tile_bounds[0], tile_bounds[3] - tile_bounds[2] );
        if( IS_TILE_PTR(segment->opaque_ptr) ) {
            gl_render_tilelist(segment->opaque_ptr);
        }
        if( IS_TILE_PTR(segment->trans_ptr) ) {
            if( pvr2_scene.sort_mode == SORT_NEVER || 
                    (pvr2_scene.sort_mode == SORT_TILEFLAG && (segment->control&SEGMENT_SORT_TRANS))) {
                gl_render_tilelist(segment->trans_ptr);
            } else {
                render_autosort_tile(segment->trans_ptr, RENDER_NORMAL );
            }
        }
        if( IS_TILE_PTR(segment->punchout_ptr) ) {
            glEnable(GL_ALPHA_TEST );
            render_autosort_tile(segment->punchout_ptr, RENDER_NORMAL );
            glDisable(GL_ALPHA_TEST );
        }
    } while( !IS_LAST_SEGMENT(segment++) );
    glDisable( GL_SCISSOR_TEST );

    glsl_enable_shaders(FALSE);

    gettimeofday( &end_tv, NULL );
    ms = (end_tv.tv_sec - tex_tv.tv_sec) * 1000 +
    (end_tv.tv_usec - tex_tv.tv_usec)/1000;
    DEBUG( "Scene render in %dms", ms );
}
