/**
 * $Id: rendcore.c,v 1.4 2006-09-12 08:38:38 nkeynes Exp $
 *
 * PVR2 renderer core.
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
    GL_REPLACE, GL_BLEND, GL_DECAL, GL_MODULATE };
int pvr2_render_colour_format[8] = {
    COLFMT_ARGB1555, COLFMT_RGB565, COLFMT_ARGB4444, COLFMT_ARGB1555,
    COLFMT_RGB888, COLFMT_ARGB8888, COLFMT_ARGB8888, COLFMT_ARGB4444 };


#define RENDER_ZONLY  0
#define RENDER_NORMAL 1     /* Render non-modified polygons */
#define RENDER_CHEAPMOD 2   /* Render cheap-modified polygons */
#define RENDER_FULLMOD 3    /* Render the fully-modified version of the polygons */

#define CULL_NONE 0
#define CULL_SMALL 1
#define CULL_CCW 2
#define CULL_CW 3

#define SEGMENT_END         0x80000000
#define SEGMENT_SORT_TRANS  0x20000000
#define SEGMENT_START       0x10000000
#define SEGMENT_X(c)        (((c) >> 2) & 0x3F)
#define SEGMENT_Y(c)        (((c) >> 8) & 0x3F)
#define NO_POINTER          0x80000000

extern char *video_base;

struct tile_segment {
    uint32_t control;
    pvraddr_t opaque_ptr;
    pvraddr_t opaquemod_ptr;
    pvraddr_t trans_ptr;
    pvraddr_t transmod_ptr;
    pvraddr_t punchout_ptr;
};

/**
 * Convert a half-float (16-bit) FP number to a regular 32-bit float.
 * Source is 1-bit sign, 5-bit exponent, 10-bit mantissa.
 * TODO: Check the correctness of this.
 */
float halftofloat( uint16_t half )
{
    union {
        float f;
        uint32_t i;
    } temp;
    int e = ((half & 0x7C00) >> 10) - 15 + 127;

    temp.i = ((half & 0x8000) << 16) | (e << 23) |
             ((half & 0x03FF) << 13);
    return temp.f;
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

    if( POLY1_DEPTH_ENABLE(poly1) ) {
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( POLY1_DEPTH_MODE(poly1) );
    } else {
	glDisable( GL_DEPTH_TEST );
    }
    
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

    if( POLY1_TEXTURED(poly1) ) {
	int width = POLY2_TEX_WIDTH(poly2);
	int height = POLY2_TEX_HEIGHT(poly2);
	glEnable(GL_TEXTURE_2D);
	texcache_get_texture( (texture&0x001FFFFF)<<3, width, height, texture );
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, POLY2_TEX_BLEND(poly2) );
    } else {
	glDisable( GL_TEXTURE_2D );
    }

    glShadeModel( POLY1_SHADE_MODEL(poly1) );

    int srcblend = POLY2_SRC_BLEND(poly2);
    int destblend = POLY2_DEST_BLEND(poly2);
    glBlendFunc( srcblend, destblend );
    if( POLY2_TEX_ALPHA_ENABLE(poly2) ) {
	glEnable(GL_BLEND);
    } else {
	glDisable(GL_BLEND);
    }
}

void render_vertexes( uint32_t poly1, uint32_t *vertexes, int num_vertexes, int vertex_size,
		      int render_mode ) 
{
    int i, m=0;

    if( render_mode == RENDER_FULLMOD ) {
	m = (vertex_size - 3)/2;
    }

    glBegin( GL_TRIANGLE_STRIP );
    
    for( i=0; i<num_vertexes; i++ ) {
	float *vertexf = (float *)vertexes;
	uint32_t argb;
	if( POLY1_TEXTURED(poly1) ) {
	    if( POLY1_UV16(poly1) ) {
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
	vertexes += vertex_size;
    }

    glEnd();
}

/**
 * Render a simple (not auto-sorted) tile
 */
void render_tile( pvraddr_t tile_entry, int render_mode, gboolean cheap_modifier_mode ) {
    uint32_t poly_bank = MMIO_READ(PVR2,RENDER_POLYBASE);
    uint32_t *tile_list = (uint32_t *)(video_base+tile_entry);
    do {
	uint32_t entry = *tile_list++;
	if( entry >> 28 == 0x0F ) {
	    break;
	} else if( entry >> 28 == 0x0E ) {
	    tile_list = (uint32_t *)(video_base + (entry&0x007FFFFF));
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
		    render_set_context( polygon, render_mode );
		    render_vertexes( *polygon, polygon+context_length, 3, vertex_length,
				     render_mode );
		    polygon += polygon_length;
		}
	    } else if( (entry & 0xE0000000) == 0xA0000000 ) {
		/* Sprite(s) */
		int strip_count = (entry >> 25) & 0x0F;
		int polygon_length = 4 * vertex_length + context_length;
		int i;
		for( i=0; i<strip_count; i++ ) {
		    render_set_context( polygon, render_mode );
		    render_vertexes( *polygon, polygon+context_length, 4, vertex_length,
				     render_mode );
		    polygon += polygon_length;
		}
	    } else {
		/* Polygon */
		int i, first=-1, last = -1;
		for( i=0; i<6; i++ ) {
		    if( entry & (0x40000000>>i) ) {
			if( first == -1 ) first = i;
			last = i;
		    }
		}
		if( first != -1 ) {
		    first = 0;
		    render_set_context(polygon, render_mode);
		    render_vertexes( *polygon, polygon+context_length + (first*vertex_length),
				     (last-first+3), vertex_length, render_mode );
		}
	    }
	}
    } while( 1 );
}

void render_autosort_tile( pvraddr_t tile_entry, int render_mode, gboolean cheap_modifier_mode ) {
    //WARN( "Autosort not implemented yet" );
    render_tile( tile_entry, render_mode, cheap_modifier_mode );
}

void pvr2_render_tilebuffer( int width, int height, int clipx1, int clipy1, 
			int clipx2, int clipy2 ) {

    pvraddr_t segmentbase = MMIO_READ( PVR2, RENDER_TILEBASE );
    int tile_sort;
    gboolean cheap_shadow;

    int obj_config = MMIO_READ( PVR2, RENDER_OBJCFG );
    int isp_config = MMIO_READ( PVR2, RENDER_ISPCFG );
    int shadow_cfg = MMIO_READ( PVR2, RENDER_SHADOW );

    if( obj_config & 0x00200000 ) {
	if( isp_config & 1 ) {
	    tile_sort = 0;
	} else {
	    tile_sort = 2;
	}
    } else {
	tile_sort = 1;
    }

    cheap_shadow = shadow_cfg & 0x100 ? TRUE : FALSE;

    struct tile_segment *segment = (struct tile_segment *)(video_base + segmentbase);

    struct timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);
    fprintf( stderr, "Start render at %d.%d\n", tv_start.tv_sec, tv_start.tv_usec );
    glEnable( GL_SCISSOR_TEST );
    while( (segment->control & SEGMENT_END) == 0 ) {
	// fwrite_dump32v( (uint32_t *)segment, sizeof(struct tile_segment), 6, stderr );
	int tilex = SEGMENT_X(segment->control);
	int tiley = SEGMENT_Y(segment->control);
	
	int x1 = tilex << 5;
	int y1 = tiley << 5;
	if( x1 + 32 <= clipx1 ||
	    y1 + 32 <= clipy1 ||
	    x1 >= clipx2 ||
	    y1 >= clipy2 ) {
	    /* Tile completely clipped, skip */
	    segment++;
	    continue;
	}

	/* Set a scissor on the visible part of the tile */
	int w = MIN(x1+32, clipx2) - x1;
	int h = MIN(y1+32, clipy2) - y1;
	x1 = MAX(x1,clipx1);
	y1 = MAX(y1,clipy1);
	glScissor( x1, height-y1-h, w, h );

	if( (segment->opaque_ptr & NO_POINTER) == 0 ) {
	    if( (segment->opaquemod_ptr & NO_POINTER) == 0 ) {
		/* TODO */
	    }
	    render_tile( segment->opaque_ptr, RENDER_NORMAL, cheap_shadow );
	}

	if( (segment->trans_ptr & NO_POINTER) == 0 ) {
	    if( (segment->transmod_ptr & NO_POINTER) == 0 ) {
		/* TODO */
	    } 
	    if( tile_sort == 2 || (tile_sort == 1 && (segment->control & SEGMENT_SORT_TRANS)) ) {
		render_autosort_tile( segment->trans_ptr, RENDER_NORMAL, cheap_shadow );
	    } else {
		render_tile( segment->trans_ptr, RENDER_NORMAL, cheap_shadow );
	    }
	}

	if( (segment->punchout_ptr & NO_POINTER) == 0 ) {
	    render_tile( segment->punchout_ptr, RENDER_NORMAL, cheap_shadow );
	}
	segment++;

    }
    glDisable( GL_SCISSOR_TEST );

    gettimeofday(&tv_end, NULL);
    timersub(&tv_end,&tv_start, &tv_start);
    fprintf( stderr, "Frame took %d.%06ds\n", tv_start.tv_sec, tv_start.tv_usec );
    
}
