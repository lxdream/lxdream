/**
 * $Id$
 *
 * PVR2 (Video) VRAM handling routines (mainly for the 64-bit region)
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
#include "pvr2.h"
#include "asic.h"
#include <stdio.h>
#include <errno.h>

extern unsigned char *video_base;

void pvr2_dma_write( sh4addr_t destaddr, unsigned char *src, uint32_t count )
{
    int region;

    switch( destaddr & 0x13800000 ) {
    case 0x10000000:
    case 0x12000000:
	pvr2_ta_write( src, count );
	break;
    case 0x11000000:
    case 0x11800000:
	region = MMIO_READ( ASIC, PVRDMARGN1 );
	if( region == 0 ) {
	    pvr2_vram64_write( destaddr, src, count );
	} else {
	    unsigned char *dest = mem_get_region(destaddr);
	    memcpy( dest, src, count );
	}
	break;
    case 0x10800000:
    case 0x12800000:
	pvr2_yuv_write( src, count );
	break;
    case 0x13000000:
    case 0x13800000:
	region = MMIO_READ( ASIC, PVRDMARGN2 );
	if( region == 0 ) {
	    pvr2_vram64_write( destaddr, src, count );
	} else {
	    unsigned char *dest = mem_get_region(destaddr);
	    memcpy( dest, src, count );
	}
    }	    
}

void pvr2_vram64_write( sh4addr_t destaddr, unsigned char *src, uint32_t length )
{
    int bank_flag = (destaddr & 0x04) >> 2;
    uint32_t *banks[2];
    uint32_t *dwsrc;
    int i;

    destaddr = destaddr & 0x7FFFFF;
    if( destaddr + length > 0x800000 ) {
	length = 0x800000 - destaddr;
    }

    for( i=destaddr & 0xFFFFF000; i < destaddr + length; i+= PAGE_SIZE ) {
	texcache_invalidate_page( i );
    }

    banks[0] = ((uint32_t *)(video_base + ((destaddr & 0x007FFFF8) >>1)));
    banks[1] = banks[0] + 0x100000;
    if( bank_flag ) 
	banks[0]++;
    
    /* Handle non-aligned start of source */
    if( destaddr & 0x03 ) {
	unsigned char *dest = ((unsigned char *)banks[bank_flag]) + (destaddr & 0x03);
	for( i= destaddr & 0x03; i < 4 && length > 0; i++, length-- ) {
	    *dest++ = *src++;
	}
	bank_flag = !bank_flag;
    }

    dwsrc = (uint32_t *)src;
    while( length >= 4 ) {
	*banks[bank_flag]++ = *dwsrc++;
	bank_flag = !bank_flag;
	length -= 4;
    }
    
    /* Handle non-aligned end of source */
    if( length ) {
	src = (unsigned char *)dwsrc;
	unsigned char *dest = (unsigned char *)banks[bank_flag];
	while( length-- > 0 ) {
	    *dest++ = *src++;
	}
    }  
}

/**
 * Write an image to 64-bit vram, with a line-stride different from the line-size.
 * The destaddr must be 32-bit aligned, and both line_bytes and line_stride_bytes
 * must be multiples of 4.
 */
void pvr2_vram64_write_stride( sh4addr_t destaddr, unsigned char *src, uint32_t line_bytes, 
			       uint32_t line_stride_bytes, uint32_t line_count )
{
    int bank_flag = (destaddr & 0x04) >> 2;
    uint32_t *banks[2];
    uint32_t *dwsrc;
    uint32_t line_gap;
    int line_gap_flag;
    int i,j;

    destaddr = destaddr & 0x7FFFF8;
    i = line_stride_bytes - line_bytes;
    line_gap_flag = i & 0x04;
    line_gap = i >> 3;
    line_bytes >>= 2;

    for( i=destaddr & 0xFFFFF000; i < destaddr + line_stride_bytes*line_count; i+= PAGE_SIZE ) {
	texcache_invalidate_page( i );
    }

    banks[0] = (uint32_t *)(video_base + (destaddr >>1));
    banks[1] = banks[0] + 0x100000;
    if( bank_flag ) 
	banks[0]++;
    
    dwsrc = (uint32_t *)src;
    for( i=0; i<line_count; i++ ) {
	for( j=0; j<line_bytes; j++ ) {
	    *banks[bank_flag]++ = *dwsrc++;
	    bank_flag = !bank_flag;
	}
	banks[0] += line_gap;
	banks[1] += line_gap;
	if( line_gap_flag ) {
	    banks[bank_flag]++;
	    bank_flag = !bank_flag;
	}
    }    
}

/**
 * Read an image from 64-bit vram, with a destination line-stride different from the line-size.
 * The srcaddr must be 32-bit aligned, and both line_bytes and line_stride_bytes
 * must be multiples of 4. line_stride_bytes must be >= line_bytes.
 * This method is used to extract a "stride" texture from vram.
 */
void pvr2_vram64_read_stride( unsigned char *dest, uint32_t dest_line_bytes, sh4addr_t srcaddr,
				   uint32_t src_line_bytes, uint32_t line_count )
{
    int bank_flag = (srcaddr & 0x04) >> 2;
    uint32_t *banks[2];
    uint32_t *dwdest;
    uint32_t dest_line_gap = 0;
    uint32_t src_line_gap = 0;
    uint32_t line_bytes;
    int src_line_gap_flag;
    int i,j;

    srcaddr = srcaddr & 0x7FFFF8;
    if( src_line_bytes <= dest_line_bytes ) {
	dest_line_gap = (dest_line_bytes - src_line_bytes) >> 2;
	src_line_gap = 0;
	src_line_gap_flag = 0;
	line_bytes = src_line_bytes >> 2;
    } else {
	i = (src_line_bytes - dest_line_bytes);
	src_line_gap_flag = i & 0x04;
	src_line_gap = i >> 3;
	line_bytes = dest_line_bytes >> 2;
    }
	
    banks[0] = (uint32_t *)(video_base + (srcaddr>>1));
    banks[1] = banks[0] + 0x100000;
    if( bank_flag )
	banks[0]++;
    
    dwdest = (uint32_t *)dest;
    for( i=0; i<line_count; i++ ) {
	for( j=0; j<line_bytes; j++ ) {
	    *dwdest++ = *banks[bank_flag]++;
	    bank_flag = !bank_flag;
	}
	dwdest += dest_line_gap;
	banks[0] += src_line_gap;
	banks[1] += src_line_gap;
	if( src_line_gap_flag ) {
	    banks[bank_flag]++;
	    bank_flag = !bank_flag;
	}
    }    
}


/**
 * @param dest Destination image buffer
 * @param banks Source data expressed as two bank pointers
 * @param offset Offset into banks[0] specifying where the next byte
 *  to read is (0..3)
 * @param x1,y1 Destination coordinates
 * @param width Width of current destination block
 * @param stride Total width of image (ie stride) in bytes
 */

static void pvr2_vram64_detwiddle_4( uint8_t *dest, uint8_t *banks[2], int offset,
				     int x1, int y1, int width, int stride )
{
    if( width == 2 ) {
	x1 = x1 >> 1;
	uint8_t t1 = *banks[offset<4?0:1]++;
	uint8_t t2 = *banks[offset<3?0:1]++;
	dest[y1*stride + x1] = (t1 & 0x0F) | (t2<<4);
	dest[(y1+1)*stride + x1] = (t1>>4) | (t2&0xF0);
    } else if( width == 4 ) {
	pvr2_vram64_detwiddle_4( dest, banks, offset, x1, y1, 2, stride );
	pvr2_vram64_detwiddle_4( dest, banks, offset+2, x1, y1+2, 2, stride );
	pvr2_vram64_detwiddle_4( dest, banks, offset+4, x1+2, y1, 2, stride );
	pvr2_vram64_detwiddle_4( dest, banks, offset+6, x1+2, y1+2, 2, stride );
	
    } else {
	int subdivide = width >> 1;
	pvr2_vram64_detwiddle_4( dest, banks, offset, x1, y1, subdivide, stride );
	pvr2_vram64_detwiddle_4( dest, banks, offset, x1, y1+subdivide, subdivide, stride );
	pvr2_vram64_detwiddle_4( dest, banks, offset, x1+subdivide, y1, subdivide, stride );
	pvr2_vram64_detwiddle_4( dest, banks, offset, x1+subdivide, y1+subdivide, subdivide, stride );
    }
}

/**
 * @param dest Destination image buffer
 * @param banks Source data expressed as two bank pointers
 * @param offset Offset into banks[0] specifying where the next byte
 *  to read is (0..3)
 * @param x1,y1 Destination coordinates
 * @param width Width of current destination block
 * @param stride Total width of image (ie stride)
 */

static void pvr2_vram64_detwiddle_8( uint8_t *dest, uint8_t *banks[2], int offset,
				     int x1, int y1, int width, int stride )
{
    if( width == 2 ) {
	dest[y1*stride + x1] = *banks[0]++;
	dest[(y1+1)*stride + x1] = *banks[offset<3?0:1]++;
	dest[y1*stride + x1 + 1] = *banks[offset<2?0:1]++;
	dest[(y1+1)*stride + x1 + 1] = *banks[offset==0?0:1]++;
	uint8_t *tmp = banks[0]; /* swap banks */
	banks[0] = banks[1];
	banks[1] = tmp;
    } else {
	int subdivide = width >> 1;
	pvr2_vram64_detwiddle_8( dest, banks, offset, x1, y1, subdivide, stride );
	pvr2_vram64_detwiddle_8( dest, banks, offset, x1, y1+subdivide, subdivide, stride );
	pvr2_vram64_detwiddle_8( dest, banks, offset, x1+subdivide, y1, subdivide, stride );
	pvr2_vram64_detwiddle_8( dest, banks, offset, x1+subdivide, y1+subdivide, subdivide, stride );
    }
}

/**
 * @param dest Destination image buffer
 * @param banks Source data expressed as two bank pointers
 * @param offset Offset into banks[0] specifying where the next word
 *  to read is (0 or 1)
 * @param x1,y1 Destination coordinates
 * @param width Width of current destination block
 * @param stride Total width of image (ie stride)
 */

static void pvr2_vram64_detwiddle_16( uint16_t *dest, uint16_t *banks[2], int offset,
				      int x1, int y1, int width, int stride )
{
    if( width == 2 ) {
	dest[y1*stride + x1] = *banks[0]++;
	dest[(y1+1)*stride + x1] = *banks[offset]++;
	dest[y1*stride + x1 + 1] = *banks[1]++;
	dest[(y1+1)*stride + x1 + 1] = *banks[offset^1]++;
    } else {
	int subdivide = width >> 1;
	pvr2_vram64_detwiddle_16( dest, banks, offset, x1, y1, subdivide, stride );
	pvr2_vram64_detwiddle_16( dest, banks, offset, x1, y1+subdivide, subdivide, stride );
	pvr2_vram64_detwiddle_16( dest, banks, offset, x1+subdivide, y1, subdivide, stride );
	pvr2_vram64_detwiddle_16( dest, banks, offset, x1+subdivide, y1+subdivide, subdivide, stride );
    }
}

/**
 * Read an image from 64-bit vram stored as twiddled 4-bit pixels. The 
 * image is written out to the destination in detwiddled form.
 * @param dest destination buffer, which must be at least width*height/2 in length
 * @param srcaddr source address in vram
 * @param width image width (must be a power of 2)
 * @param height image height (must be a power of 2)
 */
void pvr2_vram64_read_twiddled_4( unsigned char *dest, sh4addr_t srcaddr, uint32_t width, uint32_t height )
{
    int offset_flag = (srcaddr & 0x07);
    uint8_t *banks[2];
    uint8_t *wdest = (uint8_t*)dest;
    uint32_t stride = width >> 1;
    int i;

    srcaddr = srcaddr & 0x7FFFF8;

    banks[0] = (uint8_t *)(video_base + (srcaddr>>1));
    banks[1] = banks[0] + 0x400000;
    if( offset_flag & 0x04 ) { // If source is not 64-bit aligned, swap the banks
	uint8_t *tmp = banks[0];
	banks[0] = banks[1];
	banks[1] = tmp + 4;
	offset_flag &= 0x03;
    }
    banks[0] += offset_flag;

    if( width > height ) {
	for( i=0; i<width; i+=height ) {
	    pvr2_vram64_detwiddle_4( wdest, banks, offset_flag, i, 0, height, stride );
	}
    } else if( height > width ) {
	for( i=0; i<height; i+=width ) {
	    pvr2_vram64_detwiddle_4( wdest, banks, offset_flag, 0, i, width, stride );
	}
    } else if( width == 1 ) {
	*wdest = *banks[0];
    } else {
	pvr2_vram64_detwiddle_4( wdest, banks, offset_flag, 0, 0, width, stride );
    }   
}

/**
 * Read an image from 64-bit vram stored as twiddled 8-bit pixels. The 
 * image is written out to the destination in detwiddled form.
 * @param dest destination buffer, which must be at least width*height in length
 * @param srcaddr source address in vram
 * @param width image width (must be a power of 2)
 * @param height image height (must be a power of 2)
 */
void pvr2_vram64_read_twiddled_8( unsigned char *dest, sh4addr_t srcaddr, uint32_t width, uint32_t height )
{
    int offset_flag = (srcaddr & 0x07);
    uint8_t *banks[2];
    uint8_t *wdest = (uint8_t*)dest;
    int i;

    srcaddr = srcaddr & 0x7FFFF8;

    banks[0] = (uint8_t *)(video_base + (srcaddr>>1));
    banks[1] = banks[0] + 0x400000;
    if( offset_flag & 0x04 ) { // If source is not 64-bit aligned, swap the banks
	uint8_t *tmp = banks[0];
	banks[0] = banks[1];
	banks[1] = tmp + 4;
	offset_flag &= 0x03;
    }
    banks[0] += offset_flag;

    if( width > height ) {
	for( i=0; i<width; i+=height ) {
	    pvr2_vram64_detwiddle_8( wdest, banks, offset_flag, i, 0, height, width );
	}
    } else if( height > width ) {
	for( i=0; i<height; i+=width ) {
	    pvr2_vram64_detwiddle_8( wdest, banks, offset_flag, 0, i, width, width );
	}
    } else if( width == 1 ) {
	*wdest = *banks[0];
    } else {
	pvr2_vram64_detwiddle_8( wdest, banks, offset_flag, 0, 0, width, width );
    }   
}

/**
 * Read an image from 64-bit vram stored as twiddled 16-bit pixels. The 
 * image is written out to the destination in detwiddled form.
 * @param dest destination buffer, which must be at least width*height*2 in length
 * @param srcaddr source address in vram (must be 16-bit aligned)
 * @param width image width (must be a power of 2)
 * @param height image height (must be a power of 2)
 */
void pvr2_vram64_read_twiddled_16( unsigned char *dest, sh4addr_t srcaddr, uint32_t width, uint32_t height ) {
    int offset_flag = (srcaddr & 0x06) >> 1;
    uint16_t *banks[2];
    uint16_t *wdest = (uint16_t*)dest;
    int i;

    srcaddr = srcaddr & 0x7FFFF8;

    banks[0] = (uint16_t *)(video_base + (srcaddr>>1));
    banks[1] = banks[0] + 0x200000;
    if( offset_flag & 0x02 ) { // If source is not 64-bit aligned, swap the banks
	uint16_t *tmp = banks[0];
	banks[0] = banks[1];
	banks[1] = tmp + 2;
	offset_flag &= 0x01;
    }
    banks[0] += offset_flag;
	

    if( width > height ) {
	for( i=0; i<width; i+=height ) {
	    pvr2_vram64_detwiddle_16( wdest, banks, offset_flag, i, 0, height, width );
	}
    } else if( height > width ) {
	for( i=0; i<height; i+=width ) {
	    pvr2_vram64_detwiddle_16( wdest, banks, offset_flag, 0, i, width, width );
	}
    } else if( width == 1 ) {
	*wdest = *banks[0];
    } else {
	pvr2_vram64_detwiddle_16( wdest, banks, offset_flag, 0, 0, width, width );
    }    
}

void pvr2_vram_write_invert( sh4addr_t destaddr, unsigned char *src, uint32_t length, uint32_t line_length,
			     uint32_t src_stride )
{
    unsigned char *dest = video_base + (destaddr & 0x007FFFFF);
    unsigned char *p = src + length - src_stride;
    while( p >= src ) {
	memcpy( dest, p, line_length );
	p -= src_stride;
	dest += line_length;
    }
}

void pvr2_vram64_read( unsigned char *dest, sh4addr_t srcaddr, uint32_t length )
{
    int bank_flag = (srcaddr & 0x04) >> 2;
    uint32_t *banks[2];
    uint32_t *dwdest;
    int i;

    srcaddr = srcaddr & 0x7FFFFF;
    if( srcaddr + length > 0x800000 )
	length = 0x800000 - srcaddr;

    banks[0] = ((uint32_t *)(video_base + ((srcaddr&0x007FFFF8)>>1)));
    banks[1] = banks[0] + 0x100000;
    if( bank_flag )
	banks[0]++;
    
    /* Handle non-aligned start of source */
    if( srcaddr & 0x03 ) {
	char *src = ((char *)banks[bank_flag]) + (srcaddr & 0x03);
	for( i= srcaddr & 0x03; i < 4 && length > 0; i++, length-- ) {
	    *dest++ = *src++;
	}
	bank_flag = !bank_flag;
    }

    dwdest = (uint32_t *)dest;
    while( length >= 4 ) {
	*dwdest++ = *banks[bank_flag]++;
	bank_flag = !bank_flag;
	length -= 4;
    }
    
    /* Handle non-aligned end of source */
    if( length ) {
	dest = (unsigned char *)dwdest;
	unsigned char *src = (unsigned char *)banks[bank_flag];
	while( length-- > 0 ) {
	    *dest++ = *src++;
	}
    }
}

void pvr2_vram64_dump_file( sh4addr_t addr, uint32_t length, gchar *filename )
{
    uint32_t tmp[length>>2];
    FILE *f = fopen(filename, "wo");
    unsigned int i, j;

    if( f == NULL ) {
	ERROR( "Unable to write to dump file '%s' (%s)", filename, strerror(errno) );
	return;
    }
    pvr2_vram64_read( (unsigned char *)tmp, addr, length );
    fprintf( f, "%08X\n", addr );
    for( i =0; i<length>>2; i+=8 ) {
	for( j=i; j<i+8; j++ ) {
	    if( j < length )
		fprintf( f, " %08X", tmp[j] );
	    else
		fprintf( f, "         " );
	}
	fprintf( f, "\n" );
    }
    fclose(f);
}

void pvr2_vram64_dump( sh4addr_t addr, uint32_t length, FILE *f ) 
{
    unsigned char tmp[length];
    pvr2_vram64_read( tmp, addr, length );
    fwrite_dump( tmp, length, f );
}



/**
 * Flush the indicated render buffer back to PVR. Caller is responsible for
 * tracking whether there is actually anything in the buffer.
 *
 * @param buffer A render buffer indicating the address to store to, and the
 * format the data needs to be in.
 * @param backBuffer TRUE to flush the back buffer, FALSE for 
 * the front buffer.
 */
void pvr2_render_buffer_copy_to_sh4( render_buffer_t buffer )
{
    if( (buffer->address & 0xFF000000) == 0x04000000 ) {
	/* Interlaced buffer. Go the double copy... :( */
	unsigned char target[buffer->size];
	display_driver->read_render_buffer( target, buffer, buffer->rowstride, buffer->colour_format );
	pvr2_vram64_write( buffer->address, target, buffer->size );
    } else {
	/* Regular buffer */
        unsigned char target[buffer->size];
	int line_size = buffer->width * colour_formats[buffer->colour_format].bpp;
	display_driver->read_render_buffer( target, buffer, buffer->rowstride, buffer->colour_format );
        if( (buffer->scale & 0xFFFF) == 0x0800 ) {
            pvr2_vram_write_invert( buffer->address, target, buffer->size, line_size, line_size << 1 );
        } else {
            pvr2_vram_write_invert( buffer->address, target, buffer->size, line_size, line_size );
        }
    }
}

