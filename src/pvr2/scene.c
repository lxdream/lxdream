/**
 * $Id$
 *
 * Manage the internal vertex/polygon buffers and scene data structure. 
 * Where possible this uses VBOs for the vertex + index data.
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
#include <string.h>
#include "lxdream.h"
#include "display.h"
#include "pvr2/pvr2.h"
#include "pvr2/glutil.h"
#include "pvr2/scene.h"

#define VBO_EXT_STRING "GL_ARB_vertex_buffer_object"
#define PBO_EXT_STRING "GL_ARB_pixel_buffer_object"

static inline uint32_t bgra_to_rgba(uint32_t bgra)
{
    return (bgra&0xFF00FF00) | ((bgra&0x00FF0000)>>16) | ((bgra&0x000000FF)<<16);
}

struct pvr2_scene_struct pvr2_scene;

static gboolean vbo_init = FALSE;
static gboolean vbo_supported = FALSE;

/**
 * Test for VBO support, and allocate all the system memory needed for the
 * temporary structures. GL context must have been initialized before this
 * point.
 */
void pvr2_scene_init()
{
    if( !vbo_init ) {
	if( isGLExtensionSupported(VBO_EXT_STRING) ) {
	    vbo_supported = TRUE;
	    pvr2_scene.vbo_id = 1;
	}
	pvr2_scene.vertex_array = NULL;
	pvr2_scene.vertex_array_size = 0;
	pvr2_scene.poly_array = g_malloc( MAX_POLY_BUFFER_SIZE );
	pvr2_scene.buf_to_poly_map = g_malloc0( BUF_POLY_MAP_SIZE );
	vbo_init = TRUE;
    }
}

/**
 * Clear the scene data structures in preparation for fresh data
 */
void pvr2_scene_reset()
{
    pvr2_scene.poly_count = 0;
    pvr2_scene.vertex_count = 0;
    memset( pvr2_scene.buf_to_poly_map, 0, BUF_POLY_MAP_SIZE );
}

void pvr2_scene_shutdown()
{
    if( vbo_supported ) {
	glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	glDeleteBuffersARB( 1, &pvr2_scene.vbo_id );
	pvr2_scene.vbo_id = 0;
    } else {
	g_free( pvr2_scene.vertex_array );
	pvr2_scene.vertex_array = NULL;
    }
    g_free( pvr2_scene.poly_array );
    g_free( pvr2_scene.buf_to_poly_map );
    vbo_init = FALSE;
}

void *vertex_buffer_map()
{
    glGetError();
    uint32_t size = pvr2_scene.vertex_count * sizeof(struct vertex_struct);
    if( vbo_supported ) {
	glBindBufferARB( GL_ARRAY_BUFFER_ARB, pvr2_scene.vbo_id );
	assert( glGetError() == 0 );
	if( size > pvr2_scene.vertex_array_size ) {
	    glBufferDataARB( GL_ARRAY_BUFFER_ARB, size, NULL, GL_DYNAMIC_DRAW_ARB );
	    int status = glGetError();
	    if( status != 0 ) {
		fprintf( stderr, "Error %08X allocating vertex buffer\n", status );
		abort();
	    }
	    pvr2_scene.vertex_array_size = size;
	}
	pvr2_scene.vertex_array = glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
	assert(pvr2_scene.vertex_array != NULL );
    } else {
	if( size > pvr2_scene.vertex_array_size ) {
	    pvr2_scene.vertex_array = g_realloc( pvr2_scene.vertex_array, size );
	}
    }
    return pvr2_scene.vertex_array;
}

gboolean vertex_buffer_unmap()
{
    if( vbo_supported ) {
	pvr2_scene.vertex_array = NULL;
	return glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
    } else {
	return TRUE;
    }
}

static struct polygon_struct *scene_add_polygon( pvraddr_t poly_idx, int vertex_count,
							 gboolean is_modified ) 
{
    int vert_mul = is_modified ? 2 : 1;

    if( pvr2_scene.buf_to_poly_map[poly_idx] != NULL ) {
	if( vertex_count > pvr2_scene.buf_to_poly_map[poly_idx]->vertex_count ) {
	    pvr2_scene.vertex_count += (vertex_count - pvr2_scene.buf_to_poly_map[poly_idx]->vertex_count) * vert_mul;
	    pvr2_scene.buf_to_poly_map[poly_idx]->vertex_count = vertex_count;
	}
	return pvr2_scene.buf_to_poly_map[poly_idx];
    } else {
	struct polygon_struct *poly = &pvr2_scene.poly_array[pvr2_scene.poly_count++];
	poly->context = (uint32_t *)(video_base + MMIO_READ(PVR2,RENDER_POLYBASE) + (poly_idx<<2));
	poly->vertex_count = vertex_count;
	poly->vertex_index = -1;
	poly->next = NULL;
	pvr2_scene.buf_to_poly_map[poly_idx] = poly;
	pvr2_scene.vertex_count += (vertex_count * vert_mul);
	return poly;
    }
}

/**
 * Decode a single PVR2 renderable vertex (opaque/trans/punch-out, but not shadow
 * volume)
 * @param vert Pointer to output vertex structure
 * @param poly1 First word of polygon context (needed to understand vertex)
 * @param poly2 Second word of polygon context
 * @param pvr2_data Pointer to raw pvr2 vertex data (in VRAM)
 * @param modify_offset Offset in 32-bit words to the tex/color data. 0 for
 *        the normal vertex, half the vertex length for the modified vertex.
 */
static void pvr2_decode_render_vertex( struct vertex_struct *vert, uint32_t poly1, 
				       uint32_t poly2, uint32_t *pvr2_data, 
				       int modify_offset )
{
    gboolean force_alpha = !POLY2_ALPHA_ENABLE(poly2);
    union pvr2_data_type {
	uint32_t *ival;
	float *fval;
    } data;

    data.ival = pvr2_data;
    
    vert->x = *data.fval++;
    vert->y = *data.fval++;

    float z = *data.fval++;
    if( z > pvr2_scene.bounds[5] ) {
	pvr2_scene.bounds[5] = z;
    } else if( z < pvr2_scene.bounds[4] && z != 0 ) {
	pvr2_scene.bounds[4] = z;
    }
    vert->z = z;
    data.ival += modify_offset;

    
    if( POLY1_TEXTURED(poly1) ) {
	if( POLY1_UV16(poly1) ) {
	    vert->u = halftofloat( *data.ival>>16 );
	    vert->v = halftofloat( *data.ival );
	    data.ival++;
	} else {
	    vert->u = *data.fval++;
	    vert->v = *data.fval++;
	}
	if( POLY2_TEX_BLEND(poly2) == 1 ) {
	    force_alpha = TRUE;
	}
    }
    if( force_alpha ) {
	vert->rgba = bgra_to_rgba((*data.ival++) | 0xFF000000);
	if( POLY1_SPECULAR(poly1) ) {
	    vert->offset_rgba = bgra_to_rgba((*data.ival++) | 0xFF000000);
	}
    } else {
	vert->rgba = bgra_to_rgba(*data.ival++);
	if( POLY1_SPECULAR(poly1) ) {
	    vert->offset_rgba = bgra_to_rgba(*data.ival++);
	}
    }
}

/**
 * Compute texture, colour, and z values for a result point by interpolating from
 * a set of 3 input points. The result point must define its x,y.
 */
static void scene_compute_vertex( struct vertex_struct *result, 
					  struct vertex_struct *input,
					  gboolean is_solid_shaded )
{
    int i;
    float sx = input[2].x - input[1].x;
    float sy = input[2].y - input[1].y;
    float tx = input[0].x - input[1].x;
    float ty = input[0].y - input[1].y;

    float detxy = ((sy) * (tx)) - ((ty) * (sx));
    if( detxy == 0 ) {
	result->z = input[2].z;
	result->u = input[2].u;
	result->v = input[2].v;
	result->rgba = input[2].rgba;
	result->offset_rgba = input[2].offset_rgba;
	return;
    }
    float t = ((result->x - input[1].x) * sy -
	       (result->y - input[1].y) * sx) / detxy;
    float s = ((result->y - input[1].y) * tx -
	       (result->x - input[1].x) * ty) / detxy;

    float sz = input[2].z - input[1].z;
    float tz = input[0].z - input[1].z;
    float su = input[2].u - input[1].u;
    float tu = input[0].u - input[1].u;
    float sv = input[2].v - input[1].v;
    float tv = input[0].v - input[1].v;

    float rz = input[1].z + (t*tz) + (s*sz);
    if( rz > pvr2_scene.bounds[5] ) {
	pvr2_scene.bounds[5] = rz;
    } else if( rz < pvr2_scene.bounds[4] ) {
	pvr2_scene.bounds[4] = rz; 
    }
    result->z = rz;
    result->u = input[1].u + (t*tu) + (s*su);
    result->v = input[1].v + (t*tv) + (s*sv);

    if( is_solid_shaded ) {
	result->rgba = input[2].rgba;
	result->offset_rgba = input[2].offset_rgba;
    } else {
	uint8_t *rgba0 = (uint8_t *)&input[0].rgba;
	uint8_t *rgba1 = (uint8_t *)&input[1].rgba;
	uint8_t *rgba2 = (uint8_t *)&input[2].rgba;
	uint8_t *rgba3 = (uint8_t *)&result->rgba;
	for( i=0; i<8; i++ ) { // note: depends on rgba & offset_rgba being adjacent
	    float tc = *rgba0++ - *rgba1;
	    float sc = *rgba2++ - *rgba1;
	    float rc = *rgba1++ + (t*tc) + (s*sc);
	    if( rc < 0 ) {
		rc = 0;
	    } else if( rc > 255 ) {
		rc = 255;
	    }
	    *rgba3++ = rc;
	}
    }    

}

static void scene_add_vertexes( pvraddr_t poly_idx, int vertex_length,
					gboolean is_modified )
{
    struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[poly_idx];
    uint32_t *ptr = &pvr2_scene.pvr2_pbuf[poly_idx];
    uint32_t *context = ptr;
    unsigned int i;

    assert( poly != NULL );
    if( poly->vertex_index == -1 ) {
	ptr += (is_modified ? 5 : 3 );
	poly->vertex_index = pvr2_scene.vertex_index;
	
	assert( pvr2_scene.vertex_index + poly->vertex_count <= pvr2_scene.vertex_count );
	for( i=0; i<poly->vertex_count; i++ ) {
	    pvr2_decode_render_vertex( &pvr2_scene.vertex_array[pvr2_scene.vertex_index++], context[0], context[1], ptr, 0 );
	    ptr += vertex_length;
	}
	if( is_modified ) {
	    int mod_offset = (vertex_length - 3)>>1;
	    ptr = &pvr2_scene.pvr2_pbuf[poly_idx] + 5;
	    poly->mod_vertex_index = pvr2_scene.vertex_index;
	    for( i=0; i<poly->vertex_count; i++ ) {
		pvr2_decode_render_vertex( &pvr2_scene.vertex_array[pvr2_scene.vertex_index++], context[0], context[3], ptr, mod_offset );
		ptr += vertex_length;
	    }
	}
    }
}

static void scene_add_quad_vertexes( pvraddr_t poly_idx, int vertex_length, 
					     gboolean is_modified )
{
    struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[poly_idx];
    uint32_t *ptr = &pvr2_scene.pvr2_pbuf[poly_idx];
    uint32_t *context = ptr;
    unsigned int i;

    if( poly->vertex_index == -1 ) {
	// Construct it locally and copy to the vertex buffer, as the VBO is 
	// allowed to be horribly slow for reads (ie it could be direct-mapped
	// vram).
	struct vertex_struct quad[4];
	
	assert( poly != NULL );
	ptr += (is_modified ? 5 : 3 );
	poly->vertex_index = pvr2_scene.vertex_index;
	for( i=0; i<4; i++ ) {
	    pvr2_decode_render_vertex( &quad[i], context[0], context[1], ptr, 0 );
	    ptr += vertex_length;
	}
	scene_compute_vertex( &quad[3], &quad[0], !POLY1_GOURAUD_SHADED(context[0]) );
	// Swap last two vertexes (quad arrangement => tri strip arrangement)
	memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index], quad, sizeof(struct vertex_struct)*2 );
	memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index+2], &quad[3], sizeof(struct vertex_struct) );
	memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index+3], &quad[2], sizeof(struct vertex_struct) );
	pvr2_scene.vertex_index += 4;
	
	if( is_modified ) {
	    int mod_offset = (vertex_length - 3)>>1;
	    ptr = &pvr2_scene.pvr2_pbuf[poly_idx] + 5;
	    poly->mod_vertex_index = pvr2_scene.vertex_index;
	    for( i=0; i<4; i++ ) {
		pvr2_decode_render_vertex( &quad[4], context[0], context[3], ptr, mod_offset );
		ptr += vertex_length;
	    }
	    scene_compute_vertex( &quad[3], &quad[0], !POLY1_GOURAUD_SHADED(context[0]) );
	    memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index], quad, sizeof(struct vertex_struct)*2 );
	    memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index+2], &quad[3], sizeof(struct vertex_struct) );
	    memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index+3], &quad[2], sizeof(struct vertex_struct) );
	    pvr2_scene.vertex_index += 4;
	}
    }
}

static void scene_extract_polygons( pvraddr_t tile_entry )
{
    uint32_t *tile_list = (uint32_t *)(video_base+tile_entry);
    do {
	uint32_t entry = *tile_list++;
	if( entry >> 28 == 0x0F ) {
	    break;
	} else if( entry >> 28 == 0x0E ) {
	    tile_list = (uint32_t *)(video_base + (entry&0x007FFFFF));
	} else {
	    pvraddr_t polyaddr = entry&0x000FFFFF;
	    int is_modified = (entry & 0x01000000) && pvr2_scene.full_shadow;
	    int vertex_length = (entry >> 21) & 0x07;
	    int context_length = 3;
	    if( is_modified ) {
		context_length = 5;
		vertex_length <<= 1 ;
	    }
	    vertex_length += 3;
	    
	    if( (entry & 0xE0000000) == 0x80000000 ) {
		/* Triangle(s) */
		int strip_count = ((entry >> 25) & 0x0F)+1;
		int polygon_length = 3 * vertex_length + context_length;
		int i;
		struct polygon_struct *last_poly = NULL;
		for( i=0; i<strip_count; i++ ) {
		    struct polygon_struct *poly = scene_add_polygon( polyaddr, 3, is_modified );
		    polyaddr += polygon_length;
		    if( last_poly != NULL && last_poly->next == NULL ) {
			last_poly->next = poly;
		    }
		    last_poly = poly;
		}
	    } else if( (entry & 0xE0000000) == 0xA0000000 ) {
		/* Sprite(s) */
		int strip_count = ((entry >> 25) & 0x0F)+1;
		int polygon_length = 4 * vertex_length + context_length;
		int i;
		struct polygon_struct *last_poly = NULL;
		for( i=0; i<strip_count; i++ ) {
		    struct polygon_struct *poly = scene_add_polygon( polyaddr, 4, is_modified );
		    polyaddr += polygon_length;
		    if( last_poly != NULL && last_poly->next == NULL ) {
			last_poly->next = poly;
		    }
		    last_poly = poly;
		}
	    } else {
		/* Polygon */
		int i, last = -1;
		for( i=5; i>=0; i-- ) {
		    if( entry & (0x40000000>>i) ) {
			last = i;
			break;
		    }
		}
		if( last != -1 ) {
		    scene_add_polygon( polyaddr, last+3, is_modified );
		}
	    }
	}
    } while( 1 );
}

static void scene_extract_vertexes( pvraddr_t tile_entry )
{
    uint32_t *tile_list = (uint32_t *)(video_base+tile_entry);
    do {
	uint32_t entry = *tile_list++;
	if( entry >> 28 == 0x0F ) {
	    break;
	} else if( entry >> 28 == 0x0E ) {
	    tile_list = (uint32_t *)(video_base + (entry&0x007FFFFF));
	} else {
	    pvraddr_t polyaddr = entry&0x000FFFFF;
	    int is_modified = (entry & 0x01000000) && pvr2_scene.full_shadow;
	    int vertex_length = (entry >> 21) & 0x07;
	    int context_length = 3;
	    if( is_modified ) {
		context_length = 5;
		vertex_length <<=1 ;
	    }
	    vertex_length += 3;
	    
	    if( (entry & 0xE0000000) == 0x80000000 ) {
		/* Triangle(s) */
		int strip_count = ((entry >> 25) & 0x0F)+1;
		int polygon_length = 3 * vertex_length + context_length;
		int i;
		for( i=0; i<strip_count; i++ ) {
		    scene_add_vertexes( polyaddr, vertex_length, is_modified );
		    polyaddr += polygon_length;
		}
	    } else if( (entry & 0xE0000000) == 0xA0000000 ) {
		/* Sprite(s) */
		int strip_count = ((entry >> 25) & 0x0F)+1;
		int polygon_length = 4 * vertex_length + context_length;
		int i;
		for( i=0; i<strip_count; i++ ) {
		    scene_add_quad_vertexes( polyaddr, vertex_length, is_modified );
		    polyaddr += polygon_length;
		}
	    } else {
		/* Polygon */
		int i, last = -1;
		for( i=5; i>=0; i-- ) {
		    if( entry & (0x40000000>>i) ) {
			last = i;
			break;
		    }
		}
		if( last != -1 ) {
		    scene_add_vertexes( polyaddr, vertex_length, is_modified );
		}
	    }
	}
    } while( 1 );    
}

uint32_t pvr2_scene_buffer_width()
{
    return pvr2_scene.buffer_width;
}

uint32_t pvr2_scene_buffer_height()
{
    return pvr2_scene.buffer_height;
}

/**
 * Extract the current scene into the rendering structures. We run two passes
 * - first pass extracts the polygons into pvr2_scene.poly_array (finding vertex counts), 
 * second pass extracts the vertex data into the VBO/vertex array.
 *
 * Difficult to do in single pass as we don't generally know the size of a 
 * polygon for certain until we've seen all tiles containing it. It also means we
 * can count the vertexes and allocate the appropriate size VBO.
 *
 * FIXME: accesses into VRAM need to be bounds-checked properly
 */
void pvr2_scene_read( void )
{
    pvr2_scene_init();
    pvr2_scene_reset();

    pvr2_scene.bounds[0] = MMIO_READ( PVR2, RENDER_HCLIP ) & 0x03FF;
    pvr2_scene.bounds[1] = ((MMIO_READ( PVR2, RENDER_HCLIP ) >> 16) & 0x03FF) + 1;
    pvr2_scene.bounds[2] = MMIO_READ( PVR2, RENDER_VCLIP ) & 0x03FF;
    pvr2_scene.bounds[3] = ((MMIO_READ( PVR2, RENDER_VCLIP ) >> 16) & 0x03FF) + 1;
    pvr2_scene.bounds[4] = pvr2_scene.bounds[5] = MMIO_READF( PVR2, RENDER_FARCLIP );

    uint32_t *tilebuffer = (uint32_t *)(video_base + MMIO_READ( PVR2, RENDER_TILEBASE ));
    uint32_t *segment = tilebuffer;
    pvr2_scene.segment_list = (struct tile_segment *)tilebuffer;
    pvr2_scene.pvr2_pbuf = (uint32_t *)(video_base + MMIO_READ(PVR2,RENDER_POLYBASE));
    pvr2_scene.full_shadow = MMIO_READ( PVR2, RENDER_SHADOW ) & 0x100 ? FALSE : TRUE;
   
    int max_tile_x = 0;
    int max_tile_y = 0;
    int obj_config = MMIO_READ( PVR2, RENDER_OBJCFG );
    int isp_config = MMIO_READ( PVR2, RENDER_ISPCFG );

    if( (obj_config & 0x00200000) == 0 ) {
	if( isp_config & 1 ) {
	    pvr2_scene.sort_mode = SORT_NEVER;
	} else {
	    pvr2_scene.sort_mode = SORT_ALWAYS;
	}
    } else {
	pvr2_scene.sort_mode = SORT_BYFLAG;
    }

    // Pass 1: Extract polygon list 
    uint32_t control;
    int i;
    do {
	control = *segment++;
	int tile_x = SEGMENT_X(control);
	int tile_y = SEGMENT_Y(control);
	if( tile_x > max_tile_x ) {
	    max_tile_x = tile_x;
	} 
	if( tile_y > max_tile_y ) {
	    max_tile_y = tile_y;
	}
	for( i=0; i<5; i++ ) {
	    if( (*segment & NO_POINTER) == 0 ) {
		scene_extract_polygons( *segment );
	    }
	    segment++;
	}
    } while( (control & SEGMENT_END) == 0 );

    pvr2_scene.buffer_width = (max_tile_x+1)<<5;
    pvr2_scene.buffer_height = (max_tile_y+1)<<5;

    if( pvr2_scene.vertex_count > 0 ) {
	// Pass 2: Extract vertex data
	vertex_buffer_map();
	pvr2_scene.vertex_index = 0;
	segment = tilebuffer;
	do {
	    control = *segment++;
	    for( i=0; i<5; i++ ) {
		if( (*segment & NO_POINTER) == 0 ) {
		    scene_extract_vertexes( *segment );
		}
		segment++;
	    }
	} while( (control & SEGMENT_END) == 0 );
	
	vertex_buffer_unmap();
    }
}
