/**
 * $Id$
 *
 * Tile iterator ADT. Defines an iterator that can be used to iterate through
 * a PVR2 tile list a polygon at a time.
 *
 * Note: The iterator functions are defined here to allow the compiler to
 * inline them if it wants to, but it's not always beneficial to do so
 * (hence we don't force them to be inlined)
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

#ifndef lxdream_tileiter_H
#define lxdream_tileiter_H 1

#include <assert.h>
#include "pvr2/pvr2.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * tileiter: iterator over individual polygons in a tile list.
 */
typedef struct tileiter {
    uint32_t *ptr;
    unsigned strip_count;
    unsigned poly_size;
    uint32_t poly_addr;
} tileiter;


#define TILEITER_IS_MODIFIED(it) ((*(it).ptr) & 0x01000000)
#define TILEITER_IS_TRIANGLE(it) (((*(it).ptr) >> 29) == 4)
#define TILEITER_IS_QUAD(it) (((*(it).ptr) >> 29) == 5)
#define TILEITER_IS_POLY(it) (((*(it).ptr) >> 31) == 0)

#define TILEITER_POLYADDR(it) ((it).poly_addr)
#define TILEITER_STRIPCOUNT(it) ((it).strip_count)
#define TILEITER_DONE(it) ((it).ptr == 0)
#define TILEITER_BEGIN(it, tileent) (tileiter_init(&it, tileent))
#define TILEITER_NEXT(it) (tileiter_next(&it))
#define FOREACH_TILE(it, seg) for( TILEITER_BEGIN(it, seg); !TILEITER_DONE(it); TILEITER_NEXT(it) )


/**
 * tileentryiter: iterator over entries in the tile list, where each entry is
 * a polygon, triangle set, or quad set.
 */
typedef struct tileentryiter {
    uint32_t *ptr;
    unsigned strip_count;
} tileentryiter;

#define TILEENTRYITER_POLYADDR(it) ((*((it).ptr)) & 0x001FFFFF)
#define TILEENTRYITER_STRIPCOUNT(it) ((it).strip_count)
#define TILEENTRYITER_DONE(it) ((it).ptr == 0)
#define TILEENTRYITER_BEGIN(it, tileent) (tileentryiter_init(&(it),tileent))
#define TILEENTRYITER_NEXT(it) (tileentryiter_next(&(it)))
#define FOREACH_TILEENTRY(it,seg) for( TILEENTRYITER_BEGIN(it,seg); !TILEENTRYITER_DONE(it); TILEENTRYITER_NEXT(it) )

/**************************** tileiter functions ****************************/

/**
 * Read the entry pointed to by it->ptr into the tileiter structure. If the
 * entry if a list pointer or invalid entry, the pointer will first be
 * updated to the next real entry. On end-of-list, sets ptr to NULL
 */
static void tileiter_read( tileiter *it )
{
    for(;;){
        uint32_t entry = *it->ptr;
        uint32_t tag = entry >> 29;
        if( tag < 6 ) {
            if( tag & 0x04 ) {
                int vertex_count = tag-1; /* 4 == tri, 5 == quad */
                int vertex_length = (entry >> 21) & 0x07;
                if( (entry & 0x01000000) && (pvr2_scene.shadow_mode == SHADOW_FULL) ) {
                    it->poly_size = 5 + (vertex_length<<1) * vertex_count;
                } else {
                    it->poly_size = 3 + vertex_length * vertex_count;
                }
                it->strip_count = ((entry >> 25) & 0x0F);
                it->poly_addr = entry & 0x001FFFFF;
                return;
            } else {
                /* Other polygon */
                it->strip_count = 0;
                it->poly_addr = entry & 0x001FFFFF;
                return;
            }
        } else {
            if( tag == 0x07 ) {
                if( entry & 0x10000000 ) {
                    it->ptr = NULL;
                    return;
                } else {
                    it->ptr = (uint32_t *)(pvr2_main_ram + (entry&0x007FFFFF));
                }
            } else if( tag == 6 ) {
                /* Illegal? Skip */
                it->ptr++;
            }
        }
    }
}

static void tileiter_init( tileiter *it, uint32_t segptr )
{
    if( IS_TILE_PTR(segptr) ) {
        it->ptr = (uint32_t *)(pvr2_main_ram + (segptr & 0x007FFFFF));
        tileiter_read(it);
    } else {
        it->ptr = 0;
    }
}

static inline void tileiter_next( tileiter *it )
{
    assert( it->ptr != NULL );
    if( it->strip_count > 0 ) {
        it->strip_count--;
        it->poly_addr += it->poly_size;
    } else {
        it->ptr++;
        tileiter_read(it);
    }
}


/************************* tileentryiter functions **************************/

/**
 * Read the entry pointed to by it->ptr, updating the pointer to point
 * to the next real element if the current value is not a polygon entry.
 */
static void tileentryiter_read( tileentryiter *it )
{
    for(;;){
        uint32_t entry = *it->ptr;
        uint32_t tag = entry >> 28;
        if( tag < 0x0C ) {
            if( tag & 0x08 ) {
                it->strip_count = ((entry >> 25) & 0x0F);
            } else {
                it->strip_count = 0;
            }
            return;
        } else {
            if( tag == 0x0F ) {
                it->ptr = NULL;
                return;
            } else if( tag == 0x0E ) {
                it->ptr = (uint32_t *)(pvr2_main_ram + (entry&0x007FFFFF));
                entry = *it->ptr;
            } else {
                /* Illegal? Skip */
                it->ptr++;
            }
        }
    }
}


static void tileentryiter_init( tileentryiter *it, uint32_t segptr )
{
    if( IS_TILE_PTR(segptr) ) {
        it->ptr = (uint32_t *)(pvr2_main_ram + (segptr & 0x007FFFFF));
        tileentryiter_read(it);
    } else {
        it->ptr = 0;
    }
}

static void inline tileentryiter_next( tileentryiter *it )
{
    it->ptr++;
    tileentryiter_read(it);
}


#ifdef __cplusplus
}
#endif

#endif /* !lxdream_tileiter_H */
