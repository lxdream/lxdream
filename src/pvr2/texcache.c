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
#include "pvr2/glutil.h"

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
    uint32_t poly2_mode, tex_mode;
    GLuint texture_id;
    render_buffer_t buffer;
    texcache_entry_index next;
    uint32_t lru_count;
} *texcache_entry_t;

static texcache_entry_index texcache_page_lookup[PVR2_RAM_PAGES];
static uint32_t texcache_ref_counter;
static struct texcache_entry texcache_active_list[MAX_TEXTURES];
static uint32_t texcache_palette_mode;
static uint32_t texcache_stride_width;
static gboolean texcache_have_palette_shader;
static gboolean texcache_palette_valid;
static GLuint texcache_palette_texid;

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
    texcache_palette_mode = -1;
    texcache_stride_width = 0;
}

/**
 * Setup the initial texture ids (must be called after the GL context is
 * prepared)
 */
void texcache_gl_init( )
{
    int i;
    GLuint texids[MAX_TEXTURES];

    if( glsl_is_supported() && isGLMultitextureSupported ) {
        texcache_have_palette_shader = TRUE;
        texcache_palette_valid = FALSE;
        glGenTextures(1, &texcache_palette_texid );

        /* Bind the texture and set the params */
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_1D, texcache_palette_texid);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP );
        glActiveTexture(GL_TEXTURE0);

    } else {
        texcache_have_palette_shader = FALSE;
    }

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
        texcache_active_list[i].texture_addr = -1;
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

    if( texcache_have_palette_shader )
        glDeleteTextures( 1, &texcache_palette_texid );

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
 * Load the palette into 4 textures of 256 entries each. This mirrors the
 * banking done by the PVR2 for 8-bit textures, and also ensures that we
 * can use 8-bit paletted textures ourselves.
 */
static void texcache_load_palette_texture( gboolean format_changed )
{
    GLint format, type, intFormat = GL_RGBA;
    unsigned i;
    int bpp = 2;
    uint32_t *palette = (uint32_t *)mmio_region_PVR2PAL.mem;
    uint16_t packed_palette[1024];
    char *data = (char *)palette;

    switch( texcache_palette_mode ) {
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
        bpp = 4;
        break;
    default:
        break; /* Can't happen */
    }


    if( bpp == 2 ) {
        for( i=0; i<1024; i++ ) {
            packed_palette[i] = (uint16_t)palette[i];
        }
        data = (char *)packed_palette;

    }

    glActiveTexture(GL_TEXTURE1);
//    glBindTexture(GL_TEXTURE_1D, texcache_palette_texid);
    if( format_changed )
        glTexImage1D(GL_TEXTURE_1D, 0, intFormat, 1024, 0, format, type, data );
    else
        glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 1024, format, type, data);
//    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//    glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glActiveTexture(GL_TEXTURE0);
    texcache_palette_valid = TRUE;
}


/**
 * Mark the palette as having changed. If we have palette support (via shaders)
 * we just flag the palette, otherwise we have to invalidate all palette
 * textures.
 */
void texcache_invalidate_palette( )
{
    if( texcache_have_palette_shader ) {
        texcache_palette_valid = FALSE;
    } else {
        int i;
        for( i=0; i<MAX_TEXTURES; i++ ) {
            if( texcache_active_list[i].texture_addr != -1 &&
                    PVR2_TEX_IS_PALETTE(texcache_active_list[i].tex_mode) ) {
                texcache_evict( i );
                texcache_free_ptr--;
                texcache_free_list[texcache_free_ptr] = i;
            }
        }
    }
}
/**
 * Mark all stride textures as needing a re-read (ie when the stride width
 * is changed).
 */
void texcache_invalidate_stride( )
{
    int i;
    for( i=0; i<MAX_TEXTURES; i++ ) {
        if( texcache_active_list[i].texture_addr != -1 &&
                PVR2_TEX_IS_STRIDE(texcache_active_list[i].tex_mode) ) {
            texcache_evict( i );
            texcache_free_ptr--;
            texcache_free_list[texcache_free_ptr] = i;
        }
    }
}

void texcache_begin_scene( uint32_t palette_mode, uint32_t stride )
{
    gboolean format_changed = FALSE;
    if( palette_mode != texcache_palette_mode ) {
        texcache_invalidate_palette();
        format_changed = TRUE;
    }
    if( stride != texcache_stride_width )
        texcache_invalidate_stride();
    
    texcache_palette_mode = palette_mode;
    texcache_stride_width = stride;

    if( !texcache_palette_valid && texcache_have_palette_shader )
        texcache_load_palette_texture(format_changed);
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

static void decode_pal4_to_pal8( uint8_t *out, uint8_t *in, int inbytes )
{
    int i;
    for( i=0; i<inbytes; i++ ) {
        *out++ = (uint8_t)(*in & 0x0F);
        *out++ = (uint8_t)(*in >> 4);
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
    GLint min_filter = GL_LINEAR;
    GLint max_filter = GL_LINEAR;
    GLint mipmapfilter = GL_LINEAR_MIPMAP_LINEAR;

    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );

    /* Decode the format parameters */
    switch( tex_format ) {
    case PVR2_TEX_FORMAT_IDX4:
    case PVR2_TEX_FORMAT_IDX8:
        if( texcache_have_palette_shader ) {
            intFormat = GL_ALPHA8;
            format = GL_ALPHA;
            type = GL_UNSIGNED_BYTE;
            bpp_shift = 0;
            min_filter = max_filter = GL_NEAREST;
            mipmapfilter = GL_NEAREST_MIPMAP_NEAREST;
        } else {
            /* For indexed-colour modes, we need to lookup the palette control
             * word to determine the de-indexed texture format.
             */
            switch( texcache_palette_mode ) {
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
        unsigned char data[(width*height) << bpp_shift];
        if( tex_format == PVR2_TEX_FORMAT_YUV422 ) {
            unsigned char tmp[(width*height)<<1];
            pvr2_vram64_read_stride( tmp, width<<1, texture_addr, texcache_stride_width<<1, height );
            yuv_decode( (uint32_t *)data, (uint32_t *)tmp, width, height );
        } else {
            pvr2_vram64_read_stride( data, width<<bpp_shift, texture_addr, texcache_stride_width<<bpp_shift, height );
        }
        glTexImage2D( GL_TEXTURE_2D, 0, intFormat, width, height, 0, format, type, data );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, max_filter);
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
        min_filter = mipmapfilter;
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
            if( texcache_have_palette_shader ) {
                pvr2_vram64_read_twiddled_8( data, texture_addr, mip_width, mip_height );
            } else {
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
            }
        } else if( tex_format == PVR2_TEX_FORMAT_IDX4 ) {
            src_bytes = (mip_width * mip_height) >> 1;
            unsigned char tmp[src_bytes];
            if( texcache_have_palette_shader ) {
                pvr2_vram64_read_twiddled_4( tmp, texture_addr, mip_width, mip_height );
                decode_pal4_to_pal8( data, tmp, src_bytes );
            } else {
                int bank = (mode >>21 ) & 0x3F;
                uint32_t *palette = ((uint32_t *)mmio_region_PVR2PAL.mem) + (bank<<4);
                pvr2_vram64_read_twiddled_4( tmp, texture_addr, mip_width, mip_height );
                if( bpp_shift == 2 ) {
                    decode_pal4_to_32( (uint32_t *)data, tmp, src_bytes, palette );
                } else {
                    decode_pal4_to_16( (uint16_t *)data, tmp, src_bytes, palette );
                }
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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, max_filter);
}

static int texcache_find_texture_slot( uint32_t poly2_masked_word, uint32_t texture_word )
{
    uint32_t texture_addr = (texture_word & 0x000FFFFF)<<3;
    uint32_t texture_page = texture_addr >> 12;
    texcache_entry_index next;
    texcache_entry_index idx = texcache_page_lookup[texture_page];
    while( idx != EMPTY_ENTRY ) {
        texcache_entry_t entry = &texcache_active_list[idx];
        if( entry->tex_mode == texture_word &&
                entry->poly2_mode == poly2_masked_word ) {
            entry->lru_count = texcache_ref_counter++;
            return idx;
        }
        idx = entry->next;
    }
    return -1;
}

static int texcache_alloc_texture_slot( uint32_t poly2_word, uint32_t texture_word )
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
    assert( texcache_active_list[slot].texture_addr == -1 );
    texcache_active_list[slot].texture_addr = texture_addr;
    texcache_active_list[slot].tex_mode = texture_word;
    texcache_active_list[slot].poly2_mode = poly2_word;
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
    texcache_active_list[slot].next = next;
    texcache_page_lookup[texture_page] = slot;
    return slot;
}

/**
 * Return a texture ID for the texture specified at the supplied address
 * and given parameters (the same sequence of bytes could in theory have
 * multiple interpretations). We use the texture address as the primary
 * index, but allow for multiple instances at each address.
 * 
 * If the texture has already been bound, return the ID to which it was
 * bound. Otherwise obtain an unused texture ID and set it up appropriately.
 * The current GL_TEXTURE_2D binding will be changed in this case.
 */
GLuint texcache_get_texture( uint32_t poly2_word, uint32_t texture_word )
{
    poly2_word &= 0x000F803F; /* Get just the texture-relevant bits */
    uint32_t texture_lookup = texture_word;
    if( PVR2_TEX_IS_PALETTE(texture_lookup) ) {
        texture_lookup &= 0xF81FFFFF; /* Mask out the bank bits */
    }
    int slot = texcache_find_texture_slot( poly2_word, texture_lookup );

    if( slot == -1 ) {
        /* Not found - check the free list */
        slot = texcache_alloc_texture_slot( poly2_word, texture_lookup );
        
        /* Construct the GL texture */
        uint32_t texture_addr = (texture_word & 0x000FFFFF)<<3;
        unsigned width = POLY2_TEX_WIDTH(poly2_word);
        unsigned height = POLY2_TEX_HEIGHT(poly2_word);

        glBindTexture( GL_TEXTURE_2D, texcache_active_list[slot].texture_id );
        texcache_load_texture( texture_addr, width, height, texture_word );

        /* Set texture parameters from the poly2 word */
        if( POLY2_TEX_CLAMP_U(poly2_word) ) {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
        } else if( POLY2_TEX_MIRROR_U(poly2_word) ) {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT_ARB );
        } else {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        }
        if( POLY2_TEX_CLAMP_V(poly2_word) ) {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
        } else if( POLY2_TEX_MIRROR_V(poly2_word) ) {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT_ARB );
        } else {
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
        }
    }

    return texcache_active_list[slot].texture_id;
}

#if 0
render_buffer_t texcache_get_render_buffer( uint32_t texture_addr, int mode, int width, int height )
{
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

    if( entry->buffer == NULL ) {
        entry->buffer = pvr2_create_render_buffer( texture_addr, width, height, entry->texture_id );
    } else if( entry->buffer->width != width || entry->buffer->height != height ) {        
        texcache_release_render_buffer(entry->buffer);
        entry->buffer = pvr2_create_render_buffer( texture_addr, width, height, entry->texture_id );
    }

    return entry->buffer;
}
#endif

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

/**
 * Dump the contents of the texture cache
 */
void texcache_dump()
{
    unsigned i;
    for( i=0; i< PVR2_RAM_PAGES; i++ ) {
        int slot = texcache_page_lookup[i];
        while( slot != EMPTY_ENTRY ) {
            fprintf( stderr, "%-3d: %08X %dx%d (%08X %08X)\n", slot,
                    texcache_active_list[slot].texture_addr,
                    POLY2_TEX_WIDTH(texcache_active_list[slot].poly2_mode),
                    POLY2_TEX_HEIGHT(texcache_active_list[slot].poly2_mode),
                    texcache_active_list[slot].poly2_mode,
                    texcache_active_list[slot].tex_mode );
            slot = texcache_active_list[slot].next;
        }
    }
}
