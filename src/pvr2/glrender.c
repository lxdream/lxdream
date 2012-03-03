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
#include "pvr2/glutil.h"
#include "pvr2/scene.h"
#include "pvr2/tileiter.h"
#include "pvr2/shaders.h"

#ifdef APPLE_BUILD
#include "OpenGL/CGLCurrent.h"
#include "OpenGL/CGLMacro.h"

static CGLContextObj CGL_MACRO_CONTEXT;
#endif

#define IS_NONEMPTY_TILE_LIST(p) (IS_TILE_PTR(p) && ((*((uint32_t *)(pvr2_main_ram+(p))) >> 28) != 0x0F))

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

static gboolean have_shaders = FALSE;
static int currentTexId = -1;

static inline void bind_texture(int texid)
{
    if( currentTexId != texid ) {
        currentTexId = texid;
        glBindTexture(GL_TEXTURE_2D, texid);
    }
}

/**
 * Clip the tile bounds to the clipping plane. 
 * @return TRUE if the tile was not clipped completely.
 */
static gboolean clip_tile_bounds( uint32_t *tile, uint32_t *clip )
{
    if( tile[0] < clip[0] ) tile[0] = clip[0];
    if( tile[1] > clip[1] ) tile[1] = clip[1];
    if( tile[2] < clip[2] ) tile[2] = clip[2];
    if( tile[3] > clip[3] ) tile[3] = clip[3];
    return tile[0] < tile[1] && tile[2] < tile[3];
}

static void drawrect2d( uint32_t tile_bounds[], float z )
{
    /* FIXME: Find a non-fixed-func way to do this */
#ifdef HAVE_OPENGL_FIXEDFUNC
    glBegin( GL_TRIANGLE_STRIP );
    glVertex3f( tile_bounds[0], tile_bounds[2], z );
    glVertex3f( tile_bounds[1], tile_bounds[2], z );
    glVertex3f( tile_bounds[0], tile_bounds[3], z );
    glVertex3f( tile_bounds[1], tile_bounds[3], z );
    glEnd();
#endif
}

static void pvr2_scene_load_textures()
{
    int i;
    
    texcache_begin_scene( MMIO_READ( PVR2, RENDER_PALETTE ) & 0x03,
                         (MMIO_READ( PVR2, RENDER_TEXSIZE ) & 0x003F) << 5 );
    
    for( i=0; i < pvr2_scene.poly_count; i++ ) {
        struct polygon_struct *poly = &pvr2_scene.poly_array[i];
        if( POLY1_TEXTURED(poly->context[0]) ) {
            poly->tex_id = texcache_get_texture( poly->context[1], poly->context[2] );
            if( poly->mod_vertex_index != -1 ) {
                if( pvr2_scene.shadow_mode == SHADOW_FULL ) {
                    poly->mod_tex_id = texcache_get_texture( poly->context[3], poly->context[4] );
                } else {
                    poly->mod_tex_id = poly->tex_id;
                }
            }
        } else {
            poly->tex_id = 0;
            poly->mod_tex_id = 0;
        }
    }
}


/**
 * Once-off call to setup the OpenGL context.
 */
void pvr2_setup_gl_context()
{
    if( glsl_is_supported() && isGLMultitextureSupported() ) {
        if( !glsl_load_shaders( ) ) {
            WARN( "Unable to load GL shaders" );
        } else {
            INFO( "Shaders loaded successfully" );
            have_shaders = TRUE;
        }
    } else {
        INFO( "Shaders not supported" );
    }

#ifdef APPLE_BUILD
    CGL_MACRO_CONTEXT = CGLGetCurrentContext();
#endif
    texcache_gl_init(have_shaders); // Allocate texture IDs

    /* Global settings */
    glDisable( GL_CULL_FACE );
    glEnable( GL_BLEND );
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

#ifdef HAVE_OPENGL_CLAMP_COLOR
    if( isGLExtensionSupported("GL_ARB_color_buffer_float") ) {
        glClampColorARB(GL_CLAMP_VERTEX_COLOR_ARB, GL_FALSE );
        glClampColorARB(GL_CLAMP_FRAGMENT_COLOR_ARB, GL_FALSE );
    }
#endif

#ifdef HAVE_OPENGL_FIXEDFUNC
    /* Setup defaults for perspective correction + matrices */
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
#endif


#ifdef HAVE_OPENGL_CLEAR_DEPTHF
    glClearDepthf(0);
#else
    glClearDepth(0);
#endif
    glClearStencil(0);
}

/**
 * Setup the basic context that's shared between normal and modified modes -
 * depth, culling
 */
static void render_set_base_context( uint32_t poly1, gboolean set_depth )
{
    if( set_depth ) {
        glDepthFunc( POLY1_DEPTH_MODE(poly1) );
    }

    glDepthMask( POLY1_DEPTH_WRITE(poly1) ? GL_TRUE : GL_FALSE );
}

/**
 * Setup the texture/shading settings (TSP) which vary between mod/unmod modes.
 */
static void render_set_tsp_context( uint32_t poly1, uint32_t poly2 )
{
#ifdef HAVE_OPENGL_FIXEDFUNC
    glShadeModel( POLY1_SHADE_MODEL(poly1) );

    if( !have_shaders ) {
        if( POLY1_TEXTURED(poly1) ) {
            if( POLY2_TEX_BLEND(poly2) == 2 )
                glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
            else
                glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
   
         }

         switch( POLY2_FOG_MODE(poly2) ) {
         case PVR2_POLY_FOG_LOOKUP:
             glFogfv( GL_FOG_COLOR, pvr2_scene.fog_lut_colour );
             break;
         case PVR2_POLY_FOG_VERTEX:
             glFogfv( GL_FOG_COLOR, pvr2_scene.fog_vert_colour );
             break;
         }
     }
#endif

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
static void render_set_context( uint32_t *context, gboolean set_depth )
{
    render_set_base_context(context[0], set_depth);
    render_set_tsp_context(context[0],context[1]);
}

static inline void gl_draw_vertexes( struct polygon_struct *poly )
{
    do {
        glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index, poly->vertex_count);
        poly = poly->sub_next;
    } while( poly != NULL );
}

static inline void gl_draw_mod_vertexes( struct polygon_struct *poly )
{
    do {
        glDrawArrays(GL_TRIANGLE_STRIP, poly->mod_vertex_index, poly->vertex_count);
        poly = poly->sub_next;
    } while( poly != NULL );
}

static void gl_render_poly( struct polygon_struct *poly, gboolean set_depth)
{
    if( poly->vertex_count == 0 )
        return; /* Culled */

    bind_texture(poly->tex_id);
    if( poly->mod_vertex_index == -1 ) {
        render_set_context( poly->context, set_depth );
        gl_draw_vertexes(poly);
    }  else {
        glEnable( GL_STENCIL_TEST );
        render_set_base_context( poly->context[0], set_depth );
        render_set_tsp_context( poly->context[0], poly->context[1] );
        glStencilFunc(GL_EQUAL, 0, 2);
        gl_draw_vertexes(poly);

        if( pvr2_scene.shadow_mode == SHADOW_FULL ) {
            bind_texture(poly->mod_tex_id);
            render_set_tsp_context( poly->context[0], poly->context[3] );
        }
        glStencilFunc(GL_EQUAL, 2, 2);
        gl_draw_mod_vertexes(poly);
        glDisable( GL_STENCIL_TEST );
    }
}


static void gl_render_modifier_polygon( struct polygon_struct *poly, uint32_t tile_bounds[] )
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
    
    if( poly->vertex_count == 0 )
        return; /* Culled */

    gl_draw_vertexes(poly);


    
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

static void gl_render_bkgnd( struct polygon_struct *poly )
{
    bind_texture(poly->tex_id);
    render_set_tsp_context( poly->context[0], poly->context[1] );
    glDisable( GL_DEPTH_TEST );
    glBlendFunc( GL_ONE, GL_ZERO );
    gl_draw_vertexes(poly);
    glEnable( GL_DEPTH_TEST );
}

void gl_render_triangle( struct polygon_struct *poly, int index )
{
    bind_texture(poly->tex_id);
    render_set_tsp_context( poly->context[0], poly->context[1] );
    glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index + index, 3 );

}

void gl_render_tilelist( pvraddr_t tile_entry, gboolean set_depth )
{
    tileentryiter list;

    FOREACH_TILEENTRY(list, tile_entry) {
        struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[TILEENTRYITER_POLYADDR(list)];
        if( poly != NULL ) {
            do {
                gl_render_poly(poly, set_depth);
                poly = poly->next;
            } while( list.strip_count-- > 0 );
        }
    }
}

/**
 * Render the tilelist with depthbuffer updates only.
 */
static void gl_render_tilelist_depthonly( pvraddr_t tile_entry )
{
    tileentryiter list;

    FOREACH_TILEENTRY(list, tile_entry) {
        struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[TILEENTRYITER_POLYADDR(list)];
        if( poly != NULL ) {
            do {
                render_set_base_context(poly->context[0],TRUE);
                gl_draw_vertexes(poly);
                poly = poly->next;
            } while( list.strip_count-- > 0 );
        }
    }
}

static void gl_render_modifier_tilelist( pvraddr_t tile_entry, uint32_t tile_bounds[] )
{
    tileentryiter list;

    FOREACH_TILEENTRY(list, tile_entry ) {
        struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[TILEENTRYITER_POLYADDR(list)];
        if( poly != NULL ) {
            do {
                gl_render_modifier_polygon( poly, tile_bounds );
                poly = poly->next;
            } while( list.strip_count-- > 0 );
        }
    }
}


#ifdef HAVE_OPENGL_FIXEDFUNC
void pvr2_scene_setup_fixed( GLfloat *viewMatrix )
{
    glLoadMatrixf(viewMatrix);
    glEnable( GL_DEPTH_TEST );
    
    glEnable( GL_FOG );
    glFogi(GL_FOG_COORDINATE_SOURCE_EXT, GL_FOG_COORDINATE_EXT);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0);
    glFogf(GL_FOG_END, 1.0);

    glEnable( GL_ALPHA_TEST );
    glAlphaFunc( GL_GEQUAL, 0 );

    glEnable( GL_COLOR_SUM );

    glEnableClientState( GL_VERTEX_ARRAY );
    glEnableClientState( GL_COLOR_ARRAY );
    glEnableClientState( GL_TEXTURE_COORD_ARRAY );
    glEnableClientState( GL_SECONDARY_COLOR_ARRAY );
    glEnableClientState( GL_FOG_COORDINATE_ARRAY_EXT );

    /* Vertex array pointers */
    glVertexPointer(3, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].x);
    glColorPointer(4, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].rgba[0]);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].u);
    glSecondaryColorPointerEXT(3, GL_FLOAT, sizeof(struct vertex_struct), pvr2_scene.vertex_array[0].offset_rgba );
    glFogCoordPointerEXT(GL_FLOAT, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].offset_rgba[3] );
}

void pvr2_scene_set_alpha_fixed( float alphaRef )
{
    glAlphaFunc( GL_GEQUAL, alphaRef );
}

void pvr2_scene_cleanup_fixed()
{
    glDisable( GL_COLOR_SUM );
    glDisable( GL_FOG );
    glDisable( GL_ALPHA_TEST );
    glDisable( GL_DEPTH_TEST );

    glDisableClientState( GL_VERTEX_ARRAY );
    glDisableClientState( GL_COLOR_ARRAY );
    glDisableClientState( GL_TEXTURE_COORD_ARRAY );
    glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
    glDisableClientState( GL_FOG_COORDINATE_ARRAY_EXT );

}
#else
void pvr2_scene_setup_fixed( GLfloat *viewMatrix )
{
}
void pvr2_scene_set_alpha_fixed( float alphaRef )
{
}
void pvr2_scene_cleanup_fixed()
{
}
#endif

void pvr2_scene_setup_shader( GLfloat *viewMatrix )
{
    glEnable( GL_DEPTH_TEST );

    glsl_use_pvr2_shader();
    glsl_set_pvr2_shader_view_matrix(viewMatrix);
    glsl_set_pvr2_shader_fog_colour1(pvr2_scene.fog_vert_colour);
    glsl_set_pvr2_shader_fog_colour2(pvr2_scene.fog_lut_colour);
    glsl_set_pvr2_shader_in_vertex_vec3_pointer(&pvr2_scene.vertex_array[0].x, sizeof(struct vertex_struct));
    glsl_set_pvr2_shader_in_colour_pointer(&pvr2_scene.vertex_array[0].rgba[0], sizeof(struct vertex_struct));
    glsl_set_pvr2_shader_in_colour2_pointer(&pvr2_scene.vertex_array[0].offset_rgba[0], sizeof(struct vertex_struct));
    glsl_set_pvr2_shader_in_texcoord_pointer(&pvr2_scene.vertex_array[0].u, sizeof(struct vertex_struct));
    glsl_set_pvr2_shader_alpha_ref(0.0);
    glsl_set_pvr2_shader_primary_texture(0);
    glsl_set_pvr2_shader_palette_texture(1);
}

void pvr2_scene_cleanup_shader( )
{
    glsl_clear_shader();

    glDisable( GL_DEPTH_TEST );
}

void pvr2_scene_set_alpha_shader( float alphaRef )
{
    glsl_set_pvr2_shader_alpha_ref(alphaRef);
}

/**
 * Render the currently defined scene in pvr2_scene
 */
void pvr2_scene_render( render_buffer_t buffer )
{
    /* Scene setup */
    struct timeval start_tv, tex_tv, end_tv;
    int i;
    GLfloat viewMatrix[16];
    uint32_t clip_bounds[4];


    gettimeofday(&start_tv, NULL);
    display_driver->set_render_target(buffer);
    pvr2_check_palette_changed();
    pvr2_scene_load_textures();
    currentTexId = -1;

    gettimeofday( &tex_tv, NULL );
    uint32_t ms = (tex_tv.tv_sec - start_tv.tv_sec) * 1000 +
    (tex_tv.tv_usec - start_tv.tv_usec)/1000;
    DEBUG( "Texture load in %dms", ms );

    float alphaRef = ((float)(MMIO_READ(PVR2, RENDER_ALPHA_REF)&0xFF)+1)/256.0;
    float nearz = pvr2_scene.bounds[4];
    float farz = pvr2_scene.bounds[5];
    if( nearz == farz ) {
        farz*= 4.0;
    }

    /* Generate integer clip boundaries */
    for( i=0; i<4; i++ ) {
        clip_bounds[i] = (uint32_t)pvr2_scene.bounds[i];
    }

    defineOrthoMatrix(viewMatrix, pvr2_scene.buffer_width, pvr2_scene.buffer_height, -farz, -nearz);

    if( have_shaders ) {
        pvr2_scene_setup_shader(viewMatrix);
    } else {
        pvr2_scene_setup_fixed(viewMatrix);
    }


    /* Clear the buffer (FIXME: May not want always want to do this) */
    glDisable( GL_SCISSOR_TEST );
    glDepthMask( GL_TRUE );
    glStencilMask( 0x03 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    /* Render the background */
    gl_render_bkgnd( pvr2_scene.bkgnd_poly );

    glEnable( GL_SCISSOR_TEST );
    glEnable( GL_TEXTURE_2D );

    struct tile_segment *segment;

#define FOREACH_SEGMENT(segment) \
    segment = pvr2_scene.segment_list; \
    do { \
        int tilex = SEGMENT_X(segment->control); \
        int tiley = SEGMENT_Y(segment->control); \
        \
        uint32_t tile_bounds[4] = { tilex << 5, (tilex+1)<<5, tiley<<5, (tiley+1)<<5 }; \
        if( !clip_tile_bounds(tile_bounds, clip_bounds) ) { \
            continue; \
        }
#define END_FOREACH_SEGMENT() \
    } while( !IS_LAST_SEGMENT(segment++) );
#define CLIP_TO_SEGMENT() \
    glScissor( tile_bounds[0], pvr2_scene.buffer_height-tile_bounds[3], tile_bounds[1]-tile_bounds[0], tile_bounds[3] - tile_bounds[2] )

    /* Build up the opaque stencil map */
    if( display_driver->capabilities.stencil_bits >= 2 ) {
        glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
        FOREACH_SEGMENT(segment)
            if( IS_NONEMPTY_TILE_LIST(segment->opaquemod_ptr) ) {
                CLIP_TO_SEGMENT();
                gl_render_tilelist_depthonly(segment->opaque_ptr);
            }
        END_FOREACH_SEGMENT()

        glEnable( GL_STENCIL_TEST );
        glStencilFunc( GL_ALWAYS, 0, 1 );
        glStencilOp( GL_KEEP,GL_INVERT, GL_KEEP );
        glStencilMask( 0x01 );
        glDepthFunc( GL_LEQUAL );
        glDepthMask( GL_FALSE );
        FOREACH_SEGMENT(segment)
            if( IS_NONEMPTY_TILE_LIST(segment->opaquemod_ptr) ) {
                CLIP_TO_SEGMENT();
                gl_render_modifier_tilelist(segment->opaquemod_ptr, tile_bounds);
            }
        END_FOREACH_SEGMENT()
        glDepthMask( GL_TRUE );
        glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
        glDisable( GL_SCISSOR_TEST );
        glClear( GL_DEPTH_BUFFER_BIT );
        glEnable( GL_SCISSOR_TEST );
        glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
    }

    /* Render the opaque polygons */
    FOREACH_SEGMENT(segment)
        CLIP_TO_SEGMENT();
        gl_render_tilelist(segment->opaque_ptr,TRUE);
    END_FOREACH_SEGMENT()
    glDisable( GL_STENCIL_TEST );

    /* Render the punch-out polygons */
    if( have_shaders )
        pvr2_scene_set_alpha_shader(alphaRef);
    else
        pvr2_scene_set_alpha_fixed(alphaRef);
    glDepthFunc(GL_GEQUAL);
    FOREACH_SEGMENT(segment)
        CLIP_TO_SEGMENT();
        gl_render_tilelist(segment->punchout_ptr, FALSE );
    END_FOREACH_SEGMENT()
    if( have_shaders )
        pvr2_scene_set_alpha_shader(0.0);
    else
        pvr2_scene_set_alpha_fixed(0.0);

    /* Render the translucent polygons */
    FOREACH_SEGMENT(segment)
        if( IS_NONEMPTY_TILE_LIST(segment->trans_ptr) ) {
            CLIP_TO_SEGMENT();
            if( pvr2_scene.sort_mode == SORT_NEVER || 
                    (pvr2_scene.sort_mode == SORT_TILEFLAG && (segment->control&SEGMENT_SORT_TRANS))) {
                gl_render_tilelist(segment->trans_ptr, TRUE);
            } else {
                render_autosort_tile(segment->trans_ptr, RENDER_NORMAL );
            }
        }
    END_FOREACH_SEGMENT()

    glDisable( GL_SCISSOR_TEST );

    if( have_shaders ) {
        pvr2_scene_cleanup_shader();
    } else {
        pvr2_scene_cleanup_fixed();
    }

    pvr2_scene_finished();

    gettimeofday( &end_tv, NULL );
    ms = (end_tv.tv_sec - tex_tv.tv_sec) * 1000 +
    (end_tv.tv_usec - tex_tv.tv_usec)/1000;
    DEBUG( "Scene render in %dms", ms );
}
