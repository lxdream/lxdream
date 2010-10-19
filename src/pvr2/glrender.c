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

#define IS_EMPTY_TILE_LIST(p) ((*((uint32_t *)(pvr2_main_ram+(p))) >> 28) == 0x0F)

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
    
    texcache_set_config( MMIO_READ( PVR2, RENDER_PALETTE ) & 0x03,
                         (MMIO_READ( PVR2, RENDER_TEXSIZE ) & 0x003F) << 5 );
    
    for( i=0; i < pvr2_scene.poly_count; i++ ) {
        struct polygon_struct *poly = &pvr2_scene.poly_array[i];
        if( POLY1_TEXTURED(poly->context[0]) ) {
            poly->tex_id = texcache_get_texture( poly->context[2],
                    POLY2_TEX_WIDTH(poly->context[1]),
                    POLY2_TEX_HEIGHT(poly->context[1]) );
            if( poly->mod_vertex_index != -1 ) {
                if( pvr2_scene.shadow_mode == SHADOW_FULL ) {
                    poly->mod_tex_id = texcache_get_texture( poly->context[4],
                            POLY2_TEX_WIDTH(poly->context[3]),
                            POLY2_TEX_HEIGHT(poly->context[3]) );
                } else {
                    poly->mod_tex_id = poly->tex_id;
                }
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
        if( !glsl_load_shaders( ) ) {
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
    glEnableClientState( GL_FOG_COORDINATE_ARRAY_EXT );

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(0);
    glClearStencil(0);

    glFogi(GL_FOG_COORDINATE_SOURCE_EXT, GL_FOG_COORDINATE_EXT);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0);
    glFogf(GL_FOG_END, 1.0);
}

static void render_set_cull( uint32_t poly1 )
{
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
}    

/**
 * Setup the basic context that's shared between normal and modified modes -
 * depth, culling
 */
static void render_set_base_context( uint32_t poly1, GLint depth_mode )
{
    if( depth_mode == 0 ) {
        glDepthFunc( POLY1_DEPTH_MODE(poly1) );
    } else {
        glDepthFunc(depth_mode);
    }

    glDepthMask( POLY1_DEPTH_WRITE(poly1) ? GL_TRUE : GL_FALSE );
    render_set_cull( poly1 );
}

/**
 * Setup the texture/shading settings (TSP) which vary between mod/unmod modes.
 */
static void render_set_tsp_context( uint32_t poly1, uint32_t poly2, uint32_t texture )
{
    glShadeModel( POLY1_SHADE_MODEL(poly1) );
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

     switch( POLY2_FOG_MODE(poly2) ) {
     case PVR2_POLY_FOG_LOOKUP:
         glFogfv( GL_FOG_COLOR, pvr2_scene.fog_lut_colour );
         break;
     case PVR2_POLY_FOG_VERTEX:
         glFogfv( GL_FOG_COLOR, pvr2_scene.fog_vert_colour );
         break;
     }

     int srcblend = POLY2_SRC_BLEND(poly2);
     int destblend = POLY2_DEST_BLEND(poly2);
     glBlendFunc( srcblend, destblend );

     if( POLY2_SRC_BLEND_TARGET(poly2) || POLY2_DEST_BLEND_TARGET(poly2) ) {
         WARN( "Accumulation buffer not supported" );
     }   
}

/**
 * Setup the GL context for the supplied polygon context.
 * @param context pointer to 3 or 5 words of polygon context
 * @param depth_mode force depth mode, or 0 to use the polygon's
 * depth mode.
 */
void render_set_context( uint32_t *context, GLint depth_mode )
{
    render_set_base_context(context[0], depth_mode);
    render_set_tsp_context(context[0],context[1],context[2]);
}


static void gl_render_poly( struct polygon_struct *poly, GLint depth_mode )
{
    if( poly->tex_id != -1 ) {
        glBindTexture(GL_TEXTURE_2D, poly->tex_id);
    }
    if( poly->mod_vertex_index == -1 ) {
        glDisable( GL_STENCIL_TEST );
        render_set_context( poly->context, depth_mode );
        glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index, poly->vertex_count );
    }  else {
        glEnable( GL_STENCIL_TEST );
        render_set_base_context( poly->context[0], depth_mode );
        render_set_tsp_context( poly->context[0], poly->context[1], poly->context[2] );
        glStencilFunc(GL_EQUAL, 0, 2);
        glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index, poly->vertex_count );

        if( pvr2_scene.shadow_mode == SHADOW_FULL ) {
            if( poly->mod_tex_id != -1 ) {
                glBindTexture(GL_TEXTURE_2D, poly->mod_tex_id);
            }
            render_set_tsp_context( poly->context[0], poly->context[3], poly->context[4] );
        }
        glStencilFunc(GL_EQUAL, 2, 2);
        glDrawArrays(GL_TRIANGLE_STRIP, poly->mod_vertex_index, poly->vertex_count );
    }
}

static void gl_render_bkgnd( struct polygon_struct *poly )
{
    if( poly->tex_id != -1 ) {
        glBindTexture(GL_TEXTURE_2D, poly->tex_id);
    }
    render_set_context( poly->context, 0 );
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_CULL_FACE );
    glBlendFunc( GL_ONE, GL_ZERO );
    glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index, poly->vertex_count );
    glEnable( GL_CULL_FACE );
    glEnable( GL_DEPTH_TEST );
}

void gl_render_tilelist( pvraddr_t tile_entry, GLint depth_mode )
{
    uint32_t *tile_list = (uint32_t *)(pvr2_main_ram+tile_entry);
    int strip_count;
    struct polygon_struct *poly;

    if( !IS_TILE_PTR(tile_entry) )
        return;

    while(1) {
        uint32_t entry = *tile_list++;
        switch( entry >> 28 ) {
        case 0x0F:
            return; // End-of-list
        case 0x0E:
            tile_list = (uint32_t *)(pvr2_main_ram + (entry&0x007FFFFF));
            break;
        case 0x08: case 0x09: case 0x0A: case 0x0B:
            strip_count = ((entry >> 25) & 0x0F)+1;
            poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
            while( strip_count > 0 ) {
                assert( poly != NULL );
                gl_render_poly( poly, depth_mode );
                poly = poly->next;
                strip_count--;
            }
            break;
        default:
            if( entry & 0x7E000000 ) {
                poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
                gl_render_poly( poly, depth_mode );
            }
        }
    }       
}

/**
 * Render the tilelist with depthbuffer updates only. 
 */
void gl_render_tilelist_depthonly( pvraddr_t tile_entry )
{
    uint32_t *tile_list = (uint32_t *)(pvr2_main_ram+tile_entry);
    int strip_count;
    struct polygon_struct *poly;
    
    if( !IS_TILE_PTR(tile_entry) )
        return;

    glDisable( GL_TEXTURE_2D );
    
    while(1) {
        uint32_t entry = *tile_list++;
        switch( entry >> 28 ) {
        case 0x0F:
            return; // End-of-list
        case 0x0E:
            tile_list = (uint32_t *)(pvr2_main_ram + (entry&0x007FFFFF));
            break;
        case 0x08: case 0x09: case 0x0A: case 0x0B:
            strip_count = ((entry >> 25) & 0x0F)+1;
            poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
            while( strip_count > 0 ) {
                render_set_base_context(poly->context[0],0);
                glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index, poly->vertex_count );
                poly = poly->next;
                strip_count--;
            }
            break;
        default:
            if( entry & 0x7E000000 ) {
                poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
                render_set_base_context(poly->context[0],0);
                glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index, poly->vertex_count );
            }
        }
    }           
}

static void drawrect2d( uint32_t tile_bounds[], float z )
{
    glBegin( GL_QUADS );
    glVertex3f( tile_bounds[0], tile_bounds[2], z );
    glVertex3f( tile_bounds[1], tile_bounds[2], z );
    glVertex3f( tile_bounds[1], tile_bounds[3], z );
    glVertex3f( tile_bounds[0], tile_bounds[3], z );
    glEnd();
}

void gl_render_modifier_polygon( struct polygon_struct *poly, uint32_t tile_bounds[] )
{
    /* A bit of explanation:
     * In theory it works like this: generate a 1-bit stencil for each polygon
     * volume, and then AND or OR it against the overall 1-bit tile stencil at 
     * the end of the volume. 
     * 
     * The implementation here uses a 2-bit stencil buffer, where each volume
     * is drawn using only stencil bit 0, and then a 'flush' polygon is drawn
     * to update bit 1 accordingly and clear bit 0.
     * 
     * This could probably be more efficient, but at least it works correctly 
     * now :)
     */
    
    render_set_cull(poly->context[0]);
    glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index, poly->vertex_count );
    
    int poly_type = POLY1_VOLUME_MODE(poly->context[0]);
    if( poly_type == PVR2_VOLUME_REGION0 ) {
        /* 00 => 00
         * 01 => 00
         * 10 => 10
         * 11 => 00
         */
        glStencilMask( 0x03 );
        glStencilFunc(GL_EQUAL, 0x02, 0x03);
        glStencilOp(GL_ZERO, GL_KEEP, GL_KEEP);
        glDisable( GL_CULL_FACE );
        glDisable( GL_DEPTH_TEST );

        drawrect2d( tile_bounds, pvr2_scene.bounds[4] );
        
        glEnable( GL_DEPTH_TEST );
        glStencilMask( 0x01 );
        glStencilFunc( GL_ALWAYS, 0, 1 );
        glStencilOp( GL_KEEP,GL_INVERT, GL_KEEP ); 
    } else if( poly_type == PVR2_VOLUME_REGION1 ) {
        /* This is harder with the standard stencil ops - do it in two passes
         * 00 => 00 | 00 => 10
         * 01 => 10 | 01 => 10
         * 10 => 10 | 10 => 00
         * 11 => 10 | 11 => 10
         */
        glStencilMask( 0x02 );
        glStencilOp( GL_INVERT, GL_INVERT, GL_INVERT );
        glDisable( GL_CULL_FACE );
        glDisable( GL_DEPTH_TEST );
        
        drawrect2d( tile_bounds, pvr2_scene.bounds[4] );
        
        glStencilMask( 0x03 );
        glStencilFunc( GL_NOTEQUAL,0x02, 0x03);
        glStencilOp( GL_ZERO, GL_REPLACE, GL_REPLACE );
        
        drawrect2d( tile_bounds, pvr2_scene.bounds[4] );
        
        glEnable( GL_DEPTH_TEST );
        glStencilMask( 0x01 );
        glStencilFunc( GL_ALWAYS, 0, 1 );
        glStencilOp( GL_KEEP,GL_INVERT, GL_KEEP );         
    }
}

void gl_render_modifier_tilelist( pvraddr_t tile_entry, uint32_t tile_bounds[] )
{
    uint32_t *tile_list = (uint32_t *)(pvr2_main_ram+tile_entry);
    int strip_count;
    struct polygon_struct *poly;

    if( !IS_TILE_PTR(tile_entry) )
        return;

    glDisable( GL_TEXTURE_2D );
    glDisable( GL_CULL_FACE );
    glEnable( GL_STENCIL_TEST );
    glEnable( GL_DEPTH_TEST );
    glDepthFunc( GL_LEQUAL );
    
    glStencilFunc( GL_ALWAYS, 0, 1 );
    glStencilOp( GL_KEEP,GL_INVERT, GL_KEEP ); 
    glStencilMask( 0x01 );
    glDepthMask( GL_FALSE );
    
    while(1) {
        uint32_t entry = *tile_list++;
        switch( entry >> 28 ) {
        case 0x0F:
            glDepthMask( GL_TRUE );
            glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
            return; // End-of-list
        case 0x0E:
            tile_list = (uint32_t *)(pvr2_main_ram + (entry&0x007FFFFF));
            break;
        case 0x08: case 0x09: case 0x0A: case 0x0B:
            strip_count = ((entry >> 25) & 0x0F)+1;
            poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
            while( strip_count > 0 ) {
                gl_render_modifier_polygon( poly, tile_bounds );
                poly = poly->next;
                strip_count--;
            }
            break;
        default:
            if( entry & 0x7E000000 ) {
                poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
                gl_render_modifier_polygon( poly, tile_bounds );
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
    glStencilMask( 0x03 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    /* Setup vertex array pointers */
    glVertexPointer(3, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].x);
    glColorPointer(4, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].rgba[0]);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].u);
    glSecondaryColorPointerEXT(3, GL_FLOAT, sizeof(struct vertex_struct), pvr2_scene.vertex_array[0].offset_rgba );
    glFogCoordPointerEXT(GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].offset_rgba[3] );
    /* Turn on the shaders (if available) */
    glsl_set_shader(DEFAULT_PROGRAM);

    /* Render the background */
    gl_render_bkgnd( pvr2_scene.bkgnd_poly );

    glEnable( GL_SCISSOR_TEST );
    glEnable( GL_COLOR_SUM );
    glEnable( GL_FOG );

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
        if( display_driver->capabilities.stencil_bits >= 2 && 
                IS_TILE_PTR(segment->opaquemod_ptr) &&
                !IS_EMPTY_TILE_LIST(segment->opaquemod_ptr) ) {
            /* Don't do this unless there's actually some shadow polygons */

            /* Use colormask instead of drawbuffer for simplicity */
            glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
            gl_render_tilelist_depthonly(segment->opaque_ptr);
            gl_render_modifier_tilelist(segment->opaquemod_ptr, tile_bounds);
            glClear( GL_DEPTH_BUFFER_BIT );
            glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
        }
        gl_render_tilelist(segment->opaque_ptr,0);
        if( IS_TILE_PTR(segment->punchout_ptr) ) {
            glEnable(GL_ALPHA_TEST );
            gl_render_tilelist(segment->punchout_ptr, GL_GEQUAL );
            glDisable(GL_ALPHA_TEST );
        }
        glDisable( GL_STENCIL_TEST );
        glStencilMask(0x03);
        glClear( GL_STENCIL_BUFFER_BIT );
        
        if( IS_TILE_PTR(segment->trans_ptr) ) {
            if( pvr2_scene.sort_mode == SORT_NEVER || 
                    (pvr2_scene.sort_mode == SORT_TILEFLAG && (segment->control&SEGMENT_SORT_TRANS))) {
                gl_render_tilelist(segment->trans_ptr, 0);
            } else {
                render_autosort_tile(segment->trans_ptr, RENDER_NORMAL );
            }
        }
    } while( !IS_LAST_SEGMENT(segment++) );
    glDisable( GL_SCISSOR_TEST );
    glDisable( GL_COLOR_SUM );
    glDisable( GL_FOG );
    glsl_clear_shader();

    gettimeofday( &end_tv, NULL );
    ms = (end_tv.tv_sec - tex_tv.tv_sec) * 1000 +
    (end_tv.tv_usec - tex_tv.tv_usec)/1000;
    DEBUG( "Scene render in %dms", ms );
}
