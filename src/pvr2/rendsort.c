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
#include "pvr2/pvr2.h"
#include "asic.h"

extern char *video_base;
extern gboolean pvr2_force_fragment_alpha;

#define MIN3( a,b,c ) ((a) < (b) ? ( (a) < (c) ? (a) : (c) ) : ((b) < (c) ? (b) : (c)) )
#define MAX3( a,b,c ) ((a) > (b) ? ( (a) > (c) ? (a) : (c) ) : ((b) > (c) ? (b) : (c)) )

struct render_triangle {
    uint32_t *polygon;
    int vertex_length; /* Number of 32-bit words in vertex, or 0 for an unpacked vertex */
    float minx,miny,minz;
    float maxx,maxy,maxz;
    float *vertexes[3];
};

#define SENTINEL 0xDEADBEEF

/**
 * Count the number of triangles in the list starting at the given 
 * pvr memory address.
 */
int render_count_triangles( pvraddr_t tile_entry ) {
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

static void compute_triangle_boxes( struct render_triangle *triangle, int num_triangles )
{
    int i;
    for( i=0; i<num_triangles; i++ ) {
	triangle[i].minx = MIN3(triangle[i].vertexes[0][0],triangle[i].vertexes[1][0],triangle[i].vertexes[2][0]);
	triangle[i].maxx = MAX3(triangle[i].vertexes[0][0],triangle[i].vertexes[1][0],triangle[i].vertexes[2][0]);
	triangle[i].miny = MIN3(triangle[i].vertexes[0][1],triangle[i].vertexes[1][1],triangle[i].vertexes[2][1]);
	triangle[i].maxy = MAX3(triangle[i].vertexes[0][1],triangle[i].vertexes[1][1],triangle[i].vertexes[2][1]);
	float az = 1/triangle[i].vertexes[0][2];
	float bz = 1/triangle[i].vertexes[1][2];
	float cz = 1/triangle[i].vertexes[2][2];
	triangle[i].minz = MIN3(az,bz,cz);
	triangle[i].maxz = MAX3(az,bz,cz);
    }
}

void render_extract_triangles( pvraddr_t tile_entry, gboolean cheap_modifier_mode, 
			       struct render_triangle *triangles, int num_triangles,
			       struct vertex_unpacked *vertex_space, int render_mode )
{
    uint32_t poly_bank = MMIO_READ(PVR2,RENDER_POLYBASE);
    uint32_t *tile_list = (uint32_t *)(video_base+tile_entry);
    int count = 0;
    while(1) {
	uint32_t entry = *tile_list++;
	if( entry >> 28 == 0x0F ) {
	    break;
	} else if( entry >> 28 == 0x0E ) {
	    tile_list = (uint32_t *)(video_base+(entry&0x007FFFFF));
	} else {
	    uint32_t *polygon = (uint32_t *)(video_base + poly_bank + ((entry & 0x000FFFFF) << 2));
	    int is_modified = entry & 0x01000000;
	    int vertex_length = (entry >> 21) & 0x07;
	    int context_length = 3;
	    if( is_modified && !cheap_modifier_mode ) {
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
		    float *vertex = (float *)(polygon+context_length);
		    triangles[count].polygon = polygon;
		    triangles[count].vertex_length = vertex_length;
		    triangles[count].vertexes[0] = vertex;
		    vertex+=vertex_length;
		    triangles[count].vertexes[1] = vertex;
		    vertex+=vertex_length;
		    triangles[count].vertexes[2] = vertex;
		    polygon += polygon_length;
		    count++;
		}
	    } else if( (entry & 0xE0000000) == 0xA0000000 ) {
		/* Quad(s) */
		int strip_count = ((entry >> 25) & 0x0F)+1;
		int polygon_length = 4 * vertex_length + context_length;
		
		int i;
		for( i=0; i<strip_count; i++ ) {
		    render_unpack_quad( vertex_space, *polygon, (polygon+context_length),
					vertex_length, render_mode );
		    triangles[count].polygon = polygon;
		    triangles[count].vertex_length = 0;
		    triangles[count].vertexes[0] = (float *)vertex_space;
		    triangles[count].vertexes[1] = (float *)(vertex_space + 1);
		    triangles[count].vertexes[2] = (float *)(vertex_space + 3);
		    count++;
		    /* Preserve face direction */
		    triangles[count].polygon = polygon;
		    triangles[count].vertex_length = 0;
		    triangles[count].vertexes[0] = (float *)(vertex_space + 1);
		    triangles[count].vertexes[1] = (float *)(vertex_space + 2);
		    triangles[count].vertexes[2] = (float *)(vertex_space + 3);
		    count++;
		    vertex_space += 4;
		    polygon += polygon_length;
		}
	    } else {
		/* Polygon */
		int i;
		float *vertex = (float *)polygon+context_length;
		for( i=0; i<6; i++ ) {
		    if( entry & (0x40000000>>i) ) {
			triangles[count].polygon = polygon;
			triangles[count].vertex_length = vertex_length;
			if( i&1 ) {
			    triangles[count].vertexes[0] = vertex + vertex_length;
			    triangles[count].vertexes[1] = vertex;
			    triangles[count].vertexes[2] = vertex + (vertex_length<<1);
			} else {
			    triangles[count].vertexes[0] = vertex;
			    triangles[count].vertexes[1] = vertex + vertex_length;
			    triangles[count].vertexes[2] = vertex + (vertex_length<<1);
			}
			count++;
		    }
		    vertex += vertex_length;
		}
	    }
	}
    }
    if( count != num_triangles ) {
	ERROR( "Extracted triangles do not match expected count!" );
    }
}

void render_triangles( struct render_triangle *triangles, int num_triangles,
		       int render_mode )
{
    int i;
    for( i=0; i<num_triangles; i++ ) {
	render_set_context( triangles[i].polygon, render_mode );
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GEQUAL);
	if( triangles[i].vertex_length == 0 ) {
	    render_unpacked_vertex_array( *triangles[i].polygon, (struct vertex_unpacked **)triangles[i].vertexes, 3 );
	} else {
	    render_vertex_array( *triangles[i].polygon, (uint32_t **)triangles[i].vertexes, 3,
				 triangles[i].vertex_length, render_mode );
	}
    }


}

int compare_triangles( const void *a, const void *b ) 
{
    const struct render_triangle *tri1 = a;
    const struct render_triangle *tri2 = b;
    if( tri1->minz < tri2->minz ) {  
	return 1; // No these _aren't_ back to front...
    } else if( tri1->minz > tri2->minz ) {
	return -1;
    } else {
	return 0;
    }
}

void sort_triangles( struct render_triangle *triangles, int num_triangles )
{
    qsort( triangles, num_triangles, sizeof(struct render_triangle), compare_triangles );
} 
			
void render_autosort_tile( pvraddr_t tile_entry, int render_mode, gboolean cheap_modifier_mode ) 
{
    int num_triangles = render_count_triangles(tile_entry);
    if( num_triangles == 0 ) {
	return; /* nothing to do */
    } else if( num_triangles == 1 ) { /* Triangle can hardly overlap with itself */
	render_tile( tile_entry, render_mode, cheap_modifier_mode );
    } else { /* Ooh boy here we go... */
	struct render_triangle triangles[num_triangles+1];
	struct vertex_unpacked vertex_space[num_triangles << 1]; 
	// Reserve space for num_triangles / 2 * 4 vertexes (maximum possible number of
	// quad vertices)
	triangles[num_triangles].polygon = (void *)SENTINEL;
	render_extract_triangles(tile_entry, cheap_modifier_mode, triangles, num_triangles, vertex_space, render_mode);
	compute_triangle_boxes(triangles, num_triangles);
	sort_triangles( triangles, num_triangles );
	render_triangles(triangles, num_triangles, render_mode);
	if( triangles[num_triangles].polygon != (void *)SENTINEL ) {
	    fprintf( stderr, "Triangle overflow in render_autosort_tile!" );
	}
    }
}
