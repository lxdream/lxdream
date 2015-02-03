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
#define EPSILON 0.0001

struct sort_triangle {
    struct polygon_struct *poly;
    int triangle_num; // triangle number in the poly, from 0
    /* plane equation */
    float mx, my, mz, d;
    float bounds[6]; /* x1,x2,y1,y2,z1,z2 */
};

#define SENTINEL 0xDEADBEEF

/**
 * Count the number of triangles in the list starting at the given 
 * pvr memory address. This is an upper bound as it includes
 * triangles that have been culled out.
 */
static int sort_count_triangles( pvraddr_t tile_entry ) {
    uint32_t *tile_list = (uint32_t *)(pvr2_main_ram+tile_entry);
    int count = 0;
    while(1) {
        uint32_t entry = *tile_list++;
        if( entry >> 28 == 0x0F ) {
            break;
        } else if( entry >> 28 == 0x0E ) {
            tile_list = (uint32_t *)(pvr2_main_ram+(entry&0x007FFFFF));
        } else if( entry >> 29 == 0x04 ) { /* Triangle array */
            count += ((entry >> 25) & 0x0F)+1;
        } else if( entry >> 29 == 0x05 ) { /* Quad array */
            count += ((((entry >> 25) & 0x0F)+1)<<1);
        } else { /* Polygon */
            struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
            while( poly != NULL ) {
                if( poly->vertex_count != 0 )
                    count += poly->vertex_count-2;
                poly = poly->sub_next;
            }
        }
    }
    return count;
}

static void sort_add_triangle( struct sort_triangle *triangle, struct polygon_struct *poly, int index )
{
    struct vertex_struct *vertexes = &pvr2_scene.vertex_array[poly->vertex_index+index];
    triangle->poly = poly;
    triangle->triangle_num = index;

    /* Compute triangle bounding-box */
    triangle->bounds[0] = MIN3(vertexes[0].x,vertexes[1].x,vertexes[2].x);
    triangle->bounds[1] = MAX3(vertexes[0].x,vertexes[1].x,vertexes[2].x);
    triangle->bounds[2] = MIN3(vertexes[0].y,vertexes[1].y,vertexes[2].y);
    triangle->bounds[3] = MAX3(vertexes[0].y,vertexes[1].y,vertexes[2].y);
    triangle->bounds[4] = MIN3(vertexes[0].z,vertexes[1].z,vertexes[2].z);
    triangle->bounds[5] = MAX3(vertexes[0].z,vertexes[1].z,vertexes[2].z);

    /* Compute plane equation */
    float sx = vertexes[1].x - vertexes[0].x;
    float sy = vertexes[1].y - vertexes[0].y;
    float sz = vertexes[1].z - vertexes[0].z;
    float tx = vertexes[2].x - vertexes[0].x;
    float ty = vertexes[2].y - vertexes[0].y;
    float tz = vertexes[2].z - vertexes[0].z;
    triangle->mx = sy*tz - sz*ty;
    triangle->my = sz*tx - sx*tz;
    triangle->mz = sx*ty - sy*tx;
    triangle->d = -vertexes[0].x*triangle->mx - 
                  vertexes[0].y*triangle->my - 
                  vertexes[0].z*triangle->mz;
}



/**
 * Extract a triangle list from the tile (basically indexes into the polygon list, plus
 * computing maxz while we go through it
 */
int sort_extract_triangles( pvraddr_t tile_entry, struct sort_triangle *triangles )
{
    uint32_t *tile_list = (uint32_t *)(pvr2_main_ram+tile_entry);
    int strip_count;
    struct polygon_struct *poly;
    int count = 0, i;

    while(1) {
        uint32_t entry = *tile_list++;
        switch( entry >> 28 ) {
        case 0x0F:
            return count; // End-of-list
        case 0x0E:
            tile_list = (uint32_t *)(pvr2_main_ram + (entry&0x007FFFFF));
            break;
        case 0x08: case 0x09:
            strip_count = ((entry >> 25) & 0x0F)+1;
            poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
            while( strip_count > 0 ) {
                assert( poly != NULL );
                if( poly->vertex_count != 0 ) {
                    /* Triangle could point to a strip, but we only want
                     * the first one in this case
                     */
                    sort_add_triangle( &triangles[count], poly, 0 );
                    count++;
                }
                poly = poly->next;
                strip_count--;
            }
            break;
        case 0x0A: case 0x0B:
            strip_count = ((entry >> 25) & 0x0F)+1;
            poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
            while( strip_count > 0 ) {
                assert( poly != NULL );
                for( i=0; i+2<poly->vertex_count && i < 2; i++ ) {
                    /* Note: quads can't have sub-polys */
                    sort_add_triangle( &triangles[count], poly, i );
                    count++;
                }
                poly = poly->next;
                strip_count--;
            }
            break;
        default:
            if( entry & 0x7E000000 ) {
                poly = pvr2_scene.buf_to_poly_map[entry&0x000FFFFF];
                /* FIXME: This could end up including a triangle that was
                 * excluded from the tile, if it is part of a strip that
                 * still has some other triangles in the tile.
                 * (This couldn't happen with TA output though).
                 */
                while( poly != NULL ) {
                    for( i=0; i+2<poly->vertex_count; i++ ) {
                        sort_add_triangle( &triangles[count], poly, i );
                        count++;
                    }
                    poly = poly->sub_next;
                }
            }
        }
    }       

}

void sort_render_triangles( struct sort_triangle **triangles, int num_triangles )
{
    int i;
    for( i=0; i<num_triangles; i++ ) {
        gl_render_triangle(triangles[i]->poly, triangles[i]->triangle_num);
    }
}

static int sort_triangle_compare( const void *a, const void *b ) 
{
    const struct sort_triangle *tri1 = a;
    const struct sort_triangle *tri2 = b;
    if( tri1->bounds[5] <= tri2->bounds[4] ) 
        return 1; /* tri1 is entirely under tri2 */
    else if( tri2->bounds[5] <= tri1->bounds[4] )
        return -1;  /* tri2 is entirely under tri1 */
    else if( tri1->bounds[1] <= tri2->bounds[0] ||
             tri2->bounds[1] <= tri1->bounds[0] ||
             tri1->bounds[3] <= tri2->bounds[2] ||
             tri2->bounds[3] <= tri1->bounds[2] )
        return 0; /* tri1 and tri2 don't actually overlap at all */
    else { 
        struct vertex_struct *tri2v = &pvr2_scene.vertex_array[tri2->poly->vertex_index + tri2->triangle_num];
        float v[3];
        int i;
        for( i=0; i<3; i++ ) {
            v[i] = tri1->mx * tri2v[i].x + tri1->my * tri2v[i].y + tri1->mz * tri2v[i].z + tri1->d;
            if( v[i] > -EPSILON && v[i] < EPSILON ) v[i] = 0;
        }
        if( v[0] == 0 && v[1] == 0 && v[2] == 0 ) {
            return 0; /* coplanar */
        }
        if( (v[0] >=0 && v[1] >= 0 && v[2] >= 0) ||
            (v[0] <= 0 && v[1] <= 0 && v[2] <= 0) ) {
            /* Tri is on one side of the plane. Pick an arbitrary point to determine which side */
            float t1z = -(tri1->mx * tri2v[0].x + tri1->my * tri2v[0].y + tri1->d) / tri1->mz;
            return tri2v[0].z - t1z;
        }
        
        /* If the above test failed, then tri2 intersects tri1's plane. This
         * doesn't necessarily mean the triangles intersect (although they may).
         * For now just return 0, and come back to this later as it's a fairly
         * uncommon case in practice. 
         */
        return 0;
    }
}

/**
 * This is pretty much a standard merge sort (Unfortunately can't depend on
 * the system to provide one. Note we can't use quicksort here - the sort
 * must be stable to preserve the order of coplanar triangles.
 */
static void sort_triangles( struct sort_triangle **triangles, int num_triangles, struct sort_triangle **out )
{
    if( num_triangles > 2 ) {
        int l = num_triangles>>1, r=num_triangles-l, i=0,j=0;
        struct sort_triangle *left[l];
        struct sort_triangle *right[r];
        sort_triangles( triangles, l, left );
        sort_triangles( triangles+l, r, right );
        
        /* Merge */
        while(1) {
            if( sort_triangle_compare(left[i], right[j]) <= 0 ) {
                *out++ = left[i++];
                if( i == l ) {
                    memcpy( out, &right[j], (r-j)*sizeof(struct sort_triangle *) );        
                    break;
                }
            } else {
                *out++ = right[j++];
                if( j == r ) {
                    memcpy( out, &left[i], (l-i)*sizeof(struct sort_triangle *) );
                    break;
                }
            }
        }
    } else if( num_triangles == 2 ) {
        if( sort_triangle_compare(triangles[0], triangles[1]) <= 0 ) {
            out[0] = triangles[0];
            out[1] = triangles[1];
        } else {
            struct sort_triangle *tmp = triangles[0];
            out[0] = triangles[1];
            out[1] = tmp;
        }
    } else {
        out[0] = triangles[0];
    }
} 

void render_autosort_tile( pvraddr_t tile_entry, int render_mode ) 
{
    int num_triangles = sort_count_triangles(tile_entry);
    if( num_triangles == 0 ) {
        return; /* nothing to do */
    } else if( num_triangles == 1 ) { /* Triangle can hardly overlap with itself */
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_GEQUAL);
        gl_render_tilelist(tile_entry, FALSE);
    } else { /* Ooh boy here we go... */
        int i;
        struct sort_triangle triangles[num_triangles+1];
        struct sort_triangle *triangle_order[num_triangles+1];
        triangles[num_triangles].poly = (void *)SENTINEL;
        for( i=0; i<num_triangles; i++ ) {
            triangle_order[i] = &triangles[i];
        }
        int extracted_triangles = sort_extract_triangles(tile_entry, triangles);
        assert( extracted_triangles <= num_triangles );
        sort_triangles( triangle_order, extracted_triangles, triangle_order );
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_GEQUAL);
        sort_render_triangles(triangle_order, extracted_triangles);
        assert( triangles[num_triangles].poly == (void *)SENTINEL );
    }
}
