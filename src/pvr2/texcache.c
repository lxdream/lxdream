/**
 * $Id$
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
#include <string.h>
#include "pvr2/pvr2.h"
#include "pvr2/pvr2mmio.h"

/** Specifies the maximum number of OpenGL
 * textures we're willing to have open at a time. If more are
 * needed, textures will be evicted in LRU order.
 */
#define MAX_TEXTURES 256

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

static texcache_entry_index texcache_free_ptr = 0;
static GLuint texcache_free_list[MAX_TEXTURES];

typedef struct texcache_entry {
    uint32_t texture_addr;
    int width, height, mode;
    GLuint texture_id;
    render_buffer_t buffer;
    texcache_entry_index next;
    uint32_t lru_count;
} *texcache_entry_t;

static texcache_entry_index texcache_page_lookup[PVR2_RAM_PAGES];
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
        texcache_active_list[i].texture_addr = -1;
        texcache_active_list[i].buffer = NULL;
        texcache_active_list[i].next = EMPTY_ENTRY;
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

void texcache_release_render_buffer( render_buffer_t buffer )
{
    if( !buffer->flushed ) 
        pvr2_render_buffer_copy_to_sh4(buffer);
    pvr2_destroy_render_buffer(buffer);
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
        texcache_active_list[i].next = EMPTY_ENTRY;
        if( texcache_active_list[i].buffer != NULL ) {
            texcache_release_render_buffer(texcache_active_list[i].buffer);
            texcache_active_list[i].buffer = NULL;
        }
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

static void texcache_evict( int slot )
{
    /* Remove the selected slot from the lookup table */
    assert( texcache_active_list[slot].texture_addr != -1 );
    uint32_t evict_page = texcache_active_list[slot].texture_addr >> 12;
    texcache_entry_index replace_next = texcache_active_list[slot].next;
    texcache_active_list[slot].texture_addr = -1;
    texcache_active_list[slot].next = EMPTY_ENTRY; /* Just for safety */
    if( texcache_active_list[slot].buffer != NULL ) {
        texcache_release_render_buffer(texcache_active_list[slot].buffer);
        texcache_active_list[slot].buffer = NULL;
    }
    if( texcache_page_lookup[evict_page] == slot ) {
        texcache_page_lookup[evict_page] = replace_next;
    } else {
        texcache_entry_index idx = texcache_page_lookup[evict_page];
        texcache_entry_index next;
        do {
            next = texcache_active_list[idx].next;
            if( next == slot ) {
                assert( idx != replace_next );
                texcache_active_list[idx].next = replace_next;
                break;
            }
            idx = next;
        } while( next != EMPTY_ENTRY );
    }
}

/**
 * Evict a single texture from the cache.
 * @return the slot of the evicted texture.
 */
static texcache_entry_index texcache_evict_lru( void )
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
    texcache_evict(slot);

    return slot;
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
        entry->texture_addr = -1;
        if( entry->buffer != NULL ) {
            texcache_release_render_buffer(entry->buffer);
            entry->buffer = NULL;
        }
        /* release entry */
        texcache_free_ptr--;
        texcache_free_list[texcache_free_ptr] = idx;
        idx = entry->next;
        entry->next = EMPTY_ENTRY;
    } while( idx != EMPTY_ENTRY );
    texcache_page_lookup[texture_page] = EMPTY_ENTRY;
}

/**
 * Mark all textures that use the palette table as needing a re-read (ie 
 * for when the palette is changed. We could track exactly which ones are 
 * affected, but it's not clear that the extra maintanence overhead is 
 * worthwhile.
 */
void texcache_invalidate_palette( )
{
    int i;
    for( i=0; i<MAX_TEXTURES; i++ ) {
        if( texcache_active_list[i].texture_addr != -1 &&
                PVR2_TEX_IS_PALETTE(texcache_active_list[i].mode) ) {
            texcache_evict( i );
            texcache_free_ptr--;
            texcache_free_list[texcache_free_ptr] = i;
        }
    }
}

static void decode_pal8_to_32( uint32_t *out, uint8_t *in, int inbytes, uint32_t *pal )
{
    int i;
    for( i=0; i<inbytes; i++ ) {
        *out++ = pal[*in++];
    }
}

static void decode_pal8_to_16( uint16_t *out, uint8_t *in, int inbytes, uint32_t *pal )
{
    int i;
    for( i=0; i<inbytes; i++ ) {
        *out++ = (uint16_t)pal[*in++];
    }
}

static void decode_pal4_to_32( uint32_t *out, uint8_t *in, int inbytes, uint32_t *pal )
{
    int i;
    for( i=0; i<inbytes; i++ ) {
        *out++ = pal[*in & 0x0F];
        *out++ = pal[(*in >> 4)];
        in++;
    }
}


static void decode_pal4_to_16( uint16_t *out, uint8_t *in, int inbytes, uint32_t *pal )
{
    int i;
    for( i=0; i<inbytes; i++ ) {
        *out++ = (uint16_t)pal[*in & 0x0F];
        *out++ = (uint16_t)pal[(*in >> 4)];
        in++;
    }
}

#define VQ_CODEBOOK_SIZE 2048 /* 256 entries * 4 pixels per quad * 2 byte pixels */

struct vq_codebook {
    uint16_t quad[256][4];
};

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

static void vq_decode( uint16_t *output, unsigned char *input, int width, int height, 
                       struct vq_codebook *codebook ) {
    int i,j;

    uint8_t *c = (uint8_t *)input;
    for( j=0; j<height; j+=2 ) {
        for( i=0; i<width; i+=2 ) {
            uint8_t code = *c++;
            output[i + j*width] = codebook->quad[code][0];
            output[i + 1 + j*width] = codebook->quad[code][1];
            output[i + (j+1)*width] = codebook->quad[code][2];
            output[i + 1 + (j+1)*width] = codebook->quad[code][3];
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
 * Convert raster YUV texture data into RGB32 data - most GL implementations don't
 * directly support this format unfortunately. The input data is formatted as
 * 32 bits = 2 horizontal pixels, UYVY. This is currently done rather inefficiently
 * in floating point.
 */
static void yuv_decode( uint32_t *output, uint32_t *input, int width, int height )
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

static gboolean is_npot_texture( int width )
{
    while( width != 0 ) {
        if( width & 1 ) 
            return width != 1;
        width >>= 1;
    }
    return TRUE;
}

/**
 * Load texture data from the given address and parameters into the currently
 * bound OpenGL texture.
 */
static void texcache_load_texture( uint32_t texture_addr, int width, int height,
                                   int mode ) {
    int bpp_shift = 1; /* bytes per (output) pixel as a power of 2 */
    GLint intFormat = GL_RGBA, format, type;
    int tex_format = mode & PVR2_TEX_FORMAT_MASK;
    struct vq_codebook codebook;
    GLint filter = GL_LINEAR;

    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );

    /* Decode the format parameters */
    switch( tex_format ) {
    case PVR2_TEX_FORMAT_IDX4:
    case PVR2_TEX_FORMAT_IDX8:
        /* For indexed-colour modes, we need to lookup the palette control
         * word to determine the de-indexed texture format.
         */
        switch( MMIO_READ( PVR2, RENDER_PALETTE ) & 0x03 ) {
        case 0: /* ARGB1555 */
            format = GL_BGRA;
            type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
            break;
        case 1:  /* RGB565 */
            intFormat = GL_RGB;
            format = GL_RGB;
            type = GL_UNSIGNED_SHORT_5_6_5;
            break;
        case 2: /* ARGB4444 */
            format = GL_BGRA;
            type = GL_UNSIGNED_SHORT_4_4_4_4_REV;
            break;
        case 3: /* ARGB8888 */
            format = GL_BGRA;
            type = GL_UNSIGNED_BYTE;
            bpp_shift = 2;
            break;
        default:
            return; /* Can't happen, but it makes gcc stop complaining */
        }
        break;

        default:
        case PVR2_TEX_FORMAT_ARGB1555:
            format = GL_BGRA;
            type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
            break;
        case PVR2_TEX_FORMAT_RGB565:
            intFormat = GL_RGB;
            format = GL_RGB;
            type = GL_UNSIGNED_SHORT_5_6_5;
            break;
        case PVR2_TEX_FORMAT_ARGB4444:
            format = GL_BGRA;
            type = GL_UNSIGNED_SHORT_4_4_4_4_REV;
            break;
        case PVR2_TEX_FORMAT_YUV422:
            /* YUV422 isn't directly supported by most implementations, so decode
             * it to a (reasonably) standard ARGB32.
             */
            bpp_shift = 2;
            format = GL_BGRA;
            type = GL_UNSIGNED_BYTE;
            break;
        case PVR2_TEX_FORMAT_BUMPMAP:
            WARN( "Bumpmap not supported" );
            return;
    }

    if( PVR2_TEX_IS_STRIDE(mode) && tex_format != PVR2_TEX_FORMAT_IDX4 &&
            tex_format != PVR2_TEX_FORMAT_IDX8 ) {
        /* Stride textures cannot be mip-mapped, compressed, indexed or twiddled */
        uint32_t stride = (MMIO_READ( PVR2, RENDER_TEXSIZE ) & 0x003F) << 5;
        unsigned char data[(width*height) << bpp_shift];
        if( tex_format == PVR2_TEX_FORMAT_YUV422 ) {
            unsigned char tmp[(width*height)<<1];
            pvr2_vram64_read_stride( tmp, width<<1, texture_addr, stride<<1, height );
            yuv_decode( (uint32_t *)data, (uint32_t *)tmp, width, height );
        } else {
            pvr2_vram64_read_stride( data, width<<bpp_shift, texture_addr, stride<<bpp_shift, height );
        }
        glTexImage2D( GL_TEXTURE_2D, 0, intFormat, width, height, 0, format, type, data );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        return;
    } 

    if( PVR2_TEX_IS_COMPRESSED(mode) ) {
        uint16_t tmp[VQ_CODEBOOK_SIZE];
        pvr2_vram64_read( (unsigned char *)tmp, texture_addr, VQ_CODEBOOK_SIZE );
        texture_addr += VQ_CODEBOOK_SIZE;
        vq_get_codebook( &codebook, tmp );
    }

    int level=0, last_level = 0, mip_width = width, mip_height = height, src_bytes, dest_bytes;
    if( PVR2_TEX_IS_MIPMAPPED(mode) ) {
        uint32_t src_offset = 0;
        filter = GL_LINEAR_MIPMAP_LINEAR;
        mip_height = height = width;
        while( (1<<last_level) < width ) {
            last_level++;
            src_offset += ((width>>last_level)*(width>>last_level));
        }
        if( width != 1 ) {
            src_offset += 3;
        }
        if( PVR2_TEX_IS_COMPRESSED(mode) ) {
            src_offset >>= 2;
        } else if( tex_format == PVR2_TEX_FORMAT_IDX4 ) {
            src_offset >>= 1;
        } else if( tex_format == PVR2_TEX_FORMAT_YUV422 ) {
            src_offset <<= 1;
        } else if( tex_format != PVR2_TEX_FORMAT_IDX8 ) {
            src_offset <<= bpp_shift;
        }
        texture_addr += src_offset;
    }


    dest_bytes = (mip_width * mip_height) << bpp_shift;
    src_bytes = dest_bytes; // Modes will change this (below)

    for( level=0; level<= last_level; level++ ) {
        unsigned char data[dest_bytes];
        /* load data from image, detwiddling/uncompressing as required */
        if( tex_format == PVR2_TEX_FORMAT_IDX8 ) {
            src_bytes = (mip_width * mip_height);
            int bank = (mode >> 25) &0x03;
            uint32_t *palette = ((uint32_t *)mmio_region_PVR2PAL.mem) + (bank<<8);
            unsigned char tmp[src_bytes];
            pvr2_vram64_read_twiddled_8( tmp, texture_addr, mip_width, mip_height );
            if( bpp_shift == 2 ) {
                decode_pal8_to_32( (uint32_t *)data, tmp, src_bytes, palette );
            } else {
                decode_pal8_to_16( (uint16_t *)data, tmp, src_bytes, palette );
            }
        } else if( tex_format == PVR2_TEX_FORMAT_IDX4 ) {
            src_bytes = (mip_width * mip_height) >> 1;
            int bank = (mode >>21 ) & 0x3F;
            uint32_t *palette = ((uint32_t *)mmio_region_PVR2PAL.mem) + (bank<<4);
            unsigned char tmp[src_bytes];
            pvr2_vram64_read_twiddled_4( tmp, texture_addr, mip_width, mip_height );
            if( bpp_shift == 2 ) {
                decode_pal4_to_32( (uint32_t *)data, tmp, src_bytes, palette );
            } else {
                decode_pal4_to_16( (uint16_t *)data, tmp, src_bytes, palette );
            }
        } else if( tex_format == PVR2_TEX_FORMAT_YUV422 ) {
            src_bytes = ((mip_width*mip_height)<<1);
            unsigned char tmp[src_bytes];
            if( PVR2_TEX_IS_TWIDDLED(mode) ) {
                pvr2_vram64_read_twiddled_16( tmp, texture_addr, mip_width, mip_height );
            } else {
                pvr2_vram64_read( tmp, texture_addr, src_bytes );
            }
            yuv_decode( (uint32_t *)data, (uint32_t *)tmp, mip_width, mip_height );
        } else if( PVR2_TEX_IS_COMPRESSED(mode) ) {
            src_bytes = ((mip_width*mip_height) >> 2);
            unsigned char tmp[src_bytes];
            if( PVR2_TEX_IS_TWIDDLED(mode) ) {
                pvr2_vram64_read_twiddled_8( tmp, texture_addr, mip_width>>1, mip_height>>1 );
            } else {
                pvr2_vram64_read( tmp, texture_addr, src_bytes );
            }
            vq_decode( (uint16_t *)data, tmp, mip_width, mip_height, &codebook );
        } else if( PVR2_TEX_IS_TWIDDLED(mode) ) {
            pvr2_vram64_read_twiddled_16( data, texture_addr, mip_width, mip_height );
        } else {
            pvr2_vram64_read( data, texture_addr, src_bytes );
        }

        /* Pass to GL */
        if( level == last_level && level != 0 ) { /* 1x1 stored within a 2x2 */
            glTexImage2D( GL_TEXTURE_2D, level, intFormat, 1, 1, 0, format, type,
                    data + (3 << bpp_shift) );
        } else {
            glTexImage2D( GL_TEXTURE_2D, level, intFormat, mip_width, mip_height, 0, format, type,
                    data );
            if( mip_width > 2 ) {
                mip_width >>= 1;
                mip_height >>= 1;
                dest_bytes >>= 2;
                src_bytes >>= 2;
            }
            texture_addr -= src_bytes;
        }
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static int texcache_find_texture_slot( uint32_t texture_word, int width, int height )
{
    uint32_t texture_addr = (texture_word & 0x000FFFFF)<<3;
    uint32_t texture_page = texture_addr >> 12;
    texcache_entry_index next;
    texcache_entry_index idx = texcache_page_lookup[texture_page];
    while( idx != EMPTY_ENTRY ) {
        texcache_entry_t entry = &texcache_active_list[idx];
        if( entry->texture_addr == texture_addr &&
                entry->mode == texture_word &&
                entry->width == width &&
                entry->height == height ) {
            entry->lru_count = texcache_ref_counter++;
            return idx;
        }
        idx = entry->next;
    }
    return -1;
}

static int texcache_alloc_texture_slot( uint32_t texture_word, int width, int height )
{
    uint32_t texture_addr = (texture_word & 0x000FFFFF)<<3;
    uint32_t texture_page = texture_addr >> 12;
    texcache_entry_index slot = 0;

    if( texcache_free_ptr < MAX_TEXTURES ) {
        slot = texcache_free_list[texcache_free_ptr++];
    } else {
        slot = texcache_evict_lru();
    }

    /* Construct new entry */
    texcache_active_list[slot].texture_addr = texture_addr;
    texcache_active_list[slot].width = width;
    texcache_active_list[slot].height = height;
    texcache_active_list[slot].mode = texture_word;
    texcache_active_list[slot].lru_count = texcache_ref_counter++;

    /* Add entry to the lookup table */
    int next = texcache_page_lookup[texture_page];
    if( next == slot ) {
        int i;
        fprintf( stderr, "Active list: " );
        for( i=0; i<MAX_TEXTURES; i++ ) {
            fprintf( stderr, "%d, ", texcache_active_list[i].next );
        }
        fprintf( stderr, "\n" );
        assert( next != slot );

    }
    assert( next != slot );
    texcache_active_list[slot].next = next;
    texcache_page_lookup[texture_page] = slot;
    return slot;
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
GLuint texcache_get_texture( uint32_t texture_word, int width, int height )
{
    int slot = texcache_find_texture_slot( texture_word, width, height );

    if( slot == -1 ) {
        /* Not found - check the free list */
        slot = texcache_alloc_texture_slot( texture_word, width, height );
        
        /* Construct the GL texture */
        uint32_t texture_addr = (texture_word & 0x000FFFFF)<<3;
        glBindTexture( GL_TEXTURE_2D, texcache_active_list[slot].texture_id );
        texcache_load_texture( texture_addr, width, height, texture_word );
    }

    return texcache_active_list[slot].texture_id;
}

render_buffer_t texcache_get_render_buffer( uint32_t texture_addr, int mode, int width, int height )
{
    INFO( "Rendering to texture!" );
    uint32_t texture_word = ((texture_addr >> 3) & 0x000FFFFF) | PVR2_TEX_UNTWIDDLED;
    switch( mode ) {
    case COLFMT_BGRA1555: texture_word |= PVR2_TEX_FORMAT_ARGB1555; break;
    case COLFMT_RGB565:   texture_word |= PVR2_TEX_FORMAT_RGB565; break;
    case COLFMT_BGRA4444: texture_word |= PVR2_TEX_FORMAT_ARGB4444; break;
    default:
        WARN( "Rendering to non-texture colour format" );
    }
    if( is_npot_texture(width) )
        texture_word |= PVR2_TEX_STRIDE;
    
    
    int slot = texcache_find_texture_slot( texture_word, width, height );
    if( slot == -1 ) {
        slot = texcache_alloc_texture_slot( texture_word, width, height );
    }
    
    texcache_entry_t entry = &texcache_active_list[slot];
    if( entry->width != width || entry->height != height ) {
        glBindTexture(GL_TEXTURE_2D, entry->texture_id );
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        if( entry->buffer != NULL ) {
            texcache_release_render_buffer(entry->buffer);
        }
        entry->buffer = pvr2_create_render_buffer( texture_addr, width, height, entry->texture_id );
    } else {
        if( entry->buffer == NULL )
            entry->buffer = pvr2_create_render_buffer( texture_addr, width, height, entry->texture_id );
    }

    return entry->buffer;
}

/**
 * Check the integrity of the texcache. Verifies that every cache slot
 * appears exactly once on either the free list or one page list. For 
 * active slots, the texture address must also match the page it appears on.
 * 
 */
void texcache_integrity_check()
{
    int i;
    int slot_found[MAX_TEXTURES];

    memset( slot_found, 0, sizeof(slot_found) );

    /* Check entries on the free list */
    for( i= texcache_free_ptr; i< MAX_TEXTURES; i++ ) {
        int slot = texcache_free_list[i];
        assert( slot_found[slot] == 0 );
        assert( texcache_active_list[slot].next == EMPTY_ENTRY );
        slot_found[slot] = 1;
    }

    /* Check entries on the active lists */
    for( i=0; i< PVR2_RAM_PAGES; i++ ) {
        int slot = texcache_page_lookup[i];
        while( slot != EMPTY_ENTRY ) {
            assert( slot_found[slot] == 0 );
            assert( (texcache_active_list[slot].texture_addr >> 12) == i );
            slot_found[slot] = 2;
            slot = texcache_active_list[slot].next;
        }
    }

    /* Make sure we didn't miss any entries */
    for( i=0; i<MAX_TEXTURES; i++ ) {
        assert( slot_found[i] != 0 );
    }
}
