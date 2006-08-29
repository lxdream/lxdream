/**
 * $Id: rendbkg.c,v 1.1 2006-08-29 08:12:13 nkeynes Exp $
 *
 * PVR2 background renderer. 
 *
 * Yes, it uses the same basic data structure. Yes, it needs to be handled
 * completely differently.
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

struct vertex_rgba {
    float x,y,z;
    uint32_t argb;
};

struct vertex_all {
    float x,y,z;
    float u,v;
    float rgba[4];      /* Note - RGBA order, as preferred by GL */
    float spec_rgba[4];
};

#define FARGB_A(x) (((float)(((x)>>24)+1))/256.0)
#define FARGB_R(x) (((float)((((x)>>16)&0xFF)+1))/256.0)
#define FARGB_G(x) (((float)((((x)>>8)&0xFF)+1))/256.0)
#define FARGB_B(x) (((float)(((x)&0xFF)+1))/256.0)

/**
 * Compute the values for an array of vertexes, given x,y for each
 * vertex and the base 3-vertex triple used to define the background
 * plane. Essentially the base vertexes are used to find the
 * plane equation for each of z,a,r,g,b,etc, which is then solved for
 * each of the required compute vertexes (normally the corner points).
 *
 * @param base The 3 vertexes supplied as the background definition
 * @param compute An array of vertexes to compute. x and y must be
 *   preset, other values are computed.
 * @param num_compute number of vertexes in the compute array.
 */
void compute_vertexes( struct vertex_rgba *base, 
		       struct vertex_all *compute,
		       int num_compute )
{
    struct vertex_all center;
    struct vertex_all diff0, diff1;
    int i;

    center.x = base[1].x;
    center.y = base[1].y;
    center.z = base[1].z;
    center.rgba[0] = FARGB_R(base[1].argb);
    center.rgba[1] = FARGB_G(base[1].argb);
    center.rgba[2] = FARGB_B(base[1].argb);
    center.rgba[3] = FARGB_A(base[1].argb);
    diff0.x = base[0].x - base[1].x;
    diff0.y = base[0].y - base[1].y;
    diff0.z = base[0].z - base[1].z;
    diff1.x = base[2].x - base[1].x;
    diff1.y = base[2].y - base[1].y;
    diff1.z = base[2].z - base[1].z;
    diff0.rgba[0] = FARGB_R(base[0].argb) - center.rgba[0];
    diff0.rgba[1] = FARGB_G(base[0].argb) - center.rgba[1];
    diff0.rgba[2] = FARGB_B(base[0].argb) - center.rgba[2];
    diff0.rgba[3] = FARGB_A(base[0].argb) - center.rgba[3];
    diff1.rgba[0] = FARGB_R(base[2].argb) - center.rgba[0];
    diff1.rgba[1] = FARGB_G(base[2].argb) - center.rgba[1];
    diff1.rgba[2] = FARGB_B(base[2].argb) - center.rgba[2];
    diff1.rgba[3] = FARGB_A(base[2].argb) - center.rgba[3];

    float divisor = ((diff1.y) * (diff0.x)) - ((diff0.y) * (diff1.x));
    if( divisor == 0 ) {
	/* The points lie on a single line - no plane for you. *shrugs* */
    } else {
	for( i=0; i<num_compute; i++ ) {
	    float t = ((compute[i].x - center.x) * diff1.y -
		       (compute[i].y - center.y) * diff1.x) / divisor;
	    float s = ((compute[i].y - center.y) * diff0.x -
		       (compute[i].x - center.x) * diff0.y) / divisor;
	    compute[i].z = center.z + (t*diff0.z) + (s*diff1.z);
	    compute[i].rgba[0] = center.rgba[0] + (t*diff0.rgba[0]) + (s*diff1.rgba[0]);
	    compute[i].rgba[1] = center.rgba[1] + (t*diff0.rgba[1]) + (s*diff1.rgba[1]);
	    compute[i].rgba[2] = center.rgba[2] + (t*diff0.rgba[2]) + (s*diff1.rgba[2]);
	    compute[i].rgba[3] = center.rgba[3] + (t*diff0.rgba[3]) + (s*diff1.rgba[3]);
	}
    }
}

void render_backplane( uint32_t *polygon, uint32_t width, uint32_t height, uint32_t mode ) {
    struct vertex_rgba *vertex = (struct vertex_rgba *)(polygon + 3);
    struct vertex_all compute[4] = { {0.0,0.0}, {width,0.0}, {width, height}, {0.0,height} };
    int i;

    render_set_context(polygon, 0);
    compute_vertexes( vertex, compute, 4 );
    glBegin(GL_QUADS);
    for( i=0; i<4; i++ ) {
	glColor4fv(compute[i].rgba);
	glVertex3f(compute[i].x, compute[i].y, compute[i].z);
	fprintf( stderr, "BG %d,%d: %f %f %f\n", (int)compute[i].x, (int)compute[i].y,
		 compute[i].rgba[0], compute[i].rgba[1], compute[i].rgba[2] );
    }
    glEnd();
}
