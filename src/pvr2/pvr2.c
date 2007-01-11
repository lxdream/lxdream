/**
 * $Id: pvr2.c,v 1.36 2007-01-11 06:50:11 nkeynes Exp $
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
#include "eventq.h"
#include "display.h"
#include "mem.h"
#include "asic.h"
#include "clock.h"
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
static void pvr2_update_raster_posn( uint32_t nanosecs );
static void pvr2_schedule_line_event( int eventid, int line );
static void pvr2_schedule_scanline_event( int eventid, int line );
uint32_t pvr2_get_sync_status();

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

struct video_timing pal_timing = { 50, 625, 65, 31945 };
struct video_timing ntsc_timing= { 60, 525, 65, 31746 };

struct pvr2_state {
    uint32_t frame_count;
    uint32_t line_count;
    uint32_t line_remainder;
    uint32_t cycles_run; /* Cycles already executed prior to main time slice */
    uint32_t irq_vpos1;
    uint32_t irq_vpos2;
    uint32_t odd_even_field; /* 1 = odd, 0 = even */

    /* timing */
    uint32_t dot_clock;
    uint32_t total_lines;
    uint32_t line_size;
    uint32_t line_time_ns;
    uint32_t vsync_lines;
    uint32_t hsync_width_ns;
    uint32_t front_porch_ns;
    uint32_t back_porch_ns;
    uint32_t retrace_start_line;
    uint32_t retrace_end_line;
    gboolean interlaced;
    struct video_timing timing;
} pvr2_state;

struct video_buffer video_buffer[2];
int video_buffer_idx = 0;

/**
 * Event handler for the retrace callback (fires on line 0 normally)
 */
static void pvr2_retrace_callback( int eventid ) {
    asic_event( eventid );
    pvr2_update_raster_posn(sh4r.slice_cycle);
    pvr2_schedule_line_event( EVENT_RETRACE, 0 );
}

/**
 * Event handler for the scanline callbacks. Fires the corresponding
 * ASIC event, and resets the timer for the next field.
 */
static void pvr2_scanline_callback( int eventid ) {
    asic_event( eventid );
    pvr2_update_raster_posn(sh4r.slice_cycle);
    if( eventid == EVENT_SCANLINE1 ) {
	pvr2_schedule_scanline_event( eventid, pvr2_state.irq_vpos1 );
    } else {
	pvr2_schedule_scanline_event( eventid, pvr2_state.irq_vpos2 );
    }
}

static void pvr2_init( void )
{
    register_io_region( &mmio_region_PVR2 );
    register_io_region( &mmio_region_PVR2PAL );
    register_io_region( &mmio_region_PVR2TA );
    register_event_callback( EVENT_RETRACE, pvr2_retrace_callback );
    register_event_callback( EVENT_SCANLINE1, pvr2_scanline_callback );
    register_event_callback( EVENT_SCANLINE2, pvr2_scanline_callback );
    video_base = mem_get_region_by_name( MEM_REGION_VIDEO );
    texcache_init();
    pvr2_reset();
    pvr2_ta_reset();
}

static void pvr2_reset( void )
{
    pvr2_state.line_count = 0;
    pvr2_state.line_remainder = 0;
    pvr2_state.cycles_run = 0;
    pvr2_state.irq_vpos1 = 0;
    pvr2_state.irq_vpos2 = 0;
    pvr2_state.timing = ntsc_timing;
    pvr2_state.dot_clock = PVR2_DOT_CLOCK;
    pvr2_state.back_porch_ns = 4000;
    mmio_region_PVR2_write( DISP_TOTAL, 0x0270035F );
    mmio_region_PVR2_write( DISP_SYNCTIME, 0x07D6A53F );
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

/**
 * Update the current raster position to the given number of nanoseconds,
 * relative to the last time slice. (ie the raster will be adjusted forward
 * by nanosecs - nanosecs_already_run_this_timeslice)
 */
static void pvr2_update_raster_posn( uint32_t nanosecs )
{
    uint32_t old_line_count = pvr2_state.line_count;
    if( pvr2_state.line_time_ns == 0 ) {
	return; /* do nothing */
    }
    pvr2_state.line_remainder += (nanosecs - pvr2_state.cycles_run);
    pvr2_state.cycles_run = nanosecs;
    while( pvr2_state.line_remainder >= pvr2_state.line_time_ns ) {
	pvr2_state.line_count ++;
	pvr2_state.line_remainder -= pvr2_state.line_time_ns;
    }

    if( pvr2_state.line_count >= pvr2_state.total_lines ) {
	pvr2_state.line_count -= pvr2_state.total_lines;
	if( pvr2_state.interlaced ) {
	    pvr2_state.odd_even_field = !pvr2_state.odd_even_field;
	}
    }
    if( pvr2_state.line_count >= pvr2_state.retrace_end_line &&
	(old_line_count < pvr2_state.retrace_end_line ||
	 old_line_count > pvr2_state.line_count) ) {
	pvr2_display_frame();
    }
}

static uint32_t pvr2_run_slice( uint32_t nanosecs ) 
{
    pvr2_update_raster_posn( nanosecs );
    pvr2_state.cycles_run = 0;
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
    int vidcfg = MMIO_READ( PVR2, DISP_SYNCCFG );
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
    case GUNPOS: /* Read only registers */
	break;
    case PVRRESET:
	val &= 0x00000007; /* Do stuff? */
	MMIO_WRITE( PVR2, reg, val );
	break;
    case RENDER_START:
	if( val == 0xFFFFFFFF || val == 0x00000001 )
	    pvr2_render_scene();
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
	pvr2_update_raster_posn(sh4r.slice_cycle);
	if( pvr2_state.line_count >= pvr2_state.retrace_start_line ||
	    pvr2_state.line_count < pvr2_state.retrace_end_line ) {
	    pvr2_display_frame();
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
	pvr2_update_raster_posn(sh4r.slice_cycle);
	pvr2_schedule_scanline_event( EVENT_SCANLINE1, pvr2_state.irq_vpos1 );
	pvr2_schedule_scanline_event( EVENT_SCANLINE2, pvr2_state.irq_vpos2 );
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
    case RENDER_TEXSIZE:
	MMIO_WRITE( PVR2, reg, val&0x00031F1F );
	break;
    case RENDER_PALETTE:
	MMIO_WRITE( PVR2, reg, val&0x00000003 );
	break;

	/********** CRTC registers *************/
    case DISP_HBORDER:
    case DISP_VBORDER:
	MMIO_WRITE( PVR2, reg, val&0x03FF03FF );
	break;
    case DISP_TOTAL:
	val = val & 0x03FF03FF;
	MMIO_WRITE( PVR2, reg, val );
	pvr2_update_raster_posn(sh4r.slice_cycle);
	pvr2_state.total_lines = (val >> 16) + 1;
	pvr2_state.line_size = (val & 0x03FF) + 1;
	pvr2_state.line_time_ns = 1000000 * pvr2_state.line_size / pvr2_state.dot_clock;
	pvr2_state.retrace_end_line = 0x2A;
	pvr2_state.retrace_start_line = pvr2_state.total_lines - 6;
	pvr2_schedule_line_event( EVENT_RETRACE, 0 );
	pvr2_schedule_scanline_event( EVENT_SCANLINE1, pvr2_state.irq_vpos1 );
	pvr2_schedule_scanline_event( EVENT_SCANLINE2, pvr2_state.irq_vpos2 );
	break;
    case DISP_SYNCCFG:
	MMIO_WRITE( PVR2, reg, val&0x000003FF );
	pvr2_state.interlaced = (val & 0x0010) ? TRUE : FALSE;
	break;
    case DISP_SYNCTIME:
	pvr2_state.vsync_lines = (val >> 8) & 0x0F;
	pvr2_state.hsync_width_ns = ((val & 0x7F) + 1) * 2000000 / pvr2_state.dot_clock;
	MMIO_WRITE( PVR2, reg, val&0xFFFFFF7F );
	break;
    case DISP_CFG2:
	MMIO_WRITE( PVR2, reg, val&0x003F01FF );
	break;
    case DISP_HPOS:
	val = val & 0x03FF;
	pvr2_state.front_porch_ns = (val + 1) * 1000000 / pvr2_state.dot_clock;
	MMIO_WRITE( PVR2, reg, val );
	break;
    case DISP_VPOS:
	MMIO_WRITE( PVR2, reg, val&0x03FF03FF );
	break;

	/*********** Tile accelerator registers ***********/
    case TA_POLYPOS:
    case TA_LISTPOS:
	/* Readonly registers */
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
    case TA_INIT:
	if( val & 0x80000000 )
	    pvr2_ta_init();
	break;
    case TA_REINIT:
	break;
	/**************** Scaler registers? ****************/
    case SCALERCFG:
	/* KOS suggests bits as follows:
	 *   0: enable vertical scaling
	 *  10: ???
	 *  16: enable FSAA
	 */
	DEBUG( "Scaler config set to %08X", val );
	MMIO_WRITE( PVR2, reg, val&0x0007FFFF );
	break;

    case YUV_ADDR:
	MMIO_WRITE( PVR2, reg, val&0x00FFFFF8 );
	break;
    case YUV_CFG:
	DEBUG( "YUV config set to %08X", val );
	MMIO_WRITE( PVR2, reg, val&0x01013F3F );
	break;


	/**************** Unknowns ***************/
    case PVRUNK1:
    	MMIO_WRITE( PVR2, reg, val&0x000007FF );
    	break;
    case PVRUNK2:
	MMIO_WRITE( PVR2, reg, val&0x00000007 );
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
    case PVRUNK7:
	MMIO_WRITE( PVR2, reg, val&0x00000001 );
	break;
    }
}

/**
 * Calculate the current read value of the syncstat register, using
 * the current SH4 clock time as an offset from the last timeslice.
 * The register reads (LSB to MSB) as:
 *     0..9  Current scan line
 *     10    Odd/even field (1 = odd, 0 = even)
 *     11    Display active (including border and overscan)
 *     12    Horizontal sync off
 *     13    Vertical sync off
 * Note this method is probably incorrect for anything other than straight
 * interlaced PAL/NTSC, and needs further testing. 
 */
uint32_t pvr2_get_sync_status()
{
    pvr2_update_raster_posn(sh4r.slice_cycle);
    uint32_t result = pvr2_state.line_count;

    if( pvr2_state.odd_even_field ) {
	result |= 0x0400;
    }
    if( (pvr2_state.line_count & 0x01) == pvr2_state.odd_even_field ) {
	if( pvr2_state.line_remainder > pvr2_state.hsync_width_ns ) {
	    result |= 0x1000; /* !HSYNC */
	}
	if( pvr2_state.line_count >= pvr2_state.vsync_lines ) {
	    if( pvr2_state.line_remainder > pvr2_state.front_porch_ns ) {
		result |= 0x2800; /* Display active */
	    } else {
		result |= 0x2000; /* Front porch */
	    }
	}
    } else {
	if( pvr2_state.line_count >= pvr2_state.vsync_lines ) {
	    if( pvr2_state.line_remainder < (pvr2_state.line_time_ns - pvr2_state.back_porch_ns)) {
		result |= 0x3800; /* Display active */
	    } else {
		result |= 0x3000;
	    }
	} else {
	    result |= 0x1000; /* Back porch */
	}
    }
    return result;
}

/**
 * Schedule an event for the start of the given line. If the line is actually
 * the current line, schedules it for the next field. 
 * The raster position should be updated before calling this method.
 */
static void pvr2_schedule_line_event( int eventid, int line )
{
    uint32_t time;
    if( line <= pvr2_state.line_count ) {
	time = (pvr2_state.total_lines - pvr2_state.line_count + line) * pvr2_state.line_time_ns
	    - pvr2_state.line_remainder;
    } else {
	time = (line - pvr2_state.line_count) * pvr2_state.line_time_ns - pvr2_state.line_remainder;
    }

    if( line < pvr2_state.total_lines ) {
	event_schedule( eventid, time );
    } else {
	event_cancel( eventid );
    }
}

/**
 * Schedule a "scanline" event. This actually goes off at
 * 2 * line in even fields and 2 * line + 1 in odd fields.
 * Otherwise this behaves as per pvr2_schedule_line_event().
 * The raster position should be updated before calling this
 * method.
 */
static void pvr2_schedule_scanline_event( int eventid, int line )
{
    uint32_t field = pvr2_state.odd_even_field;
    if( line <= pvr2_state.line_count && pvr2_state.interlaced ) {
	field = !field;
    }

    line <<= 1;
    if( field ) {
	line += 1;
    }
    pvr2_schedule_line_event( eventid, line );
}

MMIO_REGION_READ_FN( PVR2, reg )
{
    switch( reg ) {
        case DISP_SYNCSTAT:
            return pvr2_get_sync_status();
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
