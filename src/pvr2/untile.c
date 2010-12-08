/**
 * $Id$
 *
 * Process the tile + polygon data to extract a list of polygons that can
 * be rendered directly without tiling.
 *
 * Copyright (c) 2010 Nathan Keynes.
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

#include <stdlib.h>
#include "pvr2/pvr2.h"
#include "pvr2/scene.h"
#include "pvr2/tileiter.h"

static int sort_polydata( const void *a, const void *b )
{
    uint32_t idxa = *(const uint32_t*)a;
    uint32_t idxb = *(const uint32_t*)b;
    return pvr2_scene.poly_array[idxa].context - pvr2_scene.poly_array[idxb].context;
}

gboolean untile_list( struct tile_segment *segment, int pass, int list )
{
    int tile_width = pvr2_scene.buffer_width >> 5;
    int tile_height = pvr2_scene.buffer_height >> 5;
    tileiter tile_map[tile_width][tile_height];

    memset(tile_map, 0, tile_width*tile_height*sizeof(uint32_t *));

    /* 1. Construct the tile map for the last/pass */
    int last_x = -1, last_y = -1, tile_pass;
    do {
        int tile_x = SEGMENT_X(segment->control);
        int tile_y = SEGMENT_Y(segment->control);
        if( last_x == tile_x && last_y == tile_y ) {
            tile_pass++;
        } else {
            tile_pass = 0;
            last_x = tile_x;
            last_y = tile_y;
        }

        if( tile_pass == pass ) {
            uint32_t ptr = ((uint32_t *)segment)[list+1];
            if( IS_TILE_PTR(ptr) )
                TILEITER_BEGIN(tile_map[tile_x][tile_y], ptr);
        }
    } while( !IS_LAST_SEGMENT(segment++) );

    /* 2. Extract the polygon list, sorted by appearance. We assume the list
     * can be sorted by address, which makes this a lot simpler/faster.
     */
    uint32_t poly_present[pvr2_scene.poly_count];
    memset( poly_present, 0, sizeof(poly_present) );
    unsigned x,y, i, j, poly_count = 0;
    for( x = 0; x < tile_width; x++ ) {
        for( y = 0; y < tile_height; y++ ) {
            tileiter list = tile_map[x][y];
            while( !TILEITER_DONE(list) ) {
                struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[TILEITER_POLYADDR(list)];
                if( poly ) {
                    poly_present[POLY_NO(poly)] = 1;
                }
                TILEITER_NEXT(list);
            }
        }
    }
    /* Collapse array into a set of polygon indexes and then sort it */
    for( x=0; x<pvr2_scene.poly_count; x++ ) {
        if( poly_present[x] ) {
            poly_present[poly_count++] = x;
        }
    }
    qsort(poly_present, poly_count, sizeof(uint32_t), sort_polydata);

    /* 3. Process each polygon in the list. Extract the bounds, and check
     * each tile in which it should appear - if missing, the poly is clipped
     * (and we need to construct appropriate bounds).
     */
    for( i=0; i<poly_count; i++ ) {
        struct tile_bounds poly_bounds = {tile_width, tile_height, -1, -1};
        struct tile_bounds clip_bounds = {0, 0, tile_width, tile_height};
        struct polygon_struct *poly = &pvr2_scene.poly_array[poly_present[i]];
        uint32_t poly_addr = poly->context - ((uint32_t *)pvr2_main_ram);
        do {
            /* Extract tile bounds for the poly - we assume (since we have the
             * polygon at all) that it appears correctly in at least one tile.
             */
            struct vertex_struct *vert = &pvr2_scene.vertex_array[poly->vertex_index];
            for( j=0; j<poly->vertex_count; j++ ) {
                int tx, ty;
                if( vert[j].x < 0 ) tx = 0;
                else if( vert[j].x >= pvr2_scene.buffer_width ) tx = tile_width-1;
                else tx = ((int)vert[j].x)>>5;
                if( tx < poly_bounds.x1 ) poly_bounds.x1 = tx;
                if( tx > poly_bounds.x2 ) poly_bounds.x2 = tx;
                if( vert[j].y < 0 ) ty = 0;
                else if( vert[j].y >= pvr2_scene.buffer_height ) ty = tile_height-1;
                else ty = ((int)vert[j].y)>>5;
                if( ty < poly_bounds.y1 ) poly_bounds.y1 = ty;
                if( ty > poly_bounds.y2 ) poly_bounds.y2 = ty;
            }
            poly = poly->sub_next;
        } while( poly != NULL );
        if( poly_bounds.x1 == tile_width ) {
            continue; /* Polygon has been culled */
        }

        gl_render_poly(&pvr2_scene.poly_array[poly_present[i]], TRUE);
#if 0
        /* Search the tile map for the polygon */
        for( x = poly_bounds.x1; x <= poly_bounds.x2; x++ ) {
            for( y = poly_bounds.y1; y <= poly_bounds.y2; y++ ) {
                tileiter *list = &tile_map[x][y];


                /* Skip over earlier entries in the list, if any (can happen if
                 * we culled something, or had an empty polygon
                 */
                while( !TILEITER_DONE(*list) && TILEITER_POLYADDR(*list) < poly_addr )
                    TILEITER_NEXT(*list);
                if( TILEITER_POLYADDR(*list) == poly_addr ) {
                    /* Match */
                } else {
                    /* Clipped */
                }
            }
        }
#endif
    }
}


