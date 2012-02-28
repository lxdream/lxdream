/**
 * $Id$
 *
 * Watchpoint support (for debugging)
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

#include <stdlib.h>
#include <string.h>
#include "mem.h"

struct watch_point {
    uint32_t start;
    uint32_t end;
    int flags;
};

struct watch_point *watch_arr = NULL;
int watch_count = 0, watch_capacity = 0;


watch_point_t mem_new_watch( uint32_t start, uint32_t end, int flags )
{
    int num;
    if( watch_arr == NULL ) {
        watch_capacity = 10;
        watch_arr = calloc( sizeof(struct watch_point), watch_capacity );
        num = 0;
    } else if( watch_count == watch_capacity ) {
        struct watch_point *tmp = realloc( watch_arr, sizeof(struct watch_point) * watch_capacity * 2 );
        if( tmp == NULL )
            return NULL;
        watch_arr = tmp;
        memset( &watch_arr[watch_capacity], 0, sizeof( struct watch_point ) * watch_capacity );
        num = watch_capacity;
        watch_capacity *= 2;
    } else {
        for( num=0; num<watch_capacity; num++ ) {
            if( watch_arr[num].flags == 0 )
                break;
        }
    }
    watch_arr[num].start = start & 0x1FFFFFFF;
    watch_arr[num].end = end & 0x1FFFFFFF;
    watch_arr[num].flags = flags;
    watch_count++;
    return &watch_arr[num];
}

void mem_delete_watch( watch_point_t watch )
{
    if( watch_arr == NULL )
        return;
    int num = watch - watch_arr;
    if( num < 0 || num >= watch_capacity )
        return;
    watch->start = watch->end = 0;
    watch->flags = 0;
    watch_count--;
}


watch_point_t mem_is_watched( uint32_t addr, int size, int op )
{
    int i, count;
    addr &= 0x1FFFFFFF;
    for( i=0, count=0; count< watch_count; i++ ) {
        if( watch_arr[i].flags == 0 )
            continue;
        count++;
        if( watch_arr[i].flags & op &&
                watch_arr[i].start < addr+size &&
                watch_arr[i].end >= addr ) {
            return &watch_arr[i];
        }
    }
    return NULL;
}

