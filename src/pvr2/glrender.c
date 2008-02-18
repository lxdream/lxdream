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

#include "display.h"
#include "pvr2/pvr2.h"
#include "pvr2/scene.h"

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

/**
 * Once-off call to setup the OpenGL context.
 */
void pvr2_setup_gl_context()
{
    texcache_gl_init(); // Allocate texture IDs
    glShadeModel(GL_SMOOTH);
    glCullFace( GL_BACK );
    glEnable( GL_BLEND );
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnableClientState( GL_COLOR_ARRAY );
    glEnableClientState( GL_VERTEX_ARRAY );
    glEnableClientState( GL_TEXTURE_COORD_ARRAY );
    glEnableClientState( GL_SECONDARY_COLOR_ARRAY );

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(0);
    glClearStencil(0);
}

static void gl_render_poly( struct polygon_struct *poly )
{
    render_set_context( poly->context, RENDER_NORMAL );
    glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index, poly->vertex_count );
}

static void gl_render_tilelist( pvraddr_t tile_entry )
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
	case 0x08:
	case 0x09:
	case 0x0A:
	case 0x0B:
	    strip_count = ((entry >> 25) & 0x0F)+1;
	    poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
	    while( strip_count > 0 ) {
		gl_render_poly( poly );
		poly = poly->next;
		strip_count--;
	    }
	    break;
	default:
	    poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
	    gl_render_poly( poly );
	}
    }	    
}


/**
 * Render the currently defined scene in pvr2_scene
 */
void pvr2_scene_render( render_buffer_t buffer )
{
    /* Scene setup */
    display_driver->set_render_target(buffer);
    pvr2_check_palette_changed();

    /* Setup view projection matrix */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float nearz = pvr2_scene.bounds[4];
    float farz = pvr2_scene.bounds[5];
    if( nearz == farz ) {
	farz*= 2.0;
    }
    glOrtho( 0, pvr2_scene.buffer_width, pvr2_scene.buffer_height, 0, 
	     -nearz, -farz );

    /* Clear the buffer (FIXME: May not want always want to do this) */
    glDisable( GL_SCISSOR_TEST );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    /* Setup vertex array pointers */
    glInterleavedArrays(GL_T2F_C4UB_V3F, sizeof(struct vertex_struct), pvr2_scene.vertex_array);
    glSecondaryColorPointerEXT(4, GL_UNSIGNED_BYTE, sizeof(struct vertex_struct), &pvr2_scene.vertex_array[0].offset_rgba );

    uint32_t bgplane_mode = MMIO_READ(PVR2, RENDER_BGPLANE);
    uint32_t *bgplane = pvr2_scene.pvr2_pbuf + (((bgplane_mode & 0x00FFFFFF)) >> 3) ;
    render_backplane( bgplane, pvr2_scene.buffer_width, pvr2_scene.buffer_height, bgplane_mode );
    
    glEnable( GL_SCISSOR_TEST );

    /* Process the segment list */
    struct tile_segment *segment = pvr2_scene.segment_list;
    do {
	int tilex = SEGMENT_X(segment->control);
	int tiley = SEGMENT_Y(segment->control);
	
	int tile_bounds[4] = { tilex << 5, (tilex+1)<<5, tiley<<5, (tiley+1)<<5 };
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
	    gl_render_tilelist(segment->trans_ptr);
	}
	if( IS_TILE_PTR(segment->punchout_ptr) ) {
	    gl_render_tilelist(segment->punchout_ptr);
	}
    } while( !IS_LAST_SEGMENT(segment++) );
    glDisable( GL_SCISSOR_TEST );

}
