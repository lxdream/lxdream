/**
 * $Id$
 *
 * Scene-save support. This is mainly for test/debug purposes.
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

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include "pvr2/pvr2.h"
#include "dreamcast.h"

/**
 * Size of pages for the purposes of saving - this has nothing to do with the 
 * actual page size, of course.
 */
#define SAVE_PAGE_SIZE 1024
#define SAVE_PAGE_COUNT 8192

extern char *video_base;

/* Determine pages of memory to save. Start walking from the render tilemap
 * data and build up a page list
 */
static void pvr2_find_referenced_pages( char *pages )
{
    /* Dummy implementation - save everything */
    memset( pages, 1, SAVE_PAGE_COUNT );
}

/**
 * Save the current rendering data to a file for later analysis.
 * @return 0 on success, non-zero on failure.
 */
int pvr2_render_save_scene( const gchar *filename )
{
    struct header {
	char magic[16];
	uint32_t version;
	uint32_t timestamp;
	uint32_t frame_count;
    } scene_header;

    char page_map[SAVE_PAGE_COUNT];
    int i,j;
    pvr2_find_referenced_pages(page_map);
    
    FILE *f = fopen( filename, "wo" ); 
    if( f == NULL ) {
	ERROR( "Unable to open file '%s' to write scene data: %s", filename, strerror(errno) );
	return -1;
    }

    /* Header */
    memcpy( scene_header.magic, SCENE_SAVE_MAGIC, 16 );
    scene_header.version = SCENE_SAVE_VERSION;
    scene_header.timestamp = time(NULL);
    scene_header.frame_count = pvr2_get_frame_count();
    fwrite( &scene_header, sizeof(scene_header), 1, f );

    /* PVR2 registers - could probably be more specific, but doesn't 
     * really use a lot of space. Loader is assumed to know which
     * registers actually need to be set.
     */
    fwrite( mmio_region_PVR2.mem, 0x1000, 1, f );
    fwrite( mmio_region_PVR2PAL.mem, 0x1000, 1, f );

    /* Write out the VRAM pages we care about */
    for( i=0; i<SAVE_PAGE_COUNT; i++ ) {
	if( page_map[i] != 0 ) {
	    for( j=i+1; j<SAVE_PAGE_COUNT && page_map[j] != 0; j++ );
	    /* Write region from i..j-1 */
	    uint32_t start = i * SAVE_PAGE_SIZE;
	    uint32_t length = (j-i) * SAVE_PAGE_SIZE;
	    fwrite( &start, sizeof(uint32_t), 1, f );
	    fwrite( &length, sizeof(uint32_t), 1, f );
	    fwrite( video_base + start, 1, length, f );
	    i = j-1;
	}
    }
    /* Write out the EOF marker */
    uint32_t eof = 0xFFFFFFFF;
    fwrite( &eof, sizeof(uint32_t), 1, f );
    fclose( f );
    return 0;
}
