/**
 * $Id: rendsort.c,v 1.2 2007-01-12 10:15:06 nkeynes Exp $
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

#define MIN3( a,b,c ) ((a) < (b) ? ( (a) < (c) ? (a) : (c) ) : ((b) < (c) ? (b) : (c)) )
#define MAX3( a,b,c ) ((a) > (b) ? ( (a) > (c) ? (a) : (c) ) : ((b) > (c) ? (b) : (c)) )

struct pvr_vertex {
    float x,y,z;
    uint32_t detail[1];
};

struct render_triangle {
    uint32_t *polygon;
    int vertex_length;
    float minx,miny,minz;
    float maxx,maxy,maxz;
    float *vertexes[3];
};

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
	triangle[i].minz = MIN3(triangle[i].vertexes[0][2],triangle[i].vertexes[1][2],triangle[i].vertexes[2][2]);
	triangle[i].maxz = MAX3(triangle[i].vertexes[0][2],triangle[i].vertexes[1][2],triangle[i].vertexes[2][2]);
    }
}

void render_extract_triangles( pvraddr_t tile_entry, gboolean cheap_modifier_mode, 
			       struct render_triangle *triangles )
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
		/* Sprite(s) */
		int strip_count = ((entry >> 25) & 0x0F)+1;
		int polygon_length = 4 * vertex_length + context_length;
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
		    count++;
		    /* Preserve face direction */
		    triangles[count].polygon = polygon;
		    triangles[count].vertex_length = vertex_length;
		    triangles[count].vertexes[0] = vertex;
		    triangles[count].vertexes[1] = vertex - vertex_length;
		    triangles[count].vertexes[2] = vertex + vertex_length;
		    count++;
		    polygon += polygon_length;
		}
	    } else {
		/* Polygon */
		int i, first=-1, last = -1;
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
}

void render_triangles( struct render_triangle *triangles, int num_triangles,
		       int render_mode )
{
    int i,j, m = 0;
    for( i=0; i<num_triangles; i++ ) {
	render_set_context( triangles[i].polygon, render_mode );
	if( render_mode == RENDER_FULLMOD ) {
	    m = (triangles[i].vertex_length - 3)/2;
	}

	glBegin( GL_TRIANGLE_STRIP );
    
	for( j=0; j<3; j++ ) {
	    uint32_t *vertexes = (uint32_t *)triangles[i].vertexes[j];
	    float *vertexf = (float *)vertexes;
	    uint32_t argb;
	    if( POLY1_TEXTURED(*triangles[i].polygon) ) {
		if( POLY1_UV16(*triangles[i].polygon) ) {
		    glTexCoord2f( halftofloat(vertexes[m+3]>>16),
				  halftofloat(vertexes[m+3]) );
		    argb = vertexes[m+4];
		} else {
		    glTexCoord2f( vertexf[m+3], vertexf[m+4] );
		    argb = vertexes[m+5];
		}
	    } else {
		argb = vertexes[m+3];
	    }
	    
	    glColor4ub( (GLubyte)(argb >> 16), (GLubyte)(argb >> 8), 
			(GLubyte)argb, (GLubyte)(argb >> 24) );
	    glVertex3f( vertexf[0], vertexf[1], vertexf[2] );
	}
	glEnd();
    }


}

int compare_triangles( void *a, void *b ) 
{
    struct render_triangle *tri1 = a;
    struct render_triangle *tri2 = b;
    if( tri1->minz < tri2->minz ) {
	return -1;
    } else if( tri1->minz > tri2->minz ) {
	return 1;
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
	struct render_triangle triangles[num_triangles];
	render_extract_triangles(tile_entry, cheap_modifier_mode, triangles);
	compute_triangle_boxes(triangles, num_triangles);
	sort_triangles( triangles, num_triangles );
	render_triangles(triangles, num_triangles, render_mode);
    }
}
