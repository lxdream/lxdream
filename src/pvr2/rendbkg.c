/**
 * $Id: rendbkg.c,v 1.7 2007-10-08 11:52:13 nkeynes Exp $
 *
 * PVR2 background renderer. 
 *
 * Yes, it uses the same basic data structure. Yes, it needs to be handled
 * completely differently.
 *
 * PVR2 backgrounds are defined as a set of three fully specified vertexes,
 * stored in compiled-vertex format. The vertexes form a triangle which is
 * rendered in the normal fashion. Points outside the triangle are rendered
 * by extrapolating from the gradients established by the triangle, giving
 * an overall smooth gradient across the background. Points are colour-clamped
 * prior to output to the buffer.
 *
 * As a special case, if all three points lie on the same line (or are the same
 * point, the third point is used by itself to define the entire buffer (ie
 * effectively a solid colour).
 *
 * Note: this would be really simple if GL did unclamped colour interpolation
 * but it doesn't (portably), which makes this roughly 2 orders of magnitude
 * more complicated than it otherwise would be.
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
#include <GL/glext.h>
#include <math.h>

#define MAX_CLAMP_LINES 8
#define MAX_VERTEXES 256
#define MAX_REGIONS  256

#define FARGB_A(x) (((float)(((x)>>24)+1))/256.0)
#define FARGB_R(x) (((float)((((x)>>16)&0xFF)+1))/256.0)
#define FARGB_G(x) (((float)((((x)>>8)&0xFF)+1))/256.0)
#define FARGB_B(x) (((float)(((x)&0xFF)+1))/256.0)

/**
 * Compute the line where k = target_k, (where k is normally one of
 * r,g,b,a, or z) and determines the points at which the line intersects
 * the viewport (0,0,width,height).
 *
 * @param center_x the x value for the center position
 * @param center_y the y value for the center position
 * @param center_k the k value for the center position
 * @param width Width of the viewport (ie 640)
 * @param height Height of the viewport (ie 480)
 * @param target_k determine the line where k = this value, ie 1.0
 * @param detxy
 * @param target Array to write the resultant x,y pairs to (note this
 * function only sets x and y values).
 * @return number of vertexes written to the target.
 */
static int compute_colour_line( float center_x, float center_y, float center_k, 
		  int width, int height, float target_k,
		  float detxy, float detxk, float detyk,
		  struct vertex_unpacked *target ) {
    int num_points = 0;
    float tmpk = (target_k - center_k) * detxy;
    float x0 = -1;
    float x1 = -1;
    
    if( detyk != 0 ) {
	x0 = (tmpk - ((0-center_y)*detxk))/detyk + center_x; /* x where y=0 */
	if( x0 >= 0.0 && x0 <= width ) {
	    target[num_points].x = x0;
	    target[num_points].y = 0.0;
	    num_points++;
	}
	
	x1 = (tmpk - ((height-center_y)*detxk))/detyk + center_x; /* x where y=height */
	if( x1 >= 0.0 && x1 <= width ) {
	    target[num_points].x = x1;
	    target[num_points].y = height;
	    num_points++;
	}
    }
    
    if( detxk != 0 ) {
	if( x0 != 0.0 && x1 != 0.0 ) { /* If x0 == 0 or x1 == 0, then we already have this one */
	    float y0 = (tmpk - ((0-center_x)*detyk))/detxk + center_y; /* y where x=0 */
	    if( y0 >= 0.0 && y0 <= height ) {
		target[num_points].x = 0.0;
		target[num_points].y = y0;
		num_points++;
	    }
	}
	
	if( x0 != width && x1 != width ) {
	    float y1 = (tmpk - ((width-center_x)*detyk))/detxk + center_y; /* y where x=width */
	    if( y1 >= 0.0 && y1 <= height ) {
		target[num_points].x = width;
		target[num_points].y = y1;
		num_points++;
	    }
	}
    }

    if( num_points == 0 || num_points == 2 ) {
	/* 0 = no points - line doesn't pass through the viewport */
	/* 2 = normal case - got 2 endpoints */
	return num_points;
    } else {
	ERROR( "compute_colour_line got bad number of points: %d", num_points );
	return 0;
    }
}

/**
 * A region describes a portion of the screen, possibly subdivided by a line.
 * if region_left and region_right are -1, this is a terminal region that can
 * be rendered directly. Otherwise region_left and region_right refer two 
 * sub-regions that are separated by the line segment vertex1-vertex2.
 */
struct bkg_region {
    /* Vertexes marking the line segment that splits this region */
    int vertex1;
    int vertex2;
    /* Index of the left sub-region */
    int region_left;
    /* Index of the right sub-region */
    int region_right;
};

/**
 * Convenience structure to bundle together the vertex and region data.
 */
struct bkg_scene {
    int num_vertexes;
    int num_regions;
    struct vertex_unpacked vertexes[MAX_VERTEXES];
    struct bkg_region regions[MAX_REGIONS];
};

/**
 * Constants returned by compute_line_intersection. Note that for these purposes,
 * "Left" means the point(s) result in a negative value in the line equation, while
 * "Right" means the points(s) result in a positive value in the line equation. The
 * exact meaning isn't particularly important though, as long as we're consistent
 * throughout this process
 */
#define LINE_COLLINEAR 0   /* The line segments are part of the same line */
#define LINE_SIDE_LEFT 1   /* The second line is entirely to the "left" of the first line */
#define LINE_SIDE_RIGHT 2  /* The second line is entirely to the "right" of the first line */
#define LINE_INTERSECT_FROM_LEFT 3 /* The lines intersect, and (x3,y3) is to the "left" of the first line */
#define LINE_INTERSECT_FROM_RIGHT 4 /* The lines intersect, and (x3,y3) is to the "right" of the first line */
#define LINE_SKEW 5        /* The line segments neither intersect nor do any of the above apply (should never happen here) */

/**
 * Compute the intersection of two line segments, where 
 * (x1,y1)-(x2,y2) defines the target segment, and
 * (x3,y3)-(x4,y4) defines the line intersecting it.
 *
 * Based off work by Mukesh Prasad (http://www.acm.org/pubs/tog/GraphicsGems/index.html)
 *
 * @return one of the above LINE_* constants
 */
static int compute_line_intersection( float x1, float y1,   /* First line segment */
				      float x2, float y2,
				      float x3, float y3,   /* Second line segment */
				      float x4, float y4,
				      float *x, float *y  )  /* Output value: */
{
    float a1, a2, b1, b2, c1, c2; /* Coefficients of line eqns. */
    float r1, r2, r3, r4;         /* test values */
    float denom;     /* Intermediate values */

    /* Compute a1, b1, c1, where line joining points 1 and 2
     * is "a1 x  +  b1 y  +  c1  =  0".
     */

    a1 = y2 - y1;
    b1 = x1 - x2;
    c1 = x2 * y1 - x1 * y2;

    /* Compute r3 and r4. */

    r3 = a1 * x3 + b1 * y3 + c1;
    r4 = a1 * x4 + b1 * y4 + c1;

    /* Check signs of r3 and r4.  If both point 3 and point 4 lie on
     * same side of line 1, the line segments do not intersect.
     */

    if( r3 == 0 && r4 == 0 ) {
	return LINE_COLLINEAR;
    } else if( r3 <= 0 && r4 <= 0 ) {
	return LINE_SIDE_LEFT;
    } else if( r3 >= 0 && r4 >= 0 ) {
	return LINE_SIDE_RIGHT;
    }

    /* Compute a2, b2, c2 */

    a2 = y4 - y3;
    b2 = x3 - x4;
    c2 = x4 * y3 - x3 * y4;

    /* Compute r1 and r2 */

    r1 = a2 * x1 + b2 * y1 + c2;
    r2 = a2 * x2 + b2 * y2 + c2;

    /* Check signs of r1 and r2.  If both point 1 and point 2 lie
     * on same side of second line segment, the line segments do
     * not intersect.
     */

    if ( r1 != 0 && r2 != 0 &&
         signbit(r1) == signbit(r2) ) {
        return LINE_SKEW; /* Should never happen */
    }

    /* Cmpute intersection point. 
     */
    denom = a1 * b2 - a2 * b1;
    if ( denom == 0 )
        return LINE_COLLINEAR; /* Should never get to this point either */

    *x = (b1 * c2 - b2 * c1) / denom;
    *y = (a2 * c1 - a1 * c2) / denom;

    if( r3 <= 0 && r4 >= 0 ) {
	return LINE_INTERSECT_FROM_LEFT;
    } else {
	return LINE_INTERSECT_FROM_RIGHT;
    }
}

/**
 * Given a set of vertexes and a line segment to use to split them, generates
 * two sets of vertexes representing the polygon on either side of the line
 * segment. This method preserves the winding direction of the input vertexes.
 */
static void compute_subregions( struct bkg_scene *scene,
				int splitv1, int splitv2,
				int *vertex_in, int num_vertex_in,
				int *left_vertex_out, int *num_left_vertex_out,
				int *right_vertex_out, int *num_right_vertex_out )
{
    float x1 = scene->vertexes[splitv1].x;
    float y1 = scene->vertexes[splitv1].y;
    float x2 = scene->vertexes[splitv2].x;
    float y2 = scene->vertexes[splitv2].y;

    float a1 = y2 - y1;
    float b1 = x1 - x2;
    float c1 = x2 * y1 - x1 * y2;
    int i;
    
    *num_left_vertex_out = 0;
    *num_right_vertex_out = 0;
    int last = 0;
    for( i=0; i<num_vertex_in; i++ ) {
	struct vertex_unpacked *vertex = &scene->vertexes[vertex_in[i]];
	float r = a1 * vertex->x + b1 * vertex->y + c1;
	if( r <= 0 ) {
	    if( last == 1 ) {
		/* cross-point. add the split vertexes */
		int v1 = vertex_in[i-1];
		int v2 = vertex_in[i];
		/* Determine which point is closer to the line. Strictly speaking
		 * one of them must be ON the line, but this way allows for floating
		 * point inaccuracies.
		 */
		float a2 = scene->vertexes[v2].y - scene->vertexes[v1].y;
		float b2 = scene->vertexes[v1].x - scene->vertexes[v2].x;
		float c2 = scene->vertexes[v2].x * scene->vertexes[v1].y - 
		    scene->vertexes[v1].x * scene->vertexes[v2].y;
		float r1 = a2 * x1 + b2 * y1 + c2;
		float r2 = a2 * x2 + b2 * y2 + c2;
		if( fabsf(r1) > fabs(r2) ) {
		    int tmp = splitv1;
		    splitv1 = splitv2;
		    splitv2 = tmp;
		}
		right_vertex_out[(*num_right_vertex_out)++] = splitv1;
		right_vertex_out[(*num_right_vertex_out)++] = splitv2;
		left_vertex_out[(*num_left_vertex_out)++] = splitv2;
		left_vertex_out[(*num_left_vertex_out)++] = splitv1;
		last = 2;
	    } else if( last != 2 ) {
		last = -1;
	    }
	    left_vertex_out[(*num_left_vertex_out)++] = vertex_in[i];
	} else {
	    if( last == -1 ) {
		/* cross-point. add the split vertexes */
		int v1 = vertex_in[i-1];
		int v2 = vertex_in[i];
		/* Determine which point is closer to the line. Strictly speaking
		 * one of them must be ON the line, but this way allows for floating
		 * point inaccuracies.
		 */
		float a2 = scene->vertexes[v2].y - scene->vertexes[v1].y;
		float b2 = scene->vertexes[v1].x - scene->vertexes[v2].x;
		float c2 = scene->vertexes[v2].x * scene->vertexes[v1].y - 
		    scene->vertexes[v1].x * scene->vertexes[v2].y;
		float r1 = a2 * x1 + b2 * y1 + c2;
		float r2 = a2 * x2 + b2 * y2 + c2;
		if( fabsf(r1) > fabs(r2) ) {
		    int tmp = splitv1;
		    splitv1 = splitv2;
		    splitv2 = tmp;
		}
		left_vertex_out[(*num_left_vertex_out)++] = splitv1;
		left_vertex_out[(*num_left_vertex_out)++] = splitv2;
		right_vertex_out[(*num_right_vertex_out)++] = splitv2;
		right_vertex_out[(*num_right_vertex_out)++] = splitv1;
		last = 2;
	    } else if( last != 2 ) {
		last = 1;
	    }
	    right_vertex_out[(*num_right_vertex_out)++] = vertex_in[i];
	}
    }
}

/**
 * Subdivide the region tree by splitting it along a given line.
 * 
 * @param scene  current bkg scene data
 * @param region current region under examination
 * @param vertex1 first vertex of the new line segment
 * @param vertex2 second vertex of the new line segment
 */
static void bkg_region_subdivide( struct bkg_scene *scene, int region, int vertex1, int vertex2 ) {
    struct bkg_region *this_region = &scene->regions[region];
    
    if( scene->regions[region].region_left == -1 || scene->regions[region].region_right == -1 ) {
	/* Reached the end of the tree. Setup new left+right regions */
	int i = scene->num_regions;
	scene->regions[i].region_left = scene->regions[i].region_right = -1;
	scene->regions[i+1].region_left = scene->regions[i+1].region_right = -1;
	this_region->region_left = i;
	this_region->region_right = i+1;
	this_region->vertex1 = vertex1;
	this_region->vertex2 = vertex2;
	scene->num_regions += 2;
    } else {
	float x,y;
	int thisv1 = this_region->vertex1;
	int thisv2 = this_region->vertex2;
	int vertex3;
	int status = 
	    compute_line_intersection( scene->vertexes[thisv1].x, scene->vertexes[thisv1].y,
				       scene->vertexes[thisv2].x, scene->vertexes[thisv2].y,
				       scene->vertexes[vertex1].x, scene->vertexes[vertex1].y,
				       scene->vertexes[vertex2].x, scene->vertexes[vertex2].y,
				       &x, &y );
	switch( status ) {
	case LINE_INTERSECT_FROM_LEFT:
	    /* if new line segment intersects our current line segment,
	     * subdivide the segment (add a new vertex) and recurse on both
	     * sub trees 
	     */
	    /* Compute split-point vertex */
	    vertex3 = scene->num_vertexes++;
	    scene->vertexes[vertex3].x = x;
	    scene->vertexes[vertex3].y = y;
	    /* Recurse */
	    bkg_region_subdivide( scene, scene->regions[region].region_left, vertex1,vertex3 );
	    bkg_region_subdivide( scene, scene->regions[region].region_right, vertex3, vertex2 );
	    break;
	case LINE_INTERSECT_FROM_RIGHT:
	    /* Same except line runs in the opposite direction */
	    vertex3 = scene->num_vertexes++;
	    scene->vertexes[vertex3].x = x;
	    scene->vertexes[vertex3].y = y;
	    /* Recurse */
	    bkg_region_subdivide( scene, scene->regions[region].region_left, vertex2,vertex3 );
	    bkg_region_subdivide( scene, scene->regions[region].region_right, vertex3, vertex1 );
	    break;
	case LINE_COLLINEAR:
	case LINE_SKEW:
	    /* Collinear - ignore */
	    break;
	case LINE_SIDE_LEFT:
	    /* else if line segment passes through the left sub-region alone,
	     * left-recurse only.
	     */
	    bkg_region_subdivide( scene, scene->regions[region].region_left, vertex1, vertex2 );
	    break;
	case LINE_SIDE_RIGHT:
	    /* Otherwise line segment passes through the right sub-region alone,
	     * so right-recurse.
	     */
	    bkg_region_subdivide( scene, scene->regions[region].region_right, vertex1, vertex2 );
	    break;
	}
    }
}

	

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
 */
static void bkg_compute_scene( struct vertex_unpacked *base, int width, int height,
				struct bkg_scene *scene )
{
    struct vertex_unpacked center;
    struct vertex_unpacked diff0, diff1;
    int i,k;

    center.x = base[1].x;
    center.y = base[1].y;
    center.z = (1/base[1].z);
    center.u = base[1].u;
    center.v = base[1].v;
    diff0.x = base[0].x - center.x;
    diff0.y = base[0].y - center.y;
    diff0.z = (1/base[0].z) - center.z;
    diff1.x = base[2].x - center.x;
    diff1.y = base[2].y - center.y;
    diff1.z = (1/base[2].z) - center.z;

    float detxy = ((diff1.y) * (diff0.x)) - ((diff0.y) * (diff1.x));
    
    /* Corner points first */
    scene->vertexes[0].x = 0.0;
    scene->vertexes[0].y = 0.0;
    scene->vertexes[1].x = width;
    scene->vertexes[1].y = 0.0;
    scene->vertexes[2].x = width;
    scene->vertexes[2].y = height;
    scene->vertexes[3].x = 0.0;
    scene->vertexes[3].y = height;
    scene->regions[0].region_left = -1;
    scene->regions[0].region_right = -1;
    scene->num_vertexes = 4;
    scene->num_regions = 1;

    if( detxy == 0 ) {
	/* The points lie on a single line - no plane for you. Use the values
	 * from the 3rd point for the whole screen.
	 */
	for( i=0; i<4; i++ ) {
	    scene->vertexes[i].rgba[0] = base[2].rgba[0];
	    scene->vertexes[i].rgba[1] = base[2].rgba[1];
	    scene->vertexes[i].rgba[2] = base[2].rgba[2];
	    scene->vertexes[i].rgba[3] = base[2].rgba[3];
	    scene->vertexes[i].z = 1/base[2].z;
	    scene->vertexes[i].u = base[2].u;
	    scene->vertexes[i].v = base[2].v;
	}
    } else {
	/* Compute the colour values at each corner */
	center.rgba[0] = base[1].rgba[0];
	center.rgba[1] = base[1].rgba[1];
	center.rgba[2] = base[1].rgba[2];
	center.rgba[3] = base[1].rgba[3];
	diff0.rgba[0] = base[0].rgba[0] - center.rgba[0];
	diff0.rgba[1] = base[0].rgba[1] - center.rgba[1];
	diff0.rgba[2] = base[0].rgba[2] - center.rgba[2];
	diff0.rgba[3] = base[0].rgba[3] - center.rgba[3];
	diff0.u = base[0].u - center.u;
	diff0.v = base[0].v - center.v;
	diff1.rgba[0] = base[2].rgba[0] - center.rgba[0];
	diff1.rgba[1] = base[2].rgba[1] - center.rgba[1];
	diff1.rgba[2] = base[2].rgba[2] - center.rgba[2];
	diff1.rgba[3] = base[2].rgba[3] - center.rgba[3];
	diff1.u = base[2].u - center.u;
	diff1.v = base[2].v - center.v;
	for( i=0; i<4; i++ ) {
	    float t = ((scene->vertexes[i].x - center.x) * diff1.y -
		       (scene->vertexes[i].y - center.y) * diff1.x) / detxy;
	    float s = ((scene->vertexes[i].y - center.y) * diff0.x -
		       (scene->vertexes[i].x - center.x) * diff0.y) / detxy;
	    scene->vertexes[i].z = center.z + (t*diff0.z) + (s*diff1.z);
	    scene->vertexes[i].rgba[0] = center.rgba[0] + (t*diff0.rgba[0]) + (s*diff1.rgba[0]);
	    scene->vertexes[i].rgba[1] = center.rgba[1] + (t*diff0.rgba[1]) + (s*diff1.rgba[1]);
	    scene->vertexes[i].rgba[2] = center.rgba[2] + (t*diff0.rgba[2]) + (s*diff1.rgba[2]);
	    scene->vertexes[i].rgba[3] = center.rgba[3] + (t*diff0.rgba[3]) + (s*diff1.rgba[3]);
	    scene->vertexes[i].u = center.u + (t*diff0.u) + (s*diff1.u);
	    scene->vertexes[i].v = center.v + (t*diff0.v) + (s*diff1.v);
	}

	/* Check for values > 1.0 | < 0.0 */
	for( k=0; k<4; k++ ) {
	    float detyk = ((diff1.y) * (diff0.rgba[k])) - ((diff0.y)*(diff1.rgba[k]));
	    float detxk = ((diff0.x) * (diff1.rgba[k])) - ((diff1.x)*(diff0.rgba[k]));
	    if( scene->vertexes[0].rgba[k] > 1.0 || scene->vertexes[1].rgba[k] > 1.0 || 
		scene->vertexes[2].rgba[k] > 1.0 || scene->vertexes[3].rgba[k] > 1.0 ) {
		int v1 = scene->num_vertexes;
		scene->num_vertexes += compute_colour_line(center.x, center.y, center.rgba[k],
						    width, height, 1.0,
						    detxy, detxk, detyk, 
						    scene->vertexes+scene->num_vertexes );
		if( scene->num_vertexes != v1 ) {
		    bkg_region_subdivide( scene, 0, v1, v1+1 );
		}
	    }

	    if( scene->vertexes[0].rgba[k] < 0.0 || scene->vertexes[1].rgba[k] < 0.0 || 
		scene->vertexes[2].rgba[k] < 0.0 || scene->vertexes[3].rgba[k] < 0.0 ) {
		int v1 = scene->num_vertexes;
		scene->num_vertexes += compute_colour_line(center.x, center.y, center.rgba[k],
						    width, height, 0.0,
						    detxy, detxk, detyk, 
						    scene->vertexes+scene->num_vertexes );
		if( scene->num_vertexes != v1 ) {
		    bkg_region_subdivide( scene, 0, v1, v1+1 );
		}

	    }
	}

	/* Finally compute the colour values for all vertexes 
	 * (excluding the 4 we did upfront) */
	for( i=4; i<scene->num_vertexes; i++ ) {
	    float t = ((scene->vertexes[i].x - center.x) * diff1.y -
		       (scene->vertexes[i].y - center.y) * diff1.x) / detxy;
	    float s = ((scene->vertexes[i].y - center.y) * diff0.x -
		       (scene->vertexes[i].x - center.x) * diff0.y) / detxy;
	    scene->vertexes[i].z = center.z + (t*diff0.z) + (s*diff1.z);
	    scene->vertexes[i].rgba[0] = center.rgba[0] + (t*diff0.rgba[0]) + (s*diff1.rgba[0]);
	    scene->vertexes[i].rgba[1] = center.rgba[1] + (t*diff0.rgba[1]) + (s*diff1.rgba[1]);
	    scene->vertexes[i].rgba[2] = center.rgba[2] + (t*diff0.rgba[2]) + (s*diff1.rgba[2]);
	    scene->vertexes[i].rgba[3] = center.rgba[3] + (t*diff0.rgba[3]) + (s*diff1.rgba[3]);
	    scene->vertexes[i].u = center.u + (t*diff0.u) + (s*diff1.u);
	    scene->vertexes[i].v = center.v + (t*diff0.v) + (s*diff1.v);
	}
    }
}

/**
 * Render a bkg_region.
 * @param scene the background scene data
 * @param region the region to render
 * @param vertexes the vertexes surrounding the region
 * @param num_vertexes the number of vertexes in the vertex array
 */
void bkg_render_region( struct bkg_scene *scene, int region, int *vertexes, int num_vertexes,
			uint32_t poly1 )
{
    if( scene->regions[region].region_left == -1 && scene->regions[region].region_right == -1 ) {
	/* Leaf node - render the points as given */
	int i,k;
	glBegin(GL_POLYGON);
	for( i=0; i<num_vertexes; i++ ) {
	    k = vertexes[i];
	    glColor4fv(scene->vertexes[k].rgba);
	    if( POLY1_TEXTURED(poly1) ) {
		glTexCoord2f(scene->vertexes[k].u, scene->vertexes[k].v);
	    }
	    glVertex3f(scene->vertexes[k].x, scene->vertexes[k].y, scene->vertexes[k].z);
	}
	glEnd();
    } else {
	/* split the region into left and right regions */
	int left_vertexes[num_vertexes+1];
	int right_vertexes[num_vertexes+1];
	int num_left = 0;
	int num_right = 0;
	struct bkg_region *reg = &scene->regions[region];
	compute_subregions( scene, reg->vertex1, reg->vertex2, vertexes, num_vertexes,
			    left_vertexes, &num_left, right_vertexes, &num_right );
	bkg_render_region( scene, reg->region_left, left_vertexes, num_left, poly1 );
	bkg_render_region( scene, reg->region_right, right_vertexes, num_right, poly1 );
    }
    
}


void render_backplane( uint32_t *polygon, uint32_t width, uint32_t height, uint32_t mode ) {
    struct vertex_unpacked vertex[3];
    int screen_vertexes[4] = {0,1,2,3};
    struct bkg_scene scene;
    int vertex_length = (mode >> 24) & 0x07;
    int cheap_shadow = MMIO_READ( PVR2, RENDER_SHADOW ) & 0x100;
    int is_modified = mode & 0x08000000;
    int context_length = 3;
    if( is_modified && !cheap_shadow ) {
	context_length = 5;
	vertex_length *= 2;
    }
    vertex_length += 3;
    context_length += (mode & 0x07) * vertex_length;
    

    render_unpack_vertexes( vertex, *polygon, polygon+context_length, 3, vertex_length,
			    RENDER_NORMAL );
    bkg_compute_scene(vertex, width, height, &scene);
    render_set_context(polygon, RENDER_NORMAL);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_ONE, GL_ZERO); /* For now, just disable alpha blending on the bkg */
    bkg_render_region(&scene, 0, screen_vertexes, 4, *polygon);
}
