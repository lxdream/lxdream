/**
 * $Id: yuv.c,v 1.1 2007-01-14 02:55:25 nkeynes Exp $
 *
 * YUV420 and YUV422 decoding
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

#define YUV420_BLOCK_SIZE 384

static inline uint16_t decode_yuv420_to_rgb565_pixel( uint8_t y, uint8_t u, uint8_t v )
{
    

}

/**
 * Convert a single 16x16 yuv420 block to rgb565.
 * @param dest output memory location for this block
 * @param src start of source block
 * @param stride length of overall line in pixels (ie 16-bit words)
 */
static void decode_yuv420_to_rgb565_block( uint16_t *dest, uint8_t *src, uint32_t stride ) 
{
    uint8_t *up = *src;
    uint8_t *vp = u + 64;
    uint8_t *yp = v + 64;
    
    for( int yb=0; yb<16; yb++ ) {
	for( int xb=0; xb<16; xb++ ) {
	    uint8_t y = *yp++;
	    uint8_t u = up[xb>>1 + (yb>>1)<<3];
	    uint8_t v = vp[xb>>1 + (yb>>1)<<3];
	    *dest++ = decode_yuv420_to_rgb565_block(y,u,v);
	}
	dest = dest + stride - 16;
    }
}

void decode_yuv420_to_rgb565( uint16_t *dest, uint8_t *src, int width, int height ) 
{
    uint16_t *p;
    for( int j=0; j<height; j++ ) {
	for( int i=0; i<width; i++ ) {
	    p = dest + (j<<5)*width + i<<5;
	    decode_yuv420_to_rgb565_block( p, src, width );
	    src += YUV420_BLOCK_SIZE;
	}
    }
}
