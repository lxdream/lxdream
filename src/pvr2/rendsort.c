/**
 * $Id$
 *
 * PVR2 renderer routines for depth sorted polygons
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
#include <sys/time.h>
#include <string.h>
#include <assert.h>
#include "pvr2/pvr2.h"
#include "pvr2/scene.h"
#include "asic.h"

#define MIN3( a,b,c ) ((a) < (b) ? ( (a) < (c) ? (a) : (c) ) : ((b) < (c) ? (b) : (c)) )
#define MAX3( a,b,c ) ((a) > (b) ? ( (a) > (c) ? (a) : (c) ) : ((b) > (c) ? (b) : (c)) )

struct sort_triangle {
    struct polygon_struct *poly;
    int triangle_num; // triangle number in the poly, from 0
    float maxz;
};

#define SENTINEL 0xDEADBEEF

/**
 * Count the number of triangles in the list starting at the given 
 * pvr memory address.
 */
int sort_count_triangles( pvraddr_t tile_entry ) {
    uint32_t *tile_list = (uint32_t *)(video_base+tile_entry);
    int count = 0;
    while(1) {
	uint32_t entry = *tile_list++;
	if( entry >> 28 == 0x0F ) {
	    break;
	} else if( entry >> 28 == 0x0E ) {
	    tile_list = (uint32_t *)(video_base+(entry&0x007FFFFF));
	} else if( entry >> 29 == 0x04 ) { /* Triangle array */
	    count += ((entry >> 25) & 0x0F)+1;
	} else if( entry >> 29 == 0x05 ) { /* Quad array */
	    count += ((((entry >> 25) & 0x0F)+1)<<1);
	} else { /* Polygon */
	    int i;
	    for( i=0; i<6; i++ ) {
		if( entry & (0x40000000>>i) ) {
		    count++;
		}
	    }
	}
    }
    return count;
}

/**
 * Extract a triangle list from the tile (basically indexes into the polygon list, plus
 * computing maxz while we go through it
 */
int sort_extract_triangles( pvraddr_t tile_entry, struct sort_triangle *triangles )
{
    uint32_t *tile_list = (uint32_t *)(video_base+tile_entry);
    int count = 0;
    while(1) {
	uint32_t entry = *tile_list++;
	if( entry >> 28 == 0x0F ) {
	    break;
	} else if( entry >> 28 == 0x0E ) {
	    tile_list = (uint32_t *)(video_base+(entry&0x007FFFFF));
	} else {
	    uint32_t poly_addr = entry & 0x000FFFFF;
	    int is_modified = entry & 0x01000000;
	    int vertex_length = (entry >> 21) & 0x07;
	    int context_length = 3;
	    if( is_modified && pvr2_scene.full_shadow ) {
		context_length = 5;
		vertex_length *= 2 ;
	    }
	    vertex_length += 3;

	    if( (entry & 0xE0000000) == 0x80000000 ) {
		/* Triangle(s) */
		int strip_count = ((entry >> 25) & 0x0F)+1;
		int polygon_length = 3 * vertex_length + context_length;
		int i;
		for( i=0; i<strip_count; i++ ) {
		    struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[poly_addr];
		    triangles[count].poly = poly;
		    triangles[count].triangle_num = 0;
		    triangles[count].maxz = MAX3( pvr2_scene.vertex_array[poly->vertex_index].z,
						  pvr2_scene.vertex_array[poly->vertex_index+1].z,
						  pvr2_scene.vertex_array[poly->vertex_index+2].z );
		    poly_addr += polygon_length;
		    count++;
		}
	    } else if( (entry & 0xE0000000) == 0xA0000000 ) {
		/* Quad(s) */
		int strip_count = ((entry >> 25) & 0x0F)+1;
		int polygon_length = 4 * vertex_length + context_length;
		int i;
		for( i=0; i<strip_count; i++ ) {
		    struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[poly_addr];
		    triangles[count].poly = poly;
		    triangles[count].triangle_num = 0;
		    triangles[count].maxz = MAX3( pvr2_scene.vertex_array[poly->vertex_index].z,
						  pvr2_scene.vertex_array[poly->vertex_index+1].z,
						  pvr2_scene.vertex_array[poly->vertex_index+2].z );
		    count++;
		    triangles[count].poly = poly;
		    triangles[count].triangle_num = 1;
		    triangles[count].maxz = MAX3( pvr2_scene.vertex_array[poly->vertex_index+1].z,
						  pvr2_scene.vertex_array[poly->vertex_index+2].z,
						  pvr2_scene.vertex_array[poly->vertex_index+3].z );
		    count++;
		    poly_addr += polygon_length;
		}
	    } else {
		/* Polygon */
		int i;
		struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[poly_addr];
		for( i=0; i<6; i++ ) {
		    if( entry & (0x40000000>>i) ) {
			triangles[count].poly = poly;
			triangles[count].triangle_num = i;
			triangles[count].maxz = MAX3( pvr2_scene.vertex_array[poly->vertex_index+i].z,
						      pvr2_scene.vertex_array[poly->vertex_index+i+1].z,
						      pvr2_scene.vertex_array[poly->vertex_index+i+2].z );
			count++;
		    }
		}
	    }
	}
    }
    return count;
}

void sort_render_triangles( struct sort_triangle *triangles, int num_triangles,
			    int render_mode )
{
    int i;
    for( i=0; i<num_triangles; i++ ) {
	struct polygon_struct *poly = triangles[i].poly;
	if( poly->tex_id != -1 ) {
	    glBindTexture(GL_TEXTURE_2D, poly->tex_id);
	}
	render_set_context( poly->context, RENDER_NORMAL );
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_GEQUAL);
	/* Fix cull direction */
	if( triangles[i].triangle_num & 1 ) {
	    glCullFace(GL_FRONT);
	} else {
	    glCullFace(GL_BACK);
	}
	
	glDrawArrays(GL_TRIANGLE_STRIP, poly->vertex_index + triangles[i].triangle_num, 3 );
    }
}

int compare_triangles( const void *a, const void *b ) 
{
    const struct sort_triangle *tri1 = a;
    const struct sort_triangle *tri2 = b;
    return tri2->maxz - tri1->maxz;
}

void sort_triangles( struct sort_triangle *triangles, int num_triangles )
{
    qsort( triangles, num_triangles, sizeof(struct sort_triangle), compare_triangles );
} 
			
void render_autosort_tile( pvraddr_t tile_entry, int render_mode ) 
{
    int num_triangles = sort_count_triangles(tile_entry);
    if( num_triangles == 0 ) {
	return; /* nothing to do */
    } else if( num_triangles == 1 ) { /* Triangle can hardly overlap with itself */
	gl_render_tilelist(tile_entry);
    } else { /* Ooh boy here we go... */
	struct sort_triangle triangles[num_triangles+1];
	// Reserve space for num_triangles / 2 * 4 vertexes (maximum possible number of
	// quad vertices)
	triangles[num_triangles].poly = (void *)SENTINEL;
	int extracted_triangles = sort_extract_triangles(tile_entry, triangles);
	assert( extracted_triangles == num_triangles );
	sort_triangles( triangles, num_triangles );
	sort_render_triangles(triangles, num_triangles, render_mode);
	glCullFace(GL_BACK);
	assert( triangles[num_triangles].poly == (void *)SENTINEL );
    }
}
