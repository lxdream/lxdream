/**
 * $Id: pvr2.c,v 1.44 2007-02-11 10:09:32 nkeynes Exp $
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

#define MAX_RENDER_BUFFERS 4

#define HPOS_PER_FRAME 0
#define HPOS_PER_LINECOUNT 1

static void pvr2_init( void );
static void pvr2_reset( void );
static uint32_t pvr2_run_slice( uint32_t );
static void pvr2_save_state( FILE *f );
static int pvr2_load_state( FILE *f );
static void pvr2_update_raster_posn( uint32_t nanosecs );
static void pvr2_schedule_scanline_event( int eventid, int line, int minimum_lines, int line_time_ns );
static render_buffer_t pvr2_get_render_buffer( frame_buffer_t frame );
static render_buffer_t pvr2_next_render_buffer( );
uint32_t pvr2_get_sync_status();

void pvr2_display_frame( void );

static int output_colour_formats[] = { COLFMT_ARGB1555, COLFMT_RGB565, COLFMT_RGB888, COLFMT_ARGB8888 };

struct dreamcast_module pvr2_module = { "PVR2", pvr2_init, pvr2_reset, NULL, 
					pvr2_run_slice, NULL,
					pvr2_save_state, pvr2_load_state };


display_driver_t display_driver = NULL;

struct pvr2_state {
    uint32_t frame_count;
    uint32_t line_count;
    uint32_t line_remainder;
    uint32_t cycles_run; /* Cycles already executed prior to main time slice */
    uint32_t irq_hpos_line;
    uint32_t irq_hpos_line_count;
    uint32_t irq_hpos_mode;
    uint32_t irq_hpos_time_ns; /* Time within the line */
    uint32_t irq_vpos1;
    uint32_t irq_vpos2;
    uint32_t odd_even_field; /* 1 = odd, 0 = even */
    gboolean palette_changed; /* TRUE if palette has changed since last render */
    gchar *save_next_render_filename;
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
} pvr2_state;

render_buffer_t render_buffers[MAX_RENDER_BUFFERS];
int render_buffer_count = 0;

/**
 * Event handler for the hpos callback
 */
static void pvr2_hpos_callback( int eventid ) {
    asic_event( eventid );
    pvr2_update_raster_posn(sh4r.slice_cycle);
    if( pvr2_state.irq_hpos_mode == HPOS_PER_LINECOUNT ) {
	pvr2_state.irq_hpos_line += pvr2_state.irq_hpos_line_count;
	while( pvr2_state.irq_hpos_line > (pvr2_state.total_lines>>1) ) {
	    pvr2_state.irq_hpos_line -= (pvr2_state.total_lines>>1);
	}
    }
    pvr2_schedule_scanline_event( eventid, pvr2_state.irq_hpos_line, 1, 
				  pvr2_state.irq_hpos_time_ns );
}

/**
 * Event handler for the scanline callbacks. Fires the corresponding
 * ASIC event, and resets the timer for the next field.
 */
static void pvr2_scanline_callback( int eventid ) {
    asic_event( eventid );
    pvr2_update_raster_posn(sh4r.slice_cycle);
    if( eventid == EVENT_SCANLINE1 ) {
	pvr2_schedule_scanline_event( eventid, pvr2_state.irq_vpos1, 1, 0 );
    } else {
	pvr2_schedule_scanline_event( eventid, pvr2_state.irq_vpos2, 1, 0 );
    }
}

static void pvr2_init( void )
{
    int i;
    register_io_region( &mmio_region_PVR2 );
    register_io_region( &mmio_region_PVR2PAL );
    register_io_region( &mmio_region_PVR2TA );
    register_event_callback( EVENT_HPOS, pvr2_hpos_callback );
    register_event_callback( EVENT_SCANLINE1, pvr2_scanline_callback );
    register_event_callback( EVENT_SCANLINE2, pvr2_scanline_callback );
    video_base = mem_get_region_by_name( MEM_REGION_VIDEO );
    texcache_init();
    pvr2_reset();
    pvr2_ta_reset();
    pvr2_state.save_next_render_filename = NULL;
    for( i=0; i<MAX_RENDER_BUFFERS; i++ ) {
	render_buffers[i] = NULL;
    }
    render_buffer_count = 0;
}

static void pvr2_reset( void )
{
    pvr2_state.line_count = 0;
    pvr2_state.line_remainder = 0;
    pvr2_state.cycles_run = 0;
    pvr2_state.irq_vpos1 = 0;
    pvr2_state.irq_vpos2 = 0;
    pvr2_state.dot_clock = PVR2_DOT_CLOCK;
    pvr2_state.back_porch_ns = 4000;
    pvr2_state.palette_changed = FALSE;
    mmio_region_PVR2_write( DISP_TOTAL, 0x0270035F );
    mmio_region_PVR2_write( DISP_SYNCTIME, 0x07D6A53F );
    mmio_region_PVR2_write( YUV_ADDR, 0 );
    mmio_region_PVR2_write( YUV_CFG, 0 );
    
    pvr2_ta_init();
    texcache_flush();
}

static void pvr2_save_state( FILE *f )
{
    fwrite( &pvr2_state, sizeof(pvr2_state), 1, f );
    pvr2_ta_save_state( f );
    pvr2_yuv_save_state( f );
}

static int pvr2_load_state( FILE *f )
{
    if( fread( &pvr2_state, sizeof(pvr2_state), 1, f ) != 1 )
	return 1;
    if( pvr2_ta_load_state(f) ) {
	return 1;
    }
    return pvr2_yuv_load_state(f);
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
	pvr2_state.frame_count++;
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

gboolean pvr2_save_next_scene( const gchar *filename )
{
    if( pvr2_state.save_next_render_filename != NULL ) {
	g_free( pvr2_state.save_next_render_filename );
    } 
    pvr2_state.save_next_render_filename = g_strdup(filename);
    return TRUE;
}



/**
 * Display the next frame, copying the current contents of video ram to
 * the window. If the video configuration has changed, first recompute the
 * new frame size/depth.
 */
void pvr2_display_frame( void )
{
    int dispmode = MMIO_READ( PVR2, DISP_MODE );
    int vidcfg = MMIO_READ( PVR2, DISP_SYNCCFG );
    gboolean bEnabled = (dispmode & DISPMODE_ENABLE) && (vidcfg & DISPCFG_VO ) ? TRUE : FALSE;

    if( display_driver == NULL ) {
	return; /* can't really do anything much */
    } else if( !bEnabled ) {
	/* Output disabled == black */
	display_driver->display_blank( 0 ); 
    } else if( MMIO_READ( PVR2, DISP_CFG2 ) & 0x08 ) { 
	/* Enabled but blanked - border colour */
	uint32_t colour = MMIO_READ( PVR2, DISP_BORDER );
	display_driver->display_blank( colour );
    } else {
	/* Real output - determine dimensions etc */
	struct frame_buffer fbuf;
	uint32_t dispsize = MMIO_READ( PVR2, DISP_SIZE );
	int vid_stride = (((dispsize & DISPSIZE_MODULO) >> 20) - 1);
	int vid_ppl = ((dispsize & DISPSIZE_PPL)) + 1;

	fbuf.colour_format = output_colour_formats[(dispmode & DISPMODE_COLFMT) >> 2];
	fbuf.width = vid_ppl << 2 / colour_formats[fbuf.colour_format].bpp;
	fbuf.height = ((dispsize & DISPSIZE_LPF) >> 10) + 1;
	fbuf.size = vid_ppl << 2 * fbuf.height;
	fbuf.rowstride = (vid_ppl + vid_stride) << 2;

	/* Determine the field to display, and deinterlace if possible */
	if( pvr2_state.interlaced ) {
	    if( vid_ppl == vid_stride ) { /* Magic deinterlace */
		fbuf.height = fbuf.height << 1;
		fbuf.rowstride = vid_ppl << 2;
		fbuf.address = MMIO_READ( PVR2, DISP_ADDR1 );
	    } else { 
		/* Just display the field as is, folks. This is slightly tricky -
		 * we pick the field based on which frame is about to come through,
		 * which may not be the same as the odd_even_field.
		 */
		gboolean oddfield = pvr2_state.odd_even_field;
		if( pvr2_state.line_count >= pvr2_state.retrace_start_line ) {
		    oddfield = !oddfield;
		}
		if( oddfield ) {
		    fbuf.address = MMIO_READ( PVR2, DISP_ADDR1 );
		} else {
		    fbuf.address = MMIO_READ( PVR2, DISP_ADDR2 );
		}
	    }
	} else {
	    fbuf.address = MMIO_READ( PVR2, DISP_ADDR1 );
	}
	fbuf.address = (fbuf.address & 0x00FFFFFF) + PVR2_RAM_BASE;

	render_buffer_t rbuf = pvr2_get_render_buffer( &fbuf );
	if( rbuf != NULL ) {
	    display_driver->display_render_buffer( rbuf );
	} else {
	    fbuf.data = video_base + (fbuf.address&0x00FFFFFF);
	    display_driver->display_frame_buffer( &fbuf );
	}
    }
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
    case RENDER_START: /* Don't really care what value */
	if( pvr2_state.save_next_render_filename != NULL ) {
	    if( pvr2_render_save_scene(pvr2_state.save_next_render_filename) == 0 ) {
		INFO( "Saved scene to %s", pvr2_state.save_next_render_filename);
	    }
	    g_free( pvr2_state.save_next_render_filename );
	    pvr2_state.save_next_render_filename = NULL;
	}
	render_buffer_t buffer = pvr2_next_render_buffer();
	pvr2_render_scene( buffer );
	asic_event( EVENT_PVR_RENDER_DONE );
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
	break;
    case DISP_ADDR2:
    	MMIO_WRITE( PVR2, reg, val&0x00FFFFFC );
	pvr2_update_raster_posn(sh4r.slice_cycle);
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
	pvr2_state.irq_hpos_line = val & 0x03FF;
	pvr2_state.irq_hpos_time_ns = 2000000*((val>>16)&0x03FF)/pvr2_state.dot_clock;
	pvr2_state.irq_hpos_mode = (val >> 12) & 0x03;
	switch( pvr2_state.irq_hpos_mode ) {
	case 3: /* Reserved - treat as 0 */
	case 0: /* Once per frame at specified line */
	    pvr2_state.irq_hpos_mode = HPOS_PER_FRAME;
	    break;
	case 2: /* Once per line - as per-line-count */
	    pvr2_state.irq_hpos_line = 1;
	    pvr2_state.irq_hpos_mode = 1;
	case 1: /* Once per N lines */
	    pvr2_state.irq_hpos_line_count = pvr2_state.irq_hpos_line;
	    pvr2_state.irq_hpos_line = (pvr2_state.line_count >> 1) + 
		pvr2_state.irq_hpos_line_count;
	    while( pvr2_state.irq_hpos_line > (pvr2_state.total_lines>>1) ) {
		pvr2_state.irq_hpos_line -= (pvr2_state.total_lines>>1);
	    }
	    pvr2_state.irq_hpos_mode = HPOS_PER_LINECOUNT;
	}
	pvr2_schedule_scanline_event( EVENT_HPOS, pvr2_state.irq_hpos_line, 0,
					  pvr2_state.irq_hpos_time_ns );
	break;
    case DISP_VPOSIRQ:
	val = val & 0x03FF03FF;
	pvr2_state.irq_vpos1 = (val >> 16);
	pvr2_state.irq_vpos2 = val & 0x03FF;
	pvr2_update_raster_posn(sh4r.slice_cycle);
	pvr2_schedule_scanline_event( EVENT_SCANLINE1, pvr2_state.irq_vpos1, 0, 0 );
	pvr2_schedule_scanline_event( EVENT_SCANLINE2, pvr2_state.irq_vpos2, 0, 0 );
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
	pvr2_schedule_scanline_event( EVENT_SCANLINE1, pvr2_state.irq_vpos1, 0, 0 );
	pvr2_schedule_scanline_event( EVENT_SCANLINE2, pvr2_state.irq_vpos2, 0, 0 );
	pvr2_schedule_scanline_event( EVENT_HPOS, pvr2_state.irq_hpos_line, 0, 
					  pvr2_state.irq_hpos_time_ns );
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
    case RENDER_SCALER:
	MMIO_WRITE( PVR2, reg, val&0x0007FFFF );
	break;

    case YUV_ADDR:
	val = val & 0x00FFFFF8;
	MMIO_WRITE( PVR2, reg, val );
	pvr2_yuv_init( val );
	break;
    case YUV_CFG:
	MMIO_WRITE( PVR2, reg, val&0x01013F3F );
	pvr2_yuv_set_config(val);
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
 * Schedule a "scanline" event. This actually goes off at
 * 2 * line in even fields and 2 * line + 1 in odd fields.
 * Otherwise this behaves as per pvr2_schedule_line_event().
 * The raster position should be updated before calling this
 * method.
 * @param eventid Event to fire at the specified time
 * @param line Line on which to fire the event (this is 2n/2n+1 for interlaced
 *  displays). 
 * @param hpos_ns Nanoseconds into the line at which to fire.
 */
static void pvr2_schedule_scanline_event( int eventid, int line, int minimum_lines, int hpos_ns )
{
    uint32_t field = pvr2_state.odd_even_field;
    if( line <= pvr2_state.line_count && pvr2_state.interlaced ) {
	field = !field;
    }
    if( hpos_ns > pvr2_state.line_time_ns ) {
	hpos_ns = pvr2_state.line_time_ns;
    }

    line <<= 1;
    if( field ) {
	line += 1;
    }
    
    if( line < pvr2_state.total_lines ) {
	uint32_t lines;
	uint32_t time;
	if( line <= pvr2_state.line_count ) {
	    lines = (pvr2_state.total_lines - pvr2_state.line_count + line);
	} else {
	    lines = (line - pvr2_state.line_count);
	}
	if( lines <= minimum_lines ) {
	    lines += pvr2_state.total_lines;
	}
	time = (lines * pvr2_state.line_time_ns) - pvr2_state.line_remainder + hpos_ns;
	event_schedule( eventid, time );
    } else {
	event_cancel( eventid );
    }
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

MMIO_REGION_WRITE_FN( PVR2PAL, reg, val )
{
    MMIO_WRITE( PVR2PAL, reg, val );
    pvr2_state.palette_changed = TRUE;
}

void pvr2_check_palette_changed()
{
    if( pvr2_state.palette_changed ) {
	texcache_invalidate_palette();
	pvr2_state.palette_changed = FALSE;
    }
}

MMIO_REGION_READ_DEFFN( PVR2PAL );

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

/**
 * Find the render buffer corresponding to the requested output frame
 * (does not consider texture renders). 
 * @return the render_buffer if found, or null if no such buffer.
 *
 * Note: Currently does not consider "partial matches", ie partial
 * frame overlap - it probably needs to do this.
 */
render_buffer_t pvr2_get_render_buffer( frame_buffer_t frame )
{
    int i;
    for( i=0; i<render_buffer_count; i++ ) {
	if( render_buffers[i] != NULL && render_buffers[i]->address == frame->address ) {
	    return render_buffers[i];
	}
    }
    return NULL;
}

/**
 * Determine the next render buffer to write into. The order of preference is:
 *   1. An existing buffer with the same address. (not flushed unless the new
 * size is smaller than the old one).
 *   2. An existing buffer with the same size chosen by LRU order. Old buffer
 *       is flushed to vram.
 *   3. A new buffer if one can be created.
 *   4. The current display buff
 * Note: The current display field(s) will never be overwritten except as a last
 * resort.
 */
render_buffer_t pvr2_next_render_buffer()
{
    render_buffer_t result = NULL;
    uint32_t render_addr = MMIO_READ( PVR2, RENDER_ADDR1 );
    uint32_t render_mode = MMIO_READ( PVR2, RENDER_MODE );
    uint32_t render_scale = MMIO_READ( PVR2, RENDER_SCALER );
    uint32_t render_stride = MMIO_READ( PVR2, RENDER_SIZE ) << 3;
    gboolean render_to_tex;
    if( render_addr & 0x01000000 ) { /* vram64 */
	render_addr = (render_addr & 0x00FFFFFF) + PVR2_RAM_BASE_INT;
    } else { /* vram32 */
	render_addr = (render_addr & 0x00FFFFFF) + PVR2_RAM_BASE;
    }

    int width, height, i;
    int colour_format = pvr2_render_colour_format[render_mode&0x07];
    pvr2_render_getsize( &width, &height );

    /* Check existing buffers for an available buffer */
    for( i=0; i<render_buffer_count; i++ ) {
	if( render_buffers[i]->width == width && render_buffers[i]->height == height ) {
	    /* needs to be the right dimensions */
	    if( render_buffers[i]->address == render_addr ) {
		/* perfect */
		result = render_buffers[i];
		break;
	    } else if( render_buffers[i]->address == -1 && result == NULL ) {
		result = render_buffers[i];
	    }
	} else if( render_buffers[i]->address == render_addr ) {
	    /* right address, wrong size - if it's larger, flush it, otherwise 
	     * nuke it quietly */
	    if( render_buffers[i]->width * render_buffers[i]->height >
		width*height ) {
		pvr2_render_buffer_copy_to_sh4( render_buffers[i] );
	    }
	    render_buffers[i]->address == -1;
	}
    }

    /* Nothing available - make one */
    if( result == NULL ) {
	if( render_buffer_count == MAX_RENDER_BUFFERS ) {
	    /* maximum buffers reached - need to throw one away */
	    uint32_t field1_addr = MMIO_READ( PVR2, DISP_ADDR1 );
	    uint32_t field2_addr = MMIO_READ( PVR2, DISP_ADDR2 );
	    for( i=0; i<render_buffer_count; i++ ) {
		if( render_buffers[i]->address != field1_addr &&
		    render_buffers[i]->address != field2_addr ) {
		    /* Never throw away the current "front buffer(s)" */
		    result = render_buffers[i];
		    pvr2_render_buffer_copy_to_sh4( result );
		    if( result->width != width || result->height != height ) {
			display_driver->destroy_render_buffer(render_buffers[i]);
			result = display_driver->create_render_buffer(width,height);
			render_buffers[i] = result;
		    }
		    break;
		}
	    }
	} else {
	    result = display_driver->create_render_buffer(width,height);
	    if( result != NULL ) { 
		render_buffers[render_buffer_count++] = result;
	    } else {
		ERROR( "Failed to obtain a render buffer!" );
		return NULL;
	    }
	}
    }

    /* Setup the buffer */
    result->rowstride = render_stride;
    result->colour_format = colour_format;
    result->scale = render_scale;
    result->size = width * height * colour_formats[colour_format].bpp;
    result->address = render_addr;
    result->flushed = FALSE;
    return result;
}

/**
 * Invalidate any caching on the supplied address. Specifically, if it falls
 * within any of the render buffers, flush the buffer back to PVR2 ram.
 */
gboolean pvr2_render_buffer_invalidate( sh4addr_t address, gboolean isWrite )
{
    int i;
    address = address & 0x1FFFFFFF;
    for( i=0; i<render_buffer_count; i++ ) {
	uint32_t bufaddr = render_buffers[i]->address;
	uint32_t size = render_buffers[i]->size;
	if( bufaddr != -1 && bufaddr <= address && 
	    (bufaddr + render_buffers[i]->size) > address ) {
	    if( !render_buffers[i]->flushed ) {
		pvr2_render_buffer_copy_to_sh4( render_buffers[i] );
		render_buffers[i]->flushed = TRUE;
	    }
	    if( isWrite ) {
		render_buffers[i]->address = -1; /* Invalid */
	    }
	    return TRUE; /* should never have overlapping buffers */
	}
    }
    return FALSE;
}
