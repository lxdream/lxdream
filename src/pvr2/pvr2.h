/**
 * $Id: pvr2.h,v 1.33 2007-01-29 11:24:44 nkeynes Exp $
 *
 * PVR2 (video chip) functions and macros.
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

#include "dream.h"
#include "mem.h"
#include "display.h"
#include "pvr2/pvr2mmio.h"
#include <GL/gl.h>

typedef unsigned int pvraddr_t;
typedef unsigned int pvr64addr_t;

#define DISPMODE_ENABLE      0x00000001 /* Display enable */
#define DISPMODE_LINEDOUBLE  0x00000002 /* scanline double */
#define DISPMODE_COLFMT      0x0000000C /* Colour mode */
#define DISPMODE_CLOCKDIV    0x08000000 /* Clock divide-by-2 */

#define DISPSIZE_MODULO 0x3FF00000 /* line skip +1 (32-bit words)*/
#define DISPSIZE_LPF    0x000FFC00 /* lines per field */
#define DISPSIZE_PPL    0x000003FF /* pixel words (32 bit) per line */

#define DISPCFG_VP 0x00000001 /* V-sync polarity */
#define DISPCFG_HP 0x00000002 /* H-sync polarity */
#define DISPCFG_I  0x00000010 /* Interlace enable */
#define DISPCFG_BS 0x000000C0 /* Broadcast standard */
#define DISPCFG_VO 0x00000100 /* Video output enable */

#define BS_NTSC 0x00000000
#define BS_PAL  0x00000040
#define BS_PALM 0x00000080 /* ? */
#define BS_PALN 0x000000C0 /* ? */

#define PVR2_RAM_BASE 0x05000000
#define PVR2_RAM_BASE_INT 0x04000000
#define PVR2_RAM_SIZE (8 * 1024 * 1024)
#define PVR2_RAM_PAGES (PVR2_RAM_SIZE>>12)
#define PVR2_RAM_MASK 0x7FFFFF

#define RENDER_ZONLY  0
#define RENDER_NORMAL 1     /* Render non-modified polygons */
#define RENDER_CHEAPMOD 2   /* Render cheap-modified polygons */
#define RENDER_FULLMOD 3    /* Render the fully-modified version of the polygons */

void pvr2_next_frame( void );
void pvr2_set_base_address( uint32_t );
int pvr2_get_frame_count( void );
gboolean pvr2_save_next_scene( const gchar *filename );

#define PVR2_CMD_END_OF_LIST 0x00
#define PVR2_CMD_USER_CLIP   0x20
#define PVR2_CMD_POLY_OPAQUE 0x80
#define PVR2_CMD_MOD_OPAQUE  0x81
#define PVR2_CMD_POLY_TRANS  0x82
#define PVR2_CMD_MOD_TRANS   0x83
#define PVR2_CMD_POLY_PUNCHOUT 0x84
#define PVR2_CMD_VERTEX      0xE0
#define PVR2_CMD_VERTEX_LAST 0xF0

#define PVR2_POLY_TEXTURED 0x00000008
#define PVR2_POLY_SPECULAR 0x00000004
#define PVR2_POLY_SHADED   0x00000002
#define PVR2_POLY_UV_16BIT 0x00000001

#define PVR2_POLY_MODE_CLAMP_RGB 0x00200000
#define PVR2_POLY_MODE_ALPHA    0x00100000
#define PVR2_POLY_MODE_TEXALPHA 0x00080000
#define PVR2_POLY_MODE_FLIP_S   0x00040000
#define PVR2_POLY_MODE_FLIP_T   0x00020000
#define PVR2_POLY_MODE_CLAMP_S  0x00010000
#define PVR2_POLY_MODE_CLAMP_T  0x00008000

#define PVR2_POLY_FOG_LOOKUP    0x00000000
#define PVR2_POLY_FOG_VERTEX    0x00400000
#define PVR2_POLY_FOG_DISABLED  0x00800000
#define PVR2_POLY_FOG_LOOKUP2   0x00C00000


#define PVR2_TEX_FORMAT_ARGB1555 0x00000000
#define PVR2_TEX_FORMAT_RGB565   0x08000000
#define PVR2_TEX_FORMAT_ARGB4444 0x10000000
#define PVR2_TEX_FORMAT_YUV422   0x18000000
#define PVR2_TEX_FORMAT_BUMPMAP  0x20000000
#define PVR2_TEX_FORMAT_IDX4     0x28000000
#define PVR2_TEX_FORMAT_IDX8     0x30000000

#define PVR2_TEX_MIPMAP      0x80000000
#define PVR2_TEX_COMPRESSED  0x40000000
#define PVR2_TEX_FORMAT_MASK 0x38000000
#define PVR2_TEX_UNTWIDDLED  0x04000000
#define PVR2_TEX_STRIDE      0x02000000
#define PVR2_TEX_IS_PALETTE(mode) ( (mode & PVR2_TEX_FORMAT_MASK) == PVR2_TEX_FORMAT_IDX4 || (mode&PVR2_TEX_FORMAT_MASK) == PVR2_TEX_FORMAT_IDX8 )


#define PVR2_TEX_ADDR(x) ( ((x)&0x01FFFFF)<<3 );
#define PVR2_TEX_IS_MIPMAPPED(x) ( (x) & PVR2_TEX_MIPMAP )
#define PVR2_TEX_IS_COMPRESSED(x) ( (x) & PVR2_TEX_COMPRESSED )
#define PVR2_TEX_IS_TWIDDLED(x) (((x) & PVR2_TEX_UNTWIDDLED) == 0)
#define PVR2_TEX_IS_STRIDE(x) (((x) & 0x06000000) == 0x06000000)

/****************************** Frame Buffer *****************************/

/**
 * Write a block of data to an address in the DMA range (0x10000000 - 
 * 0x13FFFFFF), ie TA, YUV, or texture ram.
 */
void pvr2_dma_write( sh4addr_t dest, char *src, uint32_t length );

/**
 * Write to the interleaved memory address space (aka 64-bit address space).
 */
void pvr2_vram64_write( sh4addr_t dest, char *src, uint32_t length );

/**
 * Write to the interleaved memory address space (aka 64-bit address space),
 * using a line length and stride.
 */
void pvr2_vram64_write_stride( sh4addr_t dest, char *src, uint32_t line_bytes,
			       uint32_t line_stride_bytes, uint32_t line_count );

/**
 * Read from the interleaved memory address space (aka 64-bit address space)
 */
void pvr2_vram64_read( char *dest, sh4addr_t src, uint32_t length );

/**
 * Read a twiddled image from interleaved memory address space (aka 64-bit address
 * space), writing the image to the destination buffer in detwiddled format. 
 * Width and height must be powers of 2
 * This version reads 4-bit pixels.
 */
void pvr2_vram64_read_twiddled_4( char *dest, sh4addr_t src, uint32_t width, uint32_t height );


/**
 * Read a twiddled image from interleaved memory address space (aka 64-bit address
 * space), writing the image to the destination buffer in detwiddled format. 
 * Width and height must be powers of 2
 * This version reads 8-bit pixels.
 */
void pvr2_vram64_read_twiddled_8( char *dest, sh4addr_t src, uint32_t width, uint32_t height );

/**
 * Read a twiddled image from interleaved memory address space (aka 64-bit address
 * space), writing the image to the destination buffer in detwiddled format. 
 * Width and height must be powers of 2, and src must be 16-bit aligned.
 * This version reads 16-bit pixels.
 */
void pvr2_vram64_read_twiddled_16( char *dest, sh4addr_t src, uint32_t width, uint32_t height );

/**
 * Read an image from the interleaved memory address space (aka 64-bit address space) 
 * where the source and destination line sizes may differ. Note that both byte
 * counts must be a multiple of 4, and the src address must be 32-bit aligned.
 */
void pvr2_vram64_read_stride( char *dest, uint32_t dest_line_bytes, sh4addr_t srcaddr,
			       uint32_t src_line_bytes, uint32_t line_count );
/**
 * Dump a portion of vram to a stream from the interleaved memory address 
 * space.
 */
void pvr2_vram64_dump( sh4addr_t addr, uint32_t length, FILE *f );


/**
 * Describes a rendering buffer that's actually held in GL, for when we need
 * to fetch the bits back to vram.
 */
typedef struct pvr2_render_buffer {
    sh4addr_t render_addr; /* The actual address rendered to in pvr ram */
    uint32_t size; /* Length of rendering region in bytes */
    int width, height;
    int colour_format;
    int scale;
} *pvr2_render_buffer_t;

/**
 * Flush the indicated render buffer back to PVR. Caller is responsible for
 * tracking whether there is actually anything in the buffer.
 *
 * @param buffer A render buffer indicating the address to store to, and the
 * format the data needs to be in.
 * @param backBuffer TRUE to flush the back buffer, FALSE for 
 * the front buffer.
 */
void pvr2_render_buffer_copy_to_sh4( pvr2_render_buffer_t buffer, 
				     gboolean backBuffer );

/**
 * Copy data from PVR ram into the GL render buffer. 
 *
 * @param buffer A render buffer indicating the address to read from, and the
 * format the data is in.
 * @param backBuffer TRUE to write the back buffer, FALSE for 
 * the front buffer.
 */
void pvr2_render_buffer_copy_from_sh4( pvr2_render_buffer_t buffer, 
				       gboolean backBuffer );


/**
 * Invalidate any caching on the supplied SH4 address
 */
gboolean pvr2_render_buffer_invalidate( sh4addr_t addr );


/**************************** Tile Accelerator ***************************/
/**
 * Process the data in the supplied buffer as an array of TA command lists.
 * Any excess bytes are held pending until a complete list is sent
 */
void pvr2_ta_write( char *buf, uint32_t length );


/**
 * (Re)initialize the tile accelerator in preparation for the next scene.
 * Normally called immediately before commencing polygon transmission.
 */
void pvr2_ta_init( void );


/****************************** YUV Converter ****************************/

/**
 * Process a block of YUV data.
 */
void pvr2_yuv_write( char *buf, uint32_t length );

/**
 * Initialize the YUV converter.
 */
void pvr2_yuv_init( uint32_t target_addr );

void pvr2_yuv_set_config( uint32_t config );

/********************************* Renderer ******************************/

/**
 * Initialize the rendering pipeline.
 * @return TRUE on success, FALSE on failure.
 */
gboolean pvr2_render_init( void );

/**
 * Render the current scene stored in PVR ram to the GL back buffer.
 */
void pvr2_render_scene( void );

/**
 * Display the scene rendered to the supplied address.
 * @return TRUE if there was an available render that was displayed,
 * otherwise FALSE (and no action was taken)
 */
gboolean pvr2_render_display_frame( uint32_t address );


void render_backplane( uint32_t *polygon, uint32_t width, uint32_t height, uint32_t mode );

void render_set_context( uint32_t *context, int render_mode );

void pvr2_render_tilebuffer( int width, int height, int clipx1, int clipy1, 
			     int clipx2, int clipy2 );

float pvr2_render_find_maximum_z();
/**
 * Structure to hold a complete unpacked vertex (excluding modifier
 * volume parameters - generate separate vertexes in that case).
 */
struct vertex_unpacked {
    float x,y,z;
    float u,v;            /* Texture coordinates */
    float rgba[4];        /* Fragment colour (RGBA order) */
    float offset_rgba[4]; /* Offset color (RGBA order) */
};

void render_unpacked_vertex_array( uint32_t poly1, struct vertex_unpacked *vertexes[], 
				   int num_vertexes );

void render_vertex_array( uint32_t poly1, uint32_t *vertexes[], int num_vertexes, 
			  int vertex_size, int render_mode );

/****************************** Texture Cache ****************************/

/**
 * Initialize the texture cache.
 */
void texcache_init( void );

/**
 * Initialize the GL side of the texture cache (texture ids and such).
 */
void texcache_gl_init( void );

/**
 * Flush all textures and delete. The cache will be non-functional until
 * the next call to texcache_init(). This would typically be done if
 * switching GL targets.
 */    
void texcache_shutdown( void );

/**
 * Evict all textures contained in the page identified by a texture address.
 */
void texcache_invalidate_page( uint32_t texture_addr );

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
			     int mode );

/************************* Rendering support macros **************************/
#define POLY1_DEPTH_MODE(poly1) ( pvr2_poly_depthmode[(poly1)>>29] )
#define POLY1_DEPTH_ENABLE(poly1) (((poly1)&0x04000000) == 0 )
#define POLY1_CULL_MODE(poly1) (((poly1)>>27)&0x03)
#define POLY1_TEXTURED(poly1) (((poly1)&0x02000000))
#define POLY1_SPECULAR(poly1) (((poly1)&0x01000000))
#define POLY1_GOURAUD_SHADED(poly1) ((poly1)&0x00800000)
#define POLY1_SHADE_MODEL(poly1) (((poly1)&0x00800000) ? GL_SMOOTH : GL_FLAT)
#define POLY1_UV16(poly1)   (((poly1)&0x00400000))
#define POLY1_SINGLE_TILE(poly1) (((poly1)&0x00200000))

#define POLY2_SRC_BLEND(poly2) ( pvr2_poly_srcblend[(poly2) >> 29] )
#define POLY2_DEST_BLEND(poly2) ( pvr2_poly_dstblend[((poly2)>>26)&0x07] )
#define POLY2_SRC_BLEND_TARGET(poly2)    ((poly2)&0x02000000)
#define POLY2_DEST_BLEND_TARGET(poly2)   ((poly2)&0x01000000)
#define POLY2_FOG_MODE(poly2)            ((poly2)&0x00C00000)
#define POLY2_COLOUR_CLAMP_ENABLE(poly2) ((poly2)&0x00200000)
#define POLY2_ALPHA_ENABLE(poly2)        ((poly2)&0x00100000)
#define POLY2_TEX_ALPHA_ENABLE(poly2)   (((poly2)&0x00080000) == 0 )
#define POLY2_TEX_CLAMP_U(poly2)         ((poly2)&0x00010000)
#define POLY2_TEX_CLAMP_V(poly2)         ((poly2)&0x00008000)
#define POLY2_TEX_WIDTH(poly2) ( 1<< ((((poly2) >> 3) & 0x07 ) + 3) )
#define POLY2_TEX_HEIGHT(poly2) ( 1<< (((poly2) & 0x07 ) + 3) )
#define POLY2_TEX_BLEND(poly2) (((poly2) >> 6)&0x03)
extern int pvr2_poly_depthmode[8];
extern int pvr2_poly_srcblend[8];
extern int pvr2_poly_dstblend[8];
extern int pvr2_poly_texblend[4];
extern int pvr2_render_colour_format[8];

float halftofloat(uint16_t half);
