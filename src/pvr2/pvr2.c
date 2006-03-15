/**
 * $Id: pvr2.c,v 1.20 2006-03-15 13:16:50 nkeynes Exp $
 *
 * PVR2 (Video) Core MMIO registers.
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
#define MODULE pvr2_module

#include "dream.h"
#include "video.h"
#include "mem.h"
#include "asic.h"
#include "pvr2/pvr2.h"
#include "sh4/sh4core.h"
#define MMIO_IMPL
#include "pvr2/pvr2mmio.h"

char *video_base;

void pvr2_init( void );
uint32_t pvr2_run_slice( uint32_t );
void pvr2_display_frame( void );

/**
 * Current PVR2 ram address of the data (if any) currently held in the 
 * OpenGL buffers.
 */

video_driver_t video_driver = NULL;
struct video_buffer video_buffer[2];
int video_buffer_idx = 0;

struct video_timing {
    int fields_per_second;
    int total_lines;
    int retrace_lines;
    int line_time_ns;
};

struct video_timing pal_timing = { 50, 625, 50, 32000 };
struct video_timing ntsc_timing= { 60, 525, 65, 31746 };

struct dreamcast_module pvr2_module = { "PVR2", pvr2_init, NULL, NULL, 
					pvr2_run_slice, NULL,
					NULL, NULL };

void pvr2_init( void )
{
    register_io_region( &mmio_region_PVR2 );
    register_io_region( &mmio_region_PVR2PAL );
    register_io_region( &mmio_region_PVR2TA );
    video_base = mem_get_region_by_name( MEM_REGION_VIDEO );
    pvr2_render_init();
    texcache_init();
}

void video_set_driver( video_driver_t driver )
{
    if( video_driver != NULL && video_driver->shutdown_driver != NULL )
	video_driver->shutdown_driver();

    video_driver = driver;
    if( driver->init_driver != NULL )
	driver->init_driver();
    driver->set_display_format( 640, 480, COLFMT_RGB32 );
    driver->set_render_format( 640, 480, COLFMT_RGB32, FALSE );
    texcache_gl_init();
}

uint32_t pvr2_line_count = 0;
uint32_t pvr2_line_remainder = 0;
uint32_t pvr2_irq_vpos1 = 0;
uint32_t pvr2_irq_vpos2 = 0;
gboolean pvr2_retrace = FALSE;
struct video_timing *pvr2_timing = &ntsc_timing;
uint32_t pvr2_time_counter = 0;
uint32_t pvr2_frame_counter = 0;
uint32_t pvr2_time_per_frame = 20000000;

uint32_t pvr2_run_slice( uint32_t nanosecs ) 
{
    pvr2_line_remainder += nanosecs;
    while( pvr2_line_remainder >= pvr2_timing->line_time_ns ) {
	pvr2_line_remainder -= pvr2_timing->line_time_ns;
	pvr2_line_count++;
	if( pvr2_line_count == pvr2_irq_vpos1 ) {
	    asic_event( EVENT_SCANLINE1 );
	} 
	if( pvr2_line_count == pvr2_irq_vpos2 ) {
	    asic_event( EVENT_SCANLINE2 );
	}
	if( pvr2_line_count == pvr2_timing->total_lines ) {
	    asic_event( EVENT_RETRACE );
	    pvr2_line_count = 0;
	    pvr2_retrace = TRUE;
	} else if( pvr2_line_count == pvr2_timing->retrace_lines ) {
	    if( pvr2_retrace ) {
		pvr2_display_frame();
		pvr2_retrace = FALSE;
	    }
	}
    }
    return nanosecs;
}

uint32_t vid_stride, vid_lpf, vid_ppl, vid_hres, vid_vres, vid_col;
int interlaced, bChanged = 1, bEnabled = 0, vid_size = 0;
char *frame_start; /* current video start address (in real memory) */

/**
 * Display the next frame, copying the current contents of video ram to
 * the window. If the video configuration has changed, first recompute the
 * new frame size/depth.
 */
void pvr2_display_frame( void )
{
    uint32_t display_addr = MMIO_READ( PVR2, DISPADDR1 );
    
    int dispsize = MMIO_READ( PVR2, DISPSIZE );
    int dispmode = MMIO_READ( PVR2, DISPMODE );
    int vidcfg = MMIO_READ( PVR2, DISPCFG );
    int vid_stride = ((dispsize & DISPSIZE_MODULO) >> 20) - 1;
    int vid_lpf = ((dispsize & DISPSIZE_LPF) >> 10) + 1;
    int vid_ppl = ((dispsize & DISPSIZE_PPL)) + 1;
    gboolean bEnabled = (dispmode & DISPMODE_DE) && (vidcfg & DISPCFG_VO ) ? TRUE : FALSE;
    gboolean interlaced = (vidcfg & DISPCFG_I ? TRUE : FALSE);
    if( bEnabled ) {
	video_buffer_t buffer = &video_buffer[video_buffer_idx];
	video_buffer_idx = !video_buffer_idx;
	video_buffer_t last = &video_buffer[video_buffer_idx];
	buffer->rowstride = (vid_ppl + vid_stride) << 2;
	buffer->data = frame_start = video_base + MMIO_READ( PVR2, DISPADDR1 );
	buffer->vres = vid_lpf;
	if( interlaced ) buffer->vres <<= 1;
	switch( (dispmode & DISPMODE_COL) >> 2 ) {
	case 0: 
	    buffer->colour_format = COLFMT_ARGB1555;
	    buffer->hres = vid_ppl << 1; 
	    break;
	case 1: 
	    buffer->colour_format = COLFMT_RGB565;
	    buffer->hres = vid_ppl << 1; 
	    break;
	case 2:
	    buffer->colour_format = COLFMT_RGB888;
	    buffer->hres = (vid_ppl << 2) / 3; 
	    break;
	case 3: 
	    buffer->colour_format = COLFMT_ARGB8888;
	    buffer->hres = vid_ppl; 
	    break;
	}
	
	if( video_driver != NULL ) {
	    if( buffer->hres != last->hres ||
		buffer->vres != last->vres ||
		buffer->colour_format != last->colour_format) {
		video_driver->set_display_format( buffer->hres, buffer->vres,
						  buffer->colour_format );
	    }
	    if( MMIO_READ( PVR2, DISPCFG2 ) & 0x08 ) { /* Blanked */
		uint32_t colour = MMIO_READ( PVR2, DISPBORDER );
		video_driver->display_blank_frame( colour );
	    } else if( !pvr2_render_display_frame( PVR2_RAM_BASE + display_addr ) ) {
		video_driver->display_frame( buffer );
	    }
	}
    } else {
	video_buffer_idx = 0;
	video_buffer[0].hres = video_buffer[0].vres = 0;
    }
    pvr2_frame_counter++;
    asic_event( EVENT_SCANLINE1 );
    asic_event( EVENT_SCANLINE2 );
    asic_event( EVENT_RETRACE );
}

void mmio_region_PVR2_write( uint32_t reg, uint32_t val )
{
    if( reg >= 0x200 && reg < 0x600 ) { /* Fog table */
        MMIO_WRITE( PVR2, reg, val );
        /* I don't want to hear about these */
        return;
    }
    
    INFO( "PVR2 write to %08X <= %08X [%s: %s]", reg, val, 
          MMIO_REGID(PVR2,reg), MMIO_REGDESC(PVR2,reg) );

    MMIO_WRITE( PVR2, reg, val );
   
    switch(reg) {
    case DISPADDR1:
	if( pvr2_retrace ) {
	    pvr2_display_frame();
	    pvr2_retrace = FALSE;
	}
	break;
    case VPOS_IRQ:
	pvr2_irq_vpos1 = (val >> 16) & 0x03FF;
	pvr2_irq_vpos2 = val & 0x03FF;
	break;
    case TAINIT:
	if( val & 0x80000000 )
	    pvr2_ta_init();
	break;
    case RENDSTART:
	if( val == 0xFFFFFFFF )
	    pvr2_render_scene();
	break;
    }
}

MMIO_REGION_READ_FN( PVR2, reg )
{
    switch( reg ) {
        case BEAMPOS:
            return sh4r.icount&0x20 ? 0x2000 : 1;
        default:
            return MMIO_READ( PVR2, reg );
    }
}

MMIO_REGION_DEFFNS( PVR2PAL )

void pvr2_set_base_address( uint32_t base ) 
{
    mmio_region_PVR2_write( DISPADDR1, base );
}




int32_t mmio_region_PVR2TA_read( uint32_t reg )
{
    return 0xFFFFFFFF;
}

void mmio_region_PVR2TA_write( uint32_t reg, uint32_t val )
{
    pvr2_ta_write( &val, sizeof(uint32_t) );
}


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
