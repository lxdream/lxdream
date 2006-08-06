/**
 * $Id: pvr2.c,v 1.31 2006-08-06 02:47:08 nkeynes Exp $
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
#include "display.h"
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

int colour_format_bytes[] = { 2, 2, 2, 1, 3, 4, 1, 1 };

struct dreamcast_module pvr2_module = { "PVR2", pvr2_init, pvr2_reset, NULL, 
					pvr2_run_slice, NULL,
					pvr2_save_state, pvr2_load_state };


display_driver_t display_driver = NULL;

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
    pvr2_ta_save_state( f );
}

static int pvr2_load_state( FILE *f )
{
    if( fread( &pvr2_state, sizeof(pvr2_state), 1, f ) != 1 )
	return 1;
    return pvr2_ta_load_state(f);
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

/**
 * Display the next frame, copying the current contents of video ram to
 * the window. If the video configuration has changed, first recompute the
 * new frame size/depth.
 */
void pvr2_display_frame( void )
{
    uint32_t display_addr = MMIO_READ( PVR2, DISP_ADDR1 );
    
    int dispsize = MMIO_READ( PVR2, DISP_SIZE );
    int dispmode = MMIO_READ( PVR2, DISP_MODE );
    int vidcfg = MMIO_READ( PVR2, DISP_CFG );
    int vid_stride = ((dispsize & DISPSIZE_MODULO) >> 20) - 1;
    int vid_lpf = ((dispsize & DISPSIZE_LPF) >> 10) + 1;
    int vid_ppl = ((dispsize & DISPSIZE_PPL)) + 1;
    gboolean bEnabled = (dispmode & DISPMODE_DE) && (vidcfg & DISPCFG_VO ) ? TRUE : FALSE;
    gboolean interlaced = (vidcfg & DISPCFG_I ? TRUE : FALSE);
    video_buffer_t buffer = &video_buffer[video_buffer_idx];
    video_buffer_idx = !video_buffer_idx;
    video_buffer_t last = &video_buffer[video_buffer_idx];
    buffer->rowstride = (vid_ppl + vid_stride) << 2;
    buffer->data = video_base + MMIO_READ( PVR2, DISP_ADDR1 );
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
	
    if( buffer->hres <=8 )
	buffer->hres = 640;
    if( buffer->vres <=8 )
	buffer->vres = 480;
    if( display_driver != NULL ) {
	if( buffer->hres != last->hres ||
	    buffer->vres != last->vres ||
	    buffer->colour_format != last->colour_format) {
	    display_driver->set_display_format( buffer->hres, buffer->vres,
						buffer->colour_format );
	}
	if( !bEnabled ) {
	    display_driver->display_blank_frame( 0 );
	} else if( MMIO_READ( PVR2, DISP_CFG2 ) & 0x08 ) { /* Blanked */
	    uint32_t colour = MMIO_READ( PVR2, DISP_BORDER );
	    display_driver->display_blank_frame( colour );
	} else if( !pvr2_render_display_frame( PVR2_RAM_BASE + display_addr ) ) {
	    display_driver->display_frame( buffer );
	}
    }
    pvr2_state.frame_count++;
}

/**
 * This has to handle every single register individually as they all get masked 
 * off differently (and its easier to do it at write time)
 */
void mmio_region_PVR2_write( uint32_t reg, uint32_t val )
{
    if( reg >= 0x200 && reg < 0x600 ) { /* Fog table */
        MMIO_WRITE( PVR2, reg, val );
        return;
    }
    
    switch(reg) {
    case PVRID:
    case PVRVER:
    case GUNPOS:
    case TA_POLYPOS:
    case TA_LISTPOS:
	/* Readonly registers */
	break;
    case PVRRESET:
	val &= 0x00000007; /* Do stuff? */
	MMIO_WRITE( PVR2, reg, val );
	break;
    case RENDER_START:
	if( val == 0xFFFFFFFF )
	    pvr2_render_scene();
	break;
    case PVRUNK1:
    	MMIO_WRITE( PVR2, reg, val&0x000007FF );
    	break;
    case RENDER_POLYBASE:
    	MMIO_WRITE( PVR2, reg, val&0x00F00000 );
    	break;
    case RENDER_TSPCFG:
    	MMIO_WRITE( PVR2, reg, val&0x00010101 );
    	break;
    case DISP_BORDER:
    	MMIO_WRITE( PVR2, reg, val&0x01FFFFFF );
    	break;
    case DISP_MODE:
    	MMIO_WRITE( PVR2, reg, val&0x00FFFF7F );
    	break;
    case RENDER_MODE:
    	MMIO_WRITE( PVR2, reg, val&0x00FFFF0F );
    	break;
    case RENDER_SIZE:
    	MMIO_WRITE( PVR2, reg, val&0x000001FF );
    	break;
    case DISP_ADDR1:
	val &= 0x00FFFFFC;
	MMIO_WRITE( PVR2, reg, val );
	if( pvr2_state.retrace ) {
	    pvr2_display_frame();
	    pvr2_state.retrace = FALSE;
	}
	break;
    case DISP_ADDR2:
    	MMIO_WRITE( PVR2, reg, val&0x00FFFFFC );
    	break;
    case DISP_SIZE:
    	MMIO_WRITE( PVR2, reg, val&0x3FFFFFFF );
    	break;
    case RENDER_ADDR1:
    case RENDER_ADDR2:
    	MMIO_WRITE( PVR2, reg, val&0x01FFFFFC );
    	break;
    case RENDER_HCLIP:
	MMIO_WRITE( PVR2, reg, val&0x07FF07FF );
	break;
    case RENDER_VCLIP:
	MMIO_WRITE( PVR2, reg, val&0x03FF03FF );
	break;
    case DISP_HPOSIRQ:
	MMIO_WRITE( PVR2, reg, val&0x03FF33FF );
	break;
    case DISP_VPOSIRQ:
	val = val & 0x03FF03FF;
	pvr2_state.irq_vpos1 = (val >> 16);
	pvr2_state.irq_vpos2 = val & 0x03FF;
	MMIO_WRITE( PVR2, reg, val );
	break;
    case RENDER_NEARCLIP:
	MMIO_WRITE( PVR2, reg, val & 0x7FFFFFFF );
	break;
    case RENDER_SHADOW:
	MMIO_WRITE( PVR2, reg, val&0x000001FF );
	break;
    case RENDER_OBJCFG:
    	MMIO_WRITE( PVR2, reg, val&0x003FFFFF );
    	break;
    case PVRUNK2:
	MMIO_WRITE( PVR2, reg, val&0x00000007 );
	break;
    case RENDER_TSPCLIP:
    	MMIO_WRITE( PVR2, reg, val&0x7FFFFFFF );
    	break;
    case RENDER_FARCLIP:
	MMIO_WRITE( PVR2, reg, val&0xFFFFFFF0 );
	break;
    case RENDER_BGPLANE:
    	MMIO_WRITE( PVR2, reg, val&0x1FFFFFFF );
    	break;
    case RENDER_ISPCFG:
    	MMIO_WRITE( PVR2, reg, val&0x00FFFFF9 );
    	break;
    case VRAM_CFG1:
	MMIO_WRITE( PVR2, reg, val&0x000000FF );
	break;
    case VRAM_CFG2:
	MMIO_WRITE( PVR2, reg, val&0x003FFFFF );
	break;
    case VRAM_CFG3:
	MMIO_WRITE( PVR2, reg, val&0x1FFFFFFF );
	break;
    case RENDER_FOGTBLCOL:
    case RENDER_FOGVRTCOL:
	MMIO_WRITE( PVR2, reg, val&0x00FFFFFF );
	break;
    case RENDER_FOGCOEFF:
	MMIO_WRITE( PVR2, reg, val&0x0000FFFF );
	break;
    case RENDER_CLAMPHI:
    case RENDER_CLAMPLO:
	MMIO_WRITE( PVR2, reg, val );
	break;
    case DISP_CFG:
	MMIO_WRITE( PVR2, reg, val&0x000003FF );
	break;
    case DISP_HBORDER:
    case DISP_SYNC:
    case DISP_VBORDER:
	MMIO_WRITE( PVR2, reg, val&0x03FF03FF );
	break;
    case DISP_SYNC2:
	MMIO_WRITE( PVR2, reg, val&0xFFFFFF7F );
	break;
    case RENDER_TEXSIZE:
	MMIO_WRITE( PVR2, reg, val&0x00031F1F );
	break;
    case DISP_CFG2:
	MMIO_WRITE( PVR2, reg, val&0x003F01FF );
	break;
    case DISP_HPOS:
	MMIO_WRITE( PVR2, reg, val&0x000003FF );
	break;
    case DISP_VPOS:
	MMIO_WRITE( PVR2, reg, val&0x03FF03FF );
	break;
    case SCALERCFG:
	MMIO_WRITE( PVR2, reg, val&0x0007FFFF );
	break;
    case RENDER_PALETTE:
	MMIO_WRITE( PVR2, reg, val&0x00000003 );
	break;
    case PVRUNK3:
	MMIO_WRITE( PVR2, reg, val&0x000FFF3F );
	break;
    case PVRUNK5:
	MMIO_WRITE( PVR2, reg, val&0x0000FFFF );
	break;
    case PVRUNK6:
	MMIO_WRITE( PVR2, reg, val&0x000000FF );
	break;
    case TA_TILEBASE:
    case TA_LISTEND:
    case TA_LISTBASE:
	MMIO_WRITE( PVR2, reg, val&0x00FFFFE0 );
	break;
    case RENDER_TILEBASE:
    case TA_POLYBASE:
    case TA_POLYEND:
	MMIO_WRITE( PVR2, reg, val&0x00FFFFFC );
	break;
    case TA_TILESIZE:
	MMIO_WRITE( PVR2, reg, val&0x000F003F );
	break;
    case TA_TILECFG:
	MMIO_WRITE( PVR2, reg, val&0x00133333 );
	break;
    case YUV_ADDR:
	MMIO_WRITE( PVR2, reg, val&0x00FFFFF8 );
	break;
    case YUV_CFG:
	MMIO_WRITE( PVR2, reg, val&0x01013F3F );
	break;
    case TA_INIT:
	if( val & 0x80000000 )
	    pvr2_ta_init();
	break;
    case TA_REINIT:
	break;
    case PVRUNK7:
	MMIO_WRITE( PVR2, reg, val&0x00000001 );
	break;
    }
}

MMIO_REGION_READ_FN( PVR2, reg )
{
    switch( reg ) {
        case DISP_BEAMPOS:
            return sh4r.icount&0x20 ? 0x2000 : 1;
        default:
            return MMIO_READ( PVR2, reg );
    }
}

MMIO_REGION_DEFFNS( PVR2PAL )

void pvr2_set_base_address( uint32_t base ) 
{
    mmio_region_PVR2_write( DISP_ADDR1, base );
}




int32_t mmio_region_PVR2TA_read( uint32_t reg )
{
    return 0xFFFFFFFF;
}

void mmio_region_PVR2TA_write( uint32_t reg, uint32_t val )
{
    pvr2_ta_write( (char *)&val, sizeof(uint32_t) );
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
