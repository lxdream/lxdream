/**
 * $Id: pvr2mem.c,v 1.1 2007-01-15 08:32:09 nkeynes Exp $
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

extern char *video_base;

void pvr2_vram64_write( sh4addr_t destaddr, char *src, uint32_t length )
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
	char *dest = ((char *)banks[bank_flag]) + (destaddr & 0x03);
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
	src = (char *)dwsrc;
	char *dest = (char *)banks[bank_flag];
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
void pvr2_vram64_write_stride( sh4addr_t destaddr, char *src, uint32_t line_bytes, 
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
	*banks[0] += line_gap;
	*banks[1] += line_gap;
	if( line_gap_flag ) {
	    *banks[bank_flag]++;
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
void pvr2_vram64_read_stride( char *dest, uint32_t dest_line_bytes, sh4addr_t srcaddr,
				   uint32_t src_line_bytes, uint32_t line_count )
{
    int bank_flag = (srcaddr & 0x04) >> 2;
    uint32_t *banks[2];
    uint32_t *dwdest;
    uint32_t dest_line_gap;
    uint32_t src_line_gap;
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

void pvr2_vram_write_invert( sh4addr_t destaddr, char *src, uint32_t length, uint32_t line_length )
{
    char *dest = video_base + (destaddr & 0x007FFFFF);
    char *p = src + length - line_length;
    while( p >= src ) {
	memcpy( dest, p, line_length );
	p -= line_length;
	dest += line_length;
    }
}

void pvr2_vram64_read( char *dest, sh4addr_t srcaddr, uint32_t length )
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
	dest = (char *)dwdest;
	char *src = (char *)banks[bank_flag];
	while( length-- > 0 ) {
	    *dest++ = *src++;
	}
    }
}

void pvr2_vram64_dump( sh4addr_t addr, uint32_t length, FILE *f ) 
{
    char tmp[length];
    pvr2_vram64_read( tmp, addr, length );
    fwrite_dump( tmp, length, f );
}
