/**
 * $Id: pvr2.c,v 1.22 2006-03-30 11:30:59 nkeynes Exp $
 *
 * PVR2 (Video) Core module implementation and MMIO registers.
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

static void pvr2_init( void );
static void pvr2_reset( void );
static uint32_t pvr2_run_slice( uint32_t );
static void pvr2_save_state( FILE *f );
static int pvr2_load_state( FILE *f );

void pvr2_display_frame( void );

struct dreamcast_module pvr2_module = { "PVR2", pvr2_init, pvr2_reset, NULL, 
					pvr2_run_slice, NULL,
					pvr2_save_state, pvr2_load_state };


video_driver_t video_driver = NULL;

struct video_timing {
    int fields_per_second;
    int total_lines;
    int retrace_lines;
    int line_time_ns;
};

struct video_timing pal_timing = { 50, 625, 65, 32000 };
struct video_timing ntsc_timing= { 60, 525, 65, 31746 };

struct pvr2_state {
    uint32_t frame_count;
    uint32_t line_count;
    uint32_t line_remainder;
    uint32_t irq_vpos1;
    uint32_t irq_vpos2;
    gboolean retrace;
    struct video_timing timing;
} pvr2_state;

struct video_buffer video_buffer[2];
int video_buffer_idx = 0;

static void pvr2_init( void )
{
    register_io_region( &mmio_region_PVR2 );
    register_io_region( &mmio_region_PVR2PAL );
    register_io_region( &mmio_region_PVR2TA );
    video_base = mem_get_region_by_name( MEM_REGION_VIDEO );
    texcache_init();
    pvr2_reset();
}

static void pvr2_reset( void )
{
    pvr2_state.line_count = 0;
    pvr2_state.line_remainder = 0;
    pvr2_state.irq_vpos1 = 0;
    pvr2_state.irq_vpos2 = 0;
    pvr2_state.retrace = FALSE;
    pvr2_state.timing = ntsc_timing;
    video_buffer_idx = 0;
    
    pvr2_ta_init();
    pvr2_render_init();
    texcache_flush();
}

static void pvr2_save_state( FILE *f )
{
    fwrite( &pvr2_state, sizeof(pvr2_state), 1, f );
}

static int pvr2_load_state( FILE *f )
{
    fread( &pvr2_state, sizeof(pvr2_state), 1, f );
}

static uint32_t pvr2_run_slice( uint32_t nanosecs ) 
{
    pvr2_state.line_remainder += nanosecs;
    while( pvr2_state.line_remainder >= pvr2_state.timing.line_time_ns ) {
	pvr2_state.line_remainder -= pvr2_state.timing.line_time_ns;

	pvr2_state.line_count++;
	if( pvr2_state.line_count == pvr2_state.timing.total_lines ) {
	    asic_event( EVENT_RETRACE );
	    pvr2_state.line_count = 0;
	    pvr2_state.retrace = TRUE;
	}

	if( pvr2_state.line_count == pvr2_state.irq_vpos1 ) {
	    asic_event( EVENT_SCANLINE1 );
	} 
	if( pvr2_state.line_count == pvr2_state.irq_vpos2 ) {
	    asic_event( EVENT_SCANLINE2 );
	}

	if( pvr2_state.line_count == pvr2_state.timing.retrace_lines ) {
	    if( pvr2_state.retrace ) {
		pvr2_display_frame();
		pvr2_state.retrace = FALSE;
	    }
	}
    }
    return nanosecs;
}

int pvr2_get_frame_count() 
{
    return pvr2_state.frame_count;
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
	buffer->data = video_base + MMIO_READ( PVR2, DISPADDR1 );
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
    pvr2_state.frame_count++;
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
	if( pvr2_state.retrace ) {
	    pvr2_display_frame();
	    pvr2_state.retrace = FALSE;
	}
	break;
    case VPOS_IRQ:
	pvr2_state.irq_vpos1 = (val >> 16) & 0x03FF;
	pvr2_state.irq_vpos2 = val & 0x03FF;
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

void pvr2_vram64_dump( sh4addr_t addr, uint32_t length, FILE *f ) 
{
    char tmp[length];
    pvr2_vram64_read( tmp, addr, length );
    fwrite_dump( tmp, length, f );
}
