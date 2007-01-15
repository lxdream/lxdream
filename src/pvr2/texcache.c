/**
 * $Id: texcache.c,v 1.14 2007-01-15 12:57:42 nkeynes Exp $
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
#define EMPTY_ENTRY 0xFF

static texcache_entry_index texcache_free_ptr = 0;
static GLuint texcache_free_list[MAX_TEXTURES];

typedef struct texcache_entry {
    uint32_t texture_addr;
    int width, height, mode;
    GLuint texture_id;
    texcache_entry_index next;
    uint32_t lru_count;
} *texcache_entry_t;

static uint8_t texcache_page_lookup[PVR2_RAM_PAGES];
static uint32_t texcache_ref_counter;
static struct texcache_entry texcache_active_list[MAX_TEXTURES];

/**
 * Initialize the texture cache.
 */
void texcache_init( )
{
    int i;
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
 * Setup the initial texture ids (must be called after the GL context is
 * prepared)
 */
void texcache_gl_init( )
{
    int i;
    GLuint texids[MAX_TEXTURES];

    glGenTextures( MAX_TEXTURES, texids );
    for( i=0; i<MAX_TEXTURES; i++ ) {
	texcache_active_list[i].texture_id = texids[i];
    }
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
    assert( texcache_free_ptr >= 0 );
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
    uint32_t evict_page = texcache_active_list[slot].texture_addr >> 12;
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

static void detwiddle_pal8_to_32(int x1, int y1, int size, int totsize,
				 char **in, uint32_t *out, uint32_t *pal) {
    if (size == 1) {
	out[y1 * totsize + x1] = pal[**in];
	(*in)++;
    } else {
	int ns = size>>1;
	detwiddle_pal8_to_32(x1, y1, ns, totsize, in, out, pal);
	detwiddle_pal8_to_32(x1, y1+ns, ns, totsize, in, out, pal);
	detwiddle_pal8_to_32(x1+ns, y1, ns, totsize, in, out, pal);
	detwiddle_pal8_to_32(x1+ns, y1+ns, ns, totsize, in, out, pal);
    }
}

static void detwiddle_pal8_to_16(int x1, int y1, int size, int totsize,
				 char **in, uint16_t *out, uint16_t *pal) {
    if (size == 1) {
	out[y1 * totsize + x1] = pal[**in];
	(*in)++;
    } else {
	int ns = size>>1;
	detwiddle_pal8_to_16(x1, y1, ns, totsize, in, out, pal);
	detwiddle_pal8_to_16(x1, y1+ns, ns, totsize, in, out, pal);
	detwiddle_pal8_to_16(x1+ns, y1, ns, totsize, in, out, pal);
	detwiddle_pal8_to_16(x1+ns, y1+ns, ns, totsize, in, out, pal);
    }
}

static void detwiddle_16_to_16(int x1, int y1, int size, int totsize,
			       uint16_t **in, uint16_t *out ) {
    if (size == 1) {
	out[y1 * totsize + x1] = **in;
	(*in)++;
    } else {
	int ns = size>>1;
	detwiddle_16_to_16(x1, y1, ns, totsize, in, out);
	detwiddle_16_to_16(x1, y1+ns, ns, totsize, in, out);
	detwiddle_16_to_16(x1+ns, y1, ns, totsize, in, out);
	detwiddle_16_to_16(x1+ns, y1+ns, ns, totsize, in, out);
    }
}
    
#define VQ_CODEBOOK_SIZE 2048 /* 256 entries * 4 pixels per quad * 2 byte pixels */

struct vq_codebook {
    uint16_t quad[256][4];
};

static void detwiddle_vq_to_16(int x1, int y1, int size, int totsize,
		   uint8_t **in, uint16_t *out, struct vq_codebook *codebook ) {
    if( size == 2 ) {
	uint8_t code = **in;
	(*in)++;
	out[y1 * totsize + x1] = codebook->quad[code][0];
	out[y1 * totsize + x1 + 1] = codebook->quad[code][1];
	out[(y1+1) * totsize + x1] = codebook->quad[code][2];
	out[(y1+1) * totsize + x1 + 1] = codebook->quad[code][3];
    } else {
	int ns = size>>1;
	detwiddle_vq_to_16(x1, y1, ns, totsize, in, out, codebook);
	detwiddle_vq_to_16(x1, y1+ns, ns, totsize, in, out, codebook);
	detwiddle_vq_to_16(x1+ns, y1, ns, totsize, in, out, codebook);
	detwiddle_vq_to_16(x1+ns, y1+ns, ns, totsize, in, out, codebook);
    }	
}

static void vq_get_codebook( struct vq_codebook *codebook, 
				uint16_t *input )
{
    /* Detwiddle the codebook, for the sake of my own sanity if nothing else */
    uint16_t *p = (uint16_t *)input;
    int i;
    for( i=0; i<256; i++ ) {
	codebook->quad[i][0] = *p++;
	codebook->quad[i][2] = *p++;
	codebook->quad[i][1] = *p++;
	codebook->quad[i][3] = *p++;
    }
}    


static void vq_decode( int width, int height, char *input, uint16_t *output,
		       struct vq_codebook *codebook, int twiddled ) {
    int i,j;
    
    uint8_t *c = (uint8_t *)input;
    if( twiddled ) {
	detwiddle_vq_to_16( 0, 0, width, width, &c, output, codebook );
    } else {
	for( j=0; j<height; j+=2 ) {
	    for( i=0; i<width; i+=2 ) {
		uint8_t code = *c;
		output[i + j*width] = codebook->quad[code][0];
		output[i + 1 + j*width] = codebook->quad[code][1];
		output[i + (j+1)*width] = codebook->quad[code][2];
		output[i + 1 + (j+1)*width] = codebook->quad[code][3];
	    }
	}
    }
}

static inline uint32_t yuv_to_rgb32( float y, float u, float v )
{
    u -= 128;
    v -= 128;
    int r = (int)(y + v*1.375);
    int g = (int)(y - u*0.34375 - v*0.6875);
    int b = (int)(y + u*1.71875);
    if( r > 255 ) { r = 255; } else if( r < 0 ) { r = 0; }
    if( g > 255 ) { g = 255; } else if( g < 0 ) { g = 0; }
    if( b > 255 ) { b = 255; } else if( b < 0 ) { b = 0; }
    return 0xFF000000 | (r<<16) | (g<<8) | (b);
}


/**
 * Convert non-twiddled YUV texture data into RGB32 data - most GL implementations don't
 * directly support this format unfortunately. The input data is formatted as
 * 32 bits = 2 horizontal pixels, UYVY. This is currently done rather inefficiently
 * in floating point.
 */
static void yuv_decode( int width, int height, uint32_t *input, uint32_t *output )
{
    int x, y;
    uint32_t *p = input;
    for( y=0; y<height; y++ ) {
	for( x=0; x<width; x+=2 ) {
	    float u = (float)(*p & 0xFF);
	    float y0 = (float)( (*p>>8)&0xFF );
	    float v = (float)( (*p>>16)&0xFF );
	    float y1 = (float)( (*p>>24)&0xFF );
	    *output++ = yuv_to_rgb32( y0, u, v ); 
	    *output++ = yuv_to_rgb32( y1, u, v );
	    p++;
	}
    }
}

/**
 * Load texture data from the given address and parameters into the currently
 * bound OpenGL texture.
 */
static texcache_load_texture( uint32_t texture_addr, int width, int height,
			      int mode ) {
    int bpp_shift = 1; /* bytes per (output) pixel as a power of 2 */
    GLint intFormat, format, type;
    int tex_format = mode & PVR2_TEX_FORMAT_MASK;
    struct vq_codebook codebook;
    GLint filter = GL_LINEAR;

    /* Decode the format parameters */
    switch( tex_format ) {
    case PVR2_TEX_FORMAT_IDX4:
	ERROR( "4-bit indexed textures not supported" );
    case PVR2_TEX_FORMAT_IDX8:
	/* For indexed-colour modes, we need to lookup the palette control
	 * word to determine the de-indexed texture format.
	 */
	switch( MMIO_READ( PVR2, RENDER_PALETTE ) & 0x03 ) {
	case 0: /* ARGB1555 */
	    intFormat = GL_RGB5_A1;
	    format = GL_RGBA;
	    type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	    break;
	case 1:  /* RGB565 */
	    intFormat = GL_RGB;
	    format = GL_RGB;
	    type = GL_UNSIGNED_SHORT_5_6_5_REV;
	    break;
	case 2: /* ARGB4444 */
	    intFormat = GL_RGBA4;
	    format = GL_BGRA;
	    type = GL_UNSIGNED_SHORT_4_4_4_4_REV;
	    break;
	case 3: /* ARGB8888 */
	    intFormat = GL_RGBA8;
	    format = GL_BGRA;
	    type = GL_UNSIGNED_INT_8_8_8_8_REV;
	    bpp_shift = 2;
	    break;
	}
	break;
	    
    case PVR2_TEX_FORMAT_ARGB1555:
	intFormat = GL_RGB5_A1;
	format = GL_RGBA;
	type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	break;
    case PVR2_TEX_FORMAT_RGB565:
	intFormat = GL_RGB;
	format = GL_RGB;
	type = GL_UNSIGNED_SHORT_5_6_5_REV;
	break;
    case PVR2_TEX_FORMAT_ARGB4444:
	intFormat = GL_RGBA4;
	format = GL_BGRA;
	type = GL_UNSIGNED_SHORT_4_4_4_4_REV;
	break;
    case PVR2_TEX_FORMAT_YUV422:
	/* YUV422 isn't directly supported by most implementations, so decode
	 * it to a (reasonably) standard ARGB32.
	 */
	bpp_shift = 2;
	intFormat = GL_RGBA8;
	format = GL_BGRA;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	break;
    case PVR2_TEX_FORMAT_BUMPMAP:
	ERROR( "Bumpmap not supported" );
	break;
    }
	
    if( PVR2_TEX_IS_STRIDE(mode) ) {
	/* Stride textures cannot be mip-mapped, compressed, indexed or twiddled */
	uint32_t stride = (MMIO_READ( PVR2, RENDER_TEXSIZE ) & 0x003F) << 5;
	char data[(width*height) << bpp_shift];
	if( tex_format == PVR2_TEX_FORMAT_YUV422 ) {
	    char tmp[(width*height)<<1];
	    pvr2_vram64_read_stride( tmp, width<<1, texture_addr, stride<<1, height );
	    yuv_decode(width, height, (uint32_t *)tmp, (uint32_t *)data );
	} else {
	    pvr2_vram64_read_stride( data, width<<bpp_shift, texture_addr, stride<<bpp_shift, height );
	}
	glTexImage2D( GL_TEXTURE_2D, 0, intFormat, width, height, 0, format, type, data );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	return;
    } 

    int level=0, last_level = 0, mip_width = width, mip_height = height, mip_bytes;
    if( PVR2_TEX_IS_MIPMAPPED(mode) ) {
	int i;
	for( i=0; 1<<(i+1) < width; i++ );
	last_level = i;
	mip_width = width >> i;
	mip_height= height >> i;
	filter = GL_LINEAR_MIPMAP_LINEAR;
    }
    mip_bytes = (mip_width * mip_width) << bpp_shift;

    if( PVR2_TEX_IS_COMPRESSED(mode) ) {
	uint16_t tmp[VQ_CODEBOOK_SIZE];
	pvr2_vram64_read( (char *)tmp, texture_addr, VQ_CODEBOOK_SIZE );
	texture_addr += VQ_CODEBOOK_SIZE;
	vq_get_codebook( &codebook, tmp );
    }

    for( level=last_level; level>= 0; level-- ) {
	char data[mip_bytes];
	/* load data from image, detwiddling/uncompressing as required */
	if( tex_format == PVR2_TEX_FORMAT_IDX8 ) {
	    int inputlength = mip_bytes >> bpp_shift;
	    int bank = (mode >> 25) &0x03;
	    char *palette = mmio_region_PVR2PAL.mem + (bank * (256 << bpp_shift));
	    char tmp[inputlength];
	    char *p = tmp;
	    pvr2_vram64_read( tmp, texture_addr, inputlength );
	    if( bpp_shift == 2 ) {
		detwiddle_pal8_to_32( 0, 0, mip_width, mip_width, &p, 
				      (uint32_t *)data, (uint32_t *)palette );
	    } else {
		detwiddle_pal8_to_16( 0, 0, mip_width, mip_width, &p,
				      (uint16_t *)data, (uint16_t *)palette );
	    }
	} else if( tex_format == PVR2_TEX_FORMAT_YUV422 ) {
	    int inputlength = ((mip_width*mip_height)<<1);
	    char tmp[inputlength];
	    pvr2_vram64_read( tmp, texture_addr, inputlength );
	    yuv_decode( mip_width, mip_height, (uint32_t *)tmp, (uint32_t *)data );
	} else if( PVR2_TEX_IS_COMPRESSED(mode) ) {
	    int inputlength = ((mip_width*mip_height) >> 2);
	    char tmp[inputlength];
	    pvr2_vram64_read( tmp, texture_addr, inputlength );
	    vq_decode( mip_width, mip_height, tmp, (uint16_t *)data, &codebook, 
		       PVR2_TEX_IS_TWIDDLED(mode) );
	} else if( PVR2_TEX_IS_TWIDDLED(mode) ) {
	    char tmp[mip_bytes];
	    uint16_t *p = (uint16_t *)tmp;
	    pvr2_vram64_read( tmp, texture_addr, mip_bytes );
	    /* Untwiddle */
	    detwiddle_16_to_16( 0, 0, mip_width, mip_width, &p, (uint16_t *)data );
	} else {
	    pvr2_vram64_read( data, texture_addr, mip_bytes );
	}
	    
	if( PVR2_TEX_IS_MIPMAPPED(mode) && mip_width == 2 ) {
	    /* Opengl requires a 1x1 texture, but the PVR2 doesn't. This should
	     * strictly speaking be the average of the 2x2 texture, but we're
	     * lazy at the moment */
	    glTexImage2D( GL_TEXTURE_2D, level+1, intFormat, 1, 1, 0, format, type, data );
	}

	/* Pass to GL */
	glTexImage2D( GL_TEXTURE_2D, level, intFormat, mip_width, mip_height, 0, format, type,
		      data );
	texture_addr += mip_bytes;
	mip_width <<= 1;
	mip_height <<= 1;
	mip_bytes <<= 2;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
    glBindTexture( GL_TEXTURE_2D, texcache_active_list[slot].texture_id );
    texcache_load_texture( texture_addr, width, height, mode );
    
    return texcache_active_list[slot].texture_id;
}
