/**
 * $Id$
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
#include "display.h"

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


#define CULL_NONE 0
#define CULL_SMALL 1
#define CULL_CCW 2
#define CULL_CW 3

#define SEGMENT_END         0x80000000
#define SEGMENT_ZCLEAR      0x40000000
#define SEGMENT_SORT_TRANS  0x20000000
#define SEGMENT_START       0x10000000
#define SEGMENT_X(c)        (((c) >> 2) & 0x3F)
#define SEGMENT_Y(c)        (((c) >> 8) & 0x3F)
#define NO_POINTER          0x80000000

extern char *video_base;

gboolean pvr2_force_fragment_alpha = FALSE;
gboolean pvr2_debug_render = FALSE;

struct tile_segment {
    uint32_t control;
    pvraddr_t opaque_ptr;
    pvraddr_t opaquemod_ptr;
    pvraddr_t trans_ptr;
    pvraddr_t transmod_ptr;
    pvraddr_t punchout_ptr;
};

void render_print_tilelist( FILE *f, uint32_t tile_entry );

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
    /* int e = ((half & 0x7C00) >> 10) - 15 + 127;

    temp.i = ((half & 0x8000) << 16) | (e << 23) |
    ((half & 0x03FF) << 13); */
    temp.i = ((uint32_t)half)<<16;
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

    if( POLY1_SPECULAR(poly1) ) {
	glEnable(GL_COLOR_SUM);
    } else {
	glDisable(GL_COLOR_SUM);
    }

    pvr2_force_fragment_alpha = POLY2_ALPHA_ENABLE(poly2) ? FALSE : TRUE;

    if( POLY1_TEXTURED(poly1) ) {
	int width = POLY2_TEX_WIDTH(poly2);
	int height = POLY2_TEX_HEIGHT(poly2);
	glEnable(GL_TEXTURE_2D);
	texcache_get_texture( (texture&0x000FFFFF)<<3, width, height, texture );
	switch( POLY2_TEX_BLEND(poly2) ) {
	case 0: /* Replace */
	    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	    break;
	case 2:/* Decal */
	    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
	    break;
	case 1: /* Modulate RGB */
	    /* This is not directly supported by opengl (other than by mucking
	     * with the texture format), but we get the same effect by forcing
	     * the fragment alpha to 1.0 and using GL_MODULATE.
	     */
	    pvr2_force_fragment_alpha = TRUE;
	case 3: /* Modulate RGBA */
	    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	    break;
	}

	if( POLY2_TEX_CLAMP_U(poly2) ) {
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
	} else {
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	}	    
	if( POLY2_TEX_CLAMP_V(poly2) ) {
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
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

#define FARGB_A(x) (((float)(((x)>>24)+1))/256.0)
#define FARGB_R(x) (((float)((((x)>>16)&0xFF)+1))/256.0)
#define FARGB_G(x) (((float)((((x)>>8)&0xFF)+1))/256.0)
#define FARGB_B(x) (((float)(((x)&0xFF)+1))/256.0)

void render_unpack_vertexes( struct vertex_unpacked *out, uint32_t poly1, 
			     uint32_t *vertexes, int num_vertexes,
			     int vertex_size, int render_mode )
{
    int m = 0, i;
    if( render_mode == RENDER_FULLMOD ) {
	m = (vertex_size - 3)/2;
    }

    for( i=0; i<num_vertexes; i++ ) {
	float *vertexf = (float *)vertexes;
	int k = m + 3;
	out[i].x = vertexf[0];
	out[i].y = vertexf[1];
	out[i].z = vertexf[2];
    	if( POLY1_TEXTURED(poly1) ) {
	    if( POLY1_UV16(poly1) ) {
		out[i].u = halftofloat(vertexes[k]>>16);
		out[i].v = halftofloat(vertexes[k]);
		k++;
	    } else {
		out[i].u = vertexf[k];
		out[i].v = vertexf[k+1];
		k+=2;
	    }
	} else {
	    out[i].u = 0;
	    out[i].v = 0;
	}
	uint32_t argb = vertexes[k++];
	out[i].rgba[0] = FARGB_R(argb);
	out[i].rgba[1] = FARGB_G(argb);
        out[i].rgba[2] = FARGB_B(argb);
	out[i].rgba[3] = FARGB_A(argb);
	if( POLY1_SPECULAR(poly1) ) {
	    uint32_t offset = vertexes[k++];
	    out[i].offset_rgba[0] = FARGB_R(offset);
	    out[i].offset_rgba[1] = FARGB_G(offset);
	    out[i].offset_rgba[2] = FARGB_B(offset);
	    out[i].offset_rgba[3] = FARGB_A(offset);
	}
	vertexes += vertex_size;
    }
}

/**
 * Unpack the vertexes for a quad, calculating the values for the last
 * vertex.
 * FIXME: Integrate this with rendbkg somehow
 */
void render_unpack_quad( struct vertex_unpacked *unpacked, uint32_t poly1, 
			 uint32_t *vertexes, int vertex_size,
			 int render_mode )
{
    int i;
    struct vertex_unpacked diff0, diff1;

    render_unpack_vertexes( unpacked, poly1, vertexes, 3, vertex_size, render_mode );
    
    diff0.x = unpacked[0].x - unpacked[1].x;
    diff0.y = unpacked[0].y - unpacked[1].y;
    diff1.x = unpacked[2].x - unpacked[1].x;
    diff1.y = unpacked[2].y - unpacked[1].y;

    float detxy = ((diff1.y) * (diff0.x)) - ((diff0.y) * (diff1.x));
    float *vertexf = (float *)(vertexes+(vertex_size*3));
    if( detxy == 0 ) {
	memcpy( &unpacked[3], &unpacked[2], sizeof(struct vertex_unpacked) );
	unpacked[3].x = vertexf[0];
	unpacked[3].y = vertexf[1];
	return;
    }	

    unpacked[3].x = vertexf[0];
    unpacked[3].y = vertexf[1];
    float t = ((unpacked[3].x - unpacked[1].x) * diff1.y -
	       (unpacked[3].y - unpacked[1].y) * diff1.x) / detxy;
    float s = ((unpacked[3].y - unpacked[1].y) * diff0.x -
	       (unpacked[3].x - unpacked[1].x) * diff0.y) / detxy;
    diff0.z = (1/unpacked[0].z) - (1/unpacked[1].z);
    diff1.z = (1/unpacked[2].z) - (1/unpacked[1].z);
    unpacked[3].z = 1/((1/unpacked[1].z) + (t*diff0.z) + (s*diff1.z));

    diff0.u = unpacked[0].u - unpacked[1].u;
    diff0.v = unpacked[0].v - unpacked[1].v;
    diff1.u = unpacked[2].u - unpacked[1].u;
    diff1.v = unpacked[2].v - unpacked[1].v;
    unpacked[3].u = unpacked[1].u + (t*diff0.u) + (s*diff1.u);
    unpacked[3].v = unpacked[1].v + (t*diff0.v) + (s*diff1.v);

    if( !POLY1_GOURAUD_SHADED(poly1) ) {
	memcpy( unpacked[3].rgba, unpacked[2].rgba, sizeof(unpacked[2].rgba) );
	memcpy( unpacked[3].offset_rgba, unpacked[2].offset_rgba, sizeof(unpacked[2].offset_rgba) );
    } else {
	for( i=0; i<4; i++ ) {
	    float d0 = unpacked[0].rgba[i] - unpacked[1].rgba[i];
	    float d1 = unpacked[2].rgba[i] - unpacked[1].rgba[i];
	    unpacked[3].rgba[i] = unpacked[1].rgba[i] + (t*d0) + (s*d1);
	    d0 = unpacked[0].offset_rgba[i] - unpacked[1].offset_rgba[i];
	    d1 = unpacked[2].offset_rgba[i] - unpacked[1].offset_rgba[i];
	    unpacked[3].offset_rgba[i] = unpacked[1].offset_rgba[i] + (t*d0) + (s*d1);
	}
    }    
}

void render_unpacked_vertex_array( uint32_t poly1, struct vertex_unpacked *vertexes[], 
				   int num_vertexes ) {
    int i;

    glBegin( GL_TRIANGLE_STRIP );

    for( i=0; i<num_vertexes; i++ ) {
	if( POLY1_TEXTURED(poly1) ) {
	    glTexCoord2f( vertexes[i]->u, vertexes[i]->v );
	}

	if( pvr2_force_fragment_alpha ) {
	    glColor4f( vertexes[i]->rgba[0], vertexes[i]->rgba[1], vertexes[i]->rgba[2], 1.0 );
	} else {
	    glColor4f( vertexes[i]->rgba[0], vertexes[i]->rgba[1], vertexes[i]->rgba[2],
		       vertexes[i]->rgba[3] );
	}
	if( POLY1_SPECULAR(poly1) ) {
	    glSecondaryColor3fEXT( vertexes[i]->offset_rgba[0],
				   vertexes[i]->offset_rgba[1],
				   vertexes[i]->offset_rgba[2] );
	}
	glVertex3f( vertexes[i]->x, vertexes[i]->y, 1/vertexes[i]->z );
    }

    glEnd();
}

void render_quad_vertexes( uint32_t poly1, uint32_t *vertexes, int vertex_size, int render_mode )
{
    struct vertex_unpacked unpacked[4];
    struct vertex_unpacked *pt[4] = {&unpacked[0], &unpacked[1], &unpacked[3], &unpacked[2]};
    render_unpack_quad( unpacked, poly1, vertexes, vertex_size, render_mode );
    render_unpacked_vertex_array( poly1, pt, 4 );
}

void render_vertex_array( uint32_t poly1, uint32_t *vert_array[], int num_vertexes, int vertex_size,
			  int render_mode ) 
{
    int i, m=0;

    if( render_mode == RENDER_FULLMOD ) {
	m = (vertex_size - 3)/2;
    }

    glBegin( GL_TRIANGLE_STRIP );
    
    for( i=0; i<num_vertexes; i++ ) {
	uint32_t *vertexes = vert_array[i];
	float *vertexf = (float *)vert_array[i];
	uint32_t argb;
	int k = m + 3;
	if( POLY1_TEXTURED(poly1) ) {
	    if( POLY1_UV16(poly1) ) {
		glTexCoord2f( halftofloat(vertexes[k]>>16),
			      halftofloat(vertexes[k]) );
		k++;
	    } else {
		glTexCoord2f( vertexf[k], vertexf[k+1] );
		k+=2;
	    }
	}

	argb = vertexes[k++];
	if( pvr2_force_fragment_alpha ) {
	    glColor4ub( (GLubyte)(argb >> 16), (GLubyte)(argb >> 8), 
			(GLubyte)argb, 0xFF );
	} else {
	    glColor4ub( (GLubyte)(argb >> 16), (GLubyte)(argb >> 8), 
			(GLubyte)argb, (GLubyte)(argb >> 24) );
	}

	if( POLY1_SPECULAR(poly1) ) {
	    uint32_t spec = vertexes[k++];
	    glSecondaryColor3ubEXT( (GLubyte)(spec >> 16), (GLubyte)(spec >> 8), 
				 (GLubyte)spec );
	}
	glVertex3f( vertexf[0], vertexf[1], 1/vertexf[2] );
	vertexes += vertex_size;
    }

    glEnd();
}

void render_vertexes( uint32_t poly1, uint32_t *vertexes, int num_vertexes, int vertex_size,
		      int render_mode )
{
    uint32_t *vert_array[num_vertexes];
    int i;
    for( i=0; i<num_vertexes; i++ ) {
	vert_array[i] = vertexes;
	vertexes += vertex_size;
    }
    render_vertex_array( poly1, vert_array, num_vertexes, vertex_size, render_mode );
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
		int strip_count = ((entry >> 25) & 0x0F)+1;
		int polygon_length = 4 * vertex_length + context_length;
		int i;
		for( i=0; i<strip_count; i++ ) {
		    render_set_context( polygon, render_mode );
		    render_quad_vertexes( *polygon, polygon+context_length, vertex_length,
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

void pvr2_render_tilebuffer( int width, int height, int clipx1, int clipy1, 
			int clipx2, int clipy2 ) {

    pvraddr_t segmentbase = MMIO_READ( PVR2, RENDER_TILEBASE );
    int tile_sort;
    gboolean cheap_shadow;

    int obj_config = MMIO_READ( PVR2, RENDER_OBJCFG );
    int isp_config = MMIO_READ( PVR2, RENDER_ISPCFG );
    int shadow_cfg = MMIO_READ( PVR2, RENDER_SHADOW );

    if( (obj_config & 0x00200000) == 0 ) {
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

    glEnable( GL_SCISSOR_TEST );
    do {
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
	    continue;
	}

	/* Set a scissor on the visible part of the tile */
	int w = MIN(x1+32, clipx2) - x1;
	int h = MIN(y1+32, clipy2) - y1;
	x1 = MAX(x1,clipx1);
	y1 = MAX(y1,clipy1);
	glScissor( x1, height-y1-h, w, h );

	if( (segment->opaque_ptr & NO_POINTER) == 0 ) {
	    if( pvr2_debug_render ) {
		fprintf( stderr, "Tile %d,%d Opaque\n", tilex, tiley );
		render_print_tilelist( stderr, segment->opaque_ptr );
	    }
	    if( (segment->opaquemod_ptr & NO_POINTER) == 0 ) {
		/* TODO */
	    }
	    render_tile( segment->opaque_ptr, RENDER_NORMAL, cheap_shadow );
	}

	if( (segment->trans_ptr & NO_POINTER) == 0 ) {
	    if( pvr2_debug_render ) {
		fprintf( stderr, "Tile %d,%d Trans\n", tilex, tiley );
		render_print_tilelist( stderr, segment->trans_ptr );
	    }
	    if( (segment->transmod_ptr & NO_POINTER) == 0 ) {
		/* TODO */
	    } 
	    if( tile_sort == 2 || 
		(tile_sort == 1 && ((segment->control & SEGMENT_SORT_TRANS)==0)) ) {
		render_autosort_tile( segment->trans_ptr, RENDER_NORMAL, cheap_shadow );
	    } else {
		render_tile( segment->trans_ptr, RENDER_NORMAL, cheap_shadow );
	    }
	}

	if( (segment->punchout_ptr & NO_POINTER) == 0 ) {
	    if( pvr2_debug_render ) {
		fprintf( stderr, "Tile %d,%d Punchout\n", tilex, tiley );
		render_print_tilelist( stderr, segment->punchout_ptr );
	    }
	    render_tile( segment->punchout_ptr, RENDER_NORMAL, cheap_shadow );
	}
    } while( ((segment++)->control & SEGMENT_END) == 0 );
    glDisable( GL_SCISSOR_TEST );
}

static float render_find_maximum_tile_z( pvraddr_t tile_entry, float inputz )
{
    uint32_t poly_bank = MMIO_READ(PVR2,RENDER_POLYBASE);
    uint32_t *tile_list = (uint32_t *)(video_base+tile_entry);
    int shadow_cfg = MMIO_READ( PVR2, RENDER_SHADOW ) & 0x100;
    int i, j;
    float z = inputz;
    do {
	uint32_t entry = *tile_list++;
	if( entry >> 28 == 0x0F ) {
	    break;
	} else if( entry >> 28 == 0x0E ) {
	    tile_list = (uint32_t *)(video_base + (entry&0x007FFFFF));
	} else {
	    uint32_t *polygon = (uint32_t *)(video_base + poly_bank + ((entry & 0x000FFFFF) << 2));
	    int vertex_length = (entry >> 21) & 0x07;
	    int context_length = 3;
	    if( (entry & 0x01000000) && (shadow_cfg==0) ) {
		context_length = 5;
		vertex_length *= 2 ;
	    }
	    vertex_length += 3;
	    if( (entry & 0xE0000000) == 0x80000000 ) {
		/* Triangle(s) */
		int strip_count = ((entry >> 25) & 0x0F)+1;
		float *vertexz = (float *)(polygon+context_length+2);
		for( i=0; i<strip_count; i++ ) {
		    for( j=0; j<3; j++ ) {
			if( *vertexz > z ) {
			    z = *vertexz;
			}
			vertexz += vertex_length;
		    }
		    vertexz += context_length;
		}
	    } else if( (entry & 0xE0000000) == 0xA0000000 ) {
		/* Sprite(s) */
		int strip_count = ((entry >> 25) & 0x0F)+1;
		int i;
		float *vertexz = (float *)(polygon+context_length+2);
		for( i=0; i<strip_count; i++ ) {
		    for( j=0; j<4; j++ ) {
			if( *vertexz > z ) {
			    z = *vertexz;
			}
			vertexz += vertex_length;
		    }
		    vertexz+=context_length;
		}
	    } else {
		/* Polygon */
		int i;
		float *vertexz = (float *)polygon+context_length+2;
		for( i=0; i<6; i++ ) {
		    if( (entry & (0x40000000>>i)) && *vertexz > z ) {
			z = *vertexz;
		    }
		    vertexz += vertex_length;
		}
	    }
	}
    } while(1);
    return z;
}

/**
 * Scan through the scene to determine the largest z value (in order to set up
 * an appropriate near clip plane).
 */
float pvr2_render_find_maximum_z( )
{
    pvraddr_t segmentbase = MMIO_READ( PVR2, RENDER_TILEBASE );
    float maximumz = MMIO_READF( PVR2, RENDER_FARCLIP ); /* Initialize to the far clip plane */

    struct tile_segment *segment = (struct tile_segment *)(video_base + segmentbase);
    do {
	
	if( (segment->opaque_ptr & NO_POINTER) == 0 ) {
	    maximumz = render_find_maximum_tile_z(segment->opaque_ptr, maximumz);
	}
	if( (segment->opaquemod_ptr & NO_POINTER) == 0 ) {
	    maximumz = render_find_maximum_tile_z(segment->opaquemod_ptr, maximumz);
	}
	if( (segment->trans_ptr & NO_POINTER) == 0 ) {
	    maximumz = render_find_maximum_tile_z(segment->trans_ptr, maximumz);
	}
	if( (segment->transmod_ptr & NO_POINTER) == 0 ) {
	    maximumz = render_find_maximum_tile_z(segment->transmod_ptr, maximumz);
	}
	if( (segment->punchout_ptr & NO_POINTER) == 0 ) {
	    maximumz = render_find_maximum_tile_z(segment->punchout_ptr, maximumz);
	}

    } while( ((segment++)->control & SEGMENT_END) == 0 );

    return 1/maximumz;
}

/**
 * Scan the segment info to determine the width and height of the render (in 
 * pixels).
 * @param x,y output values to receive the width and height info.
 */
void pvr2_render_getsize( int *x, int *y ) 
{
    pvraddr_t segmentbase = MMIO_READ( PVR2, RENDER_TILEBASE );
    int maxx = 0, maxy = 0;

    struct tile_segment *segment = (struct tile_segment *)(video_base + segmentbase);
    do {
	int tilex = SEGMENT_X(segment->control);
	int tiley = SEGMENT_Y(segment->control);
	if( tilex > maxx ) {
	    maxx = tilex;
	} 
	if( tiley > maxy ) {
	    maxy = tiley;
	}
    } while( ((segment++)->control & SEGMENT_END) == 0 );

    *x = (maxx+1)<<5;
    *y = (maxy+1)<<5;
}

void render_print_vertexes( FILE *f, uint32_t poly1, uint32_t *vert_array[], 
			    int num_vertexes, int vertex_size )
{
    char buf[256], *p;
    int i, k;
    for( i=0; i<num_vertexes; i++ ) {
	p = buf;
	float *vertf = (float *)vert_array[i];
	uint32_t *verti = (uint32_t *)vert_array[i];
	p += sprintf( p, "  V %9.5f,%9.5f,%9.5f  ", vertf[0], vertf[1], vertf[2] );
	k = 3;
	if( POLY1_TEXTURED(poly1) ) {
	    if( POLY1_UV16(poly1) ) {
		p += sprintf( p, "uv=%9.5f,%9.5f  ",
			       halftofloat(verti[k]>>16),
			       halftofloat(verti[k]) );
		k++;
	    } else {
		p += sprintf( p, "uv=%9.5f,%9.5f  ", vertf[k], vertf[k+1] );
		k+=2;
	    }
	}

	p += sprintf( p, "%08X ", verti[k++] );
	if( POLY1_SPECULAR(poly1) ) {
	    p += sprintf( p, "%08X", verti[k++] );
	}
	p += sprintf( p, "\n" );
	fprintf( f, buf );
    }
}

void render_print_polygon( FILE *f, uint32_t entry )
{
    uint32_t poly_bank = MMIO_READ(PVR2,RENDER_POLYBASE);
    int shadow_cfg = MMIO_READ( PVR2, RENDER_SHADOW ) & 0x100;
    int i;

    if( entry >> 28 == 0x0F ) {
	fprintf( f, "EOT\n" );
    } else if( entry >> 28 == 0x0E ) {
	fprintf( f, "LINK %08X\n", entry &0x7FFFFF );
    } else {
	uint32_t *polygon = (uint32_t *)(video_base + poly_bank + ((entry & 0x000FFFFF) << 2));
	int vertex_length = (entry >> 21) & 0x07;
	int context_length = 3;
	if( (entry & 0x01000000) && (shadow_cfg==0) ) {
	    context_length = 5;
	    vertex_length *= 2 ;
	}
	vertex_length += 3;
	if( (entry & 0xE0000000) == 0x80000000 ) {
	    /* Triangle(s) */
	    int strip_count = ((entry >> 25) & 0x0F)+1;
	    for( i=0; i<strip_count; i++ ) {
		fprintf( f, "TRI  %08X %08X %08X\n", polygon[0], polygon[1], polygon[2] ); 
		uint32_t *array[3];
		array[0] = polygon + context_length;
		array[1] = array[0] + vertex_length;
		array[2] = array[1] + vertex_length;
		render_print_vertexes( f, *polygon, array, 3, vertex_length );
		polygon = array[2] + vertex_length;
	    }
	} else if( (entry & 0xE0000000) == 0xA0000000 ) {
	    /* Sprite(s) */
	    int strip_count = ((entry >> 25) & 0x0F)+1;
	    for( i=0; i<strip_count; i++ ) {
		fprintf( f, "QUAD %08X %08X %08X\n", polygon[0], polygon[1], polygon[2] ); 
		uint32_t *array[4];
		array[0] = polygon + context_length;
		array[1] = array[0] + vertex_length;
		array[2] = array[1] + vertex_length;
		array[3] = array[2] + vertex_length;
		render_print_vertexes( f, *polygon, array, 4, vertex_length );
		polygon = array[3] + vertex_length;
	    }
	} else {
	    /* Polygon */
	    int last = -1;
	    uint32_t *array[8];
	    for( i=0; i<6; i++ ) {
		if( entry & (0x40000000>>i) ) {
		    last = i;
		}
	    }
	    fprintf( f, "POLY %08X %08X %08X\n", polygon[0], polygon[1], polygon[2] );
	    for( i=0; i<last+2; i++ ) {
		array[i] = polygon + context_length + vertex_length*i;
	    }
	    render_print_vertexes( f, *polygon, array, last+2, vertex_length );
	}
    }
}

void render_print_tilelist( FILE *f, uint32_t tile_entry )
{
    uint32_t *tile_list = (uint32_t *)(video_base+tile_entry);
    do {
	uint32_t entry = *tile_list++;
	if( entry >> 28 == 0x0F ) {
	    break;
	} else if( entry >> 28 == 0x0E ) {
	    tile_list = (uint32_t *)(video_base + (entry&0x007FFFFF));
	} else {
	    render_print_polygon(f, entry);
	}
    } while( 1 );
}

