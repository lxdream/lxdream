/**
 * $Id: texcache.c,v 1.1 2006-03-13 12:39:07 nkeynes Exp $
 *
 * Texture cache. Responsible for maintaining a working set of OpenGL 
 * textures. 
 *
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

#include <assert.h>
#include "pvr2/pvr2.h"

/** Specifies the maximum number of OpenGL
 * textures we're willing to have open at a time. If more are
 * needed, textures will be evicted in LRU order.
 */
#define MAX_TEXTURES 64

/**
 * Data structure:
 *
 * Main operations:
 *    find entry by texture_addr
 *    add new entry
 *    move entry to tail of lru list
 *    remove entry
 */

typedef signed short texcache_entry_index;
#define EMPTY_ENTRY -1

static texcache_entry_index texcache_free_ptr;
static GLuint texcache_free_list[MAX_TEXTURES];

typedef struct texcache_entry {
    uint32_t texture_addr;
    int width, height, mode;
    GLuint texture_id;
    texcache_entry_index next;
    uint32_t lru_count;
} *texcache_entry_t;

static uint8_t texcache_page_lookup[PVR2_RAM_PAGES];
static uint32_t texcache_active_ptr;
static uint32_t texcache_ref_counter;
static struct texcache_entry texcache_active_list[MAX_TEXTURES];

/**
 * Initialize the texture cache. Note that the GL context must have been
 * initialized before calling this function.
 */
void texcache_init( )
{
    int i;
    GLuint texids[MAX_TEXTURES];
    glGenTextures( MAX_TEXTURES, texids );
    for( i=0; i<PVR2_RAM_PAGES; i++ ) {
	texcache_page_lookup[i] = EMPTY_ENTRY;
    }
    for( i=0; i<MAX_TEXTURES; i++ ) {
	texcache_active_list[i].texture_id = texids[i];
	texcache_free_list[i] = i;
    }
    texcache_free_ptr = 0;
    texcache_ref_counter = 0;
}

/**
 * Flush all textures from the cache, returning them to the free list.
 */
void texcache_flush( )
{
    int i;
    /* clear structures */
    for( i=0; i<PVR2_RAM_PAGES; i++ ) {
	texcache_page_lookup[i] = EMPTY_ENTRY;
    }
    for( i=0; i<MAX_TEXTURES; i++ ) {
	texcache_free_list[i] = i;
    }
    texcache_free_ptr = 0;
    texcache_ref_counter = 0;
}

/**
 * Flush all textures and delete. The cache will be non-functional until
 * the next call to texcache_init(). This would typically be done if
 * switching GL targets.
 */    
void texcache_shutdown( )
{
    GLuint texids[MAX_TEXTURES];
    int i;
    texcache_flush();
    
    for( i=0; i<MAX_TEXTURES; i++ ) {
	texids[i] = texcache_active_list[i].texture_id;
    }
    glDeleteTextures( MAX_TEXTURES, texids );
}

/**
 * Evict all textures contained in the page identified by a texture address.
 */
void texcache_invalidate_page( uint32_t texture_addr ) {
    uint32_t texture_page = texture_addr >> 12;
    texcache_entry_index idx = texcache_page_lookup[texture_page];
    if( idx == EMPTY_ENTRY )
	return;
    assert( texcache_free_ptr > 0 );
    do {
	texcache_entry_t entry = &texcache_active_list[idx];	
	/* release entry */
	texcache_free_ptr--;
	texcache_free_list[texcache_free_ptr] = idx;
	idx = entry->next;
	entry->next = EMPTY_ENTRY;
    } while( idx != EMPTY_ENTRY );
    texcache_page_lookup[texture_page] = EMPTY_ENTRY;
}

/**
 * Evict a single texture from the cache.
 * @return the slot of the evicted texture.
 */
static texcache_entry_index texcache_evict( void )
{
    /* Full table scan - take over the entry with the lowest lru value */
    texcache_entry_index slot = 0;
    int lru_value = texcache_active_list[0].lru_count;
    int i;
    for( i=1; i<MAX_TEXTURES; i++ ) {
	/* FIXME: account for rollover */
	if( texcache_active_list[i].lru_count < lru_value ) {
	    slot = i;
	    lru_value = texcache_active_list[i].lru_count;
	}
    }
    
    /* Remove the selected slot from the lookup table */
    uint32_t evict_page = texcache_active_list[slot].texture_addr;
    texcache_entry_index replace_next = texcache_active_list[slot].next;
    texcache_active_list[slot].next = EMPTY_ENTRY; /* Just for safety */
    if( texcache_page_lookup[evict_page] == slot ) {
	texcache_page_lookup[evict_page] = replace_next;
    } else {
	texcache_entry_index idx = texcache_page_lookup[evict_page];
	texcache_entry_index next;
	do {
	    next = texcache_active_list[idx].next;
	    if( next == slot ) {
		texcache_active_list[idx].next = replace_next;
		break;
	    }
	    idx = next;
	} while( next != EMPTY_ENTRY );
    }
    return slot;
}

/**
 * Load texture data from the given address and parameters into the currently
 * bound OpenGL texture.
 */
static texcache_load_texture( uint32_t texture_addr, int width, int height,
			      int mode ) {
    uint32_t bytes = width * height;
    int bpp = 2;
    GLint intFormat, format, type;
    switch( mode & PVR2_TEX_FORMAT_MASK ) {
    case PVR2_TEX_FORMAT_ARGB1555:
	bytes <<= 1;
	intFormat = GL_RGB5_A1;
	format = GL_RGBA;
	type = GL_UNSIGNED_SHORT_5_5_5_1;
	break;
    case PVR2_TEX_FORMAT_RGB565:
	bytes <<= 1;
	intFormat = GL_RGBA;
	format = GL_RGBA;
	type = GL_UNSIGNED_SHORT_5_6_5;
	break;
    case PVR2_TEX_FORMAT_ARGB4444:
	bytes <<= 1;
	intFormat = GL_RGBA4;
	format = GL_RGBA;
	type = GL_UNSIGNED_SHORT_4_4_4_4;
	break;
    case PVR2_TEX_FORMAT_YUV422:
	ERROR( "YUV textures not supported" );
	break;
    case PVR2_TEX_FORMAT_BUMPMAP:
	ERROR( "Bumpmap not supported" );
	break;
    case PVR2_TEX_FORMAT_IDX4:
	/* Supported? */
	bytes >>= 1;
	intFormat = GL_INTENSITY4;
	format = GL_COLOR_INDEX;
	type = GL_UNSIGNED_BYTE;
	bpp = 0;
	break;
    case PVR2_TEX_FORMAT_IDX8:
	intFormat = GL_INTENSITY8;
	format = GL_COLOR_INDEX;
	type = GL_UNSIGNED_BYTE;
	bpp = 1;
	break;
    }

    unsigned char data[bytes];
    /* load data from image, detwiddling/uncompressing as required */
    if( PVR2_TEX_IS_COMPRESSED(mode) ) {
	ERROR( "VQ Compression not supported" );
    } else {
	pvr2_vram64_read( &data, texture_addr, bytes );
	if( PVR2_TEX_IS_TWIDDLED(mode) ) {
	    /* Untwiddle */
	}
    }
    /* Pass to GL */
    glTexImage2D( GL_TEXTURE_2D, 0, intFormat, width, height, 0, format, type,
		  data );
}

/**
 * Return a texture ID for the texture specified at the supplied address
 * and given parameters (the same sequence of bytes could in theory have
 * multiple interpretations). We use the texture address as the primary
 * index, but allow for multiple instances at each address. The texture
 * will be bound to the GL_TEXTURE_2D target before being returned.
 * 
 * If the texture has already been bound, return the ID to which it was
 * bound. Otherwise obtain an unused texture ID and set it up appropriately.
 */
GLuint texcache_get_texture( uint32_t texture_addr, int width, int height,
			     int mode )
{
    uint32_t texture_page = texture_addr >> 12;
    texcache_entry_index idx = texcache_page_lookup[texture_page];
    while( idx != EMPTY_ENTRY ) {
	texcache_entry_t entry = &texcache_active_list[idx];
	if( entry->texture_addr == texture_addr &&
	    entry->mode == mode &&
	    entry->width == width &&
	    entry->height == height ) {
	    entry->lru_count = texcache_ref_counter++;
	    glBindTexture( GL_TEXTURE_2D, entry->texture_id );
	    return entry->texture_id;
	}
        idx = entry->next;
    }

    /* Not found - check the free list */
    int slot = 0;

    if( texcache_free_ptr < MAX_TEXTURES ) {
	slot = texcache_free_list[texcache_free_ptr++];
    } else {
	slot = texcache_evict();
    }

    /* Construct new entry */
    texcache_active_list[slot].texture_addr = texture_addr;
    texcache_active_list[slot].width = width;
    texcache_active_list[slot].height = height;
    texcache_active_list[slot].mode = mode;
    texcache_active_list[slot].lru_count = texcache_ref_counter++;

    /* Add entry to the lookup table */
    texcache_active_list[slot].next = texcache_page_lookup[texture_page];
    texcache_page_lookup[texture_page] = slot;

    /* Construct the GL texture */
    GLuint texid = texcache_free_list[texcache_free_ptr++];
    glBindTexture( GL_TEXTURE_2D, texid );
    texcache_load_texture( texture_addr, width, height, mode );
    
    return texcache_active_list[slot].texture_id;
}
