/**
 * $Id: pvr2.c,v 1.16 2006-02-15 13:11:46 nkeynes Exp $
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
#include "pvr2.h"
#include "sh4/sh4core.h"
#define MMIO_IMPL
#include "pvr2.h"

char *video_base;

void pvr2_init( void );
uint32_t pvr2_run_slice( uint32_t );
void pvr2_display_frame( void );

video_driver_t video_driver = NULL;
struct video_buffer video_buffer[2];
int video_buffer_idx = 0;

struct dreamcast_module pvr2_module = { "PVR2", pvr2_init, NULL, NULL, 
					pvr2_run_slice, NULL,
					NULL, NULL };

void pvr2_init( void )
{
    register_io_region( &mmio_region_PVR2 );
    register_io_region( &mmio_region_PVR2PAL );
    register_io_region( &mmio_region_PVR2TA );
    video_base = mem_get_region_by_name( MEM_REGION_VIDEO );
    video_driver = &video_gtk_driver;
    video_driver->set_output_format( 640, 480, COLFMT_RGB32 );
}

uint32_t pvr2_time_counter = 0;
uint32_t pvr2_frame_counter = 0;
uint32_t pvr2_time_per_frame = 20000000;

uint32_t pvr2_run_slice( uint32_t nanosecs ) 
{
    pvr2_time_counter += nanosecs;
    while( pvr2_time_counter >= pvr2_time_per_frame ) {
	pvr2_display_frame();
	pvr2_time_counter -= pvr2_time_per_frame;
    }
    return nanosecs;
}

uint32_t vid_stride, vid_lpf, vid_ppl, vid_hres, vid_vres, vid_col;
int interlaced, bChanged = 1, bEnabled = 0, vid_size = 0;
char *frame_start; /* current video start address (in real memory) */

/*
 * Display the next frame, copying the current contents of video ram to
 * the window. If the video configuration has changed, first recompute the
 * new frame size/depth.
 */
void pvr2_display_frame( void )
{
    int dispsize = MMIO_READ( PVR2, DISPSIZE );
    int dispmode = MMIO_READ( PVR2, DISPMODE );
    int vidcfg = MMIO_READ( PVR2, VIDCFG );
    int vid_stride = ((dispsize & DISPSIZE_MODULO) >> 20) - 1;
    int vid_lpf = ((dispsize & DISPSIZE_LPF) >> 10) + 1;
    int vid_ppl = ((dispsize & DISPSIZE_PPL)) + 1;
    gboolean bEnabled = (dispmode & DISPMODE_DE) && (vidcfg & VIDCFG_VO ) ? TRUE : FALSE;
    gboolean interlaced = (vidcfg & VIDCFG_I ? TRUE : FALSE);
    if( bEnabled ) {
	video_buffer_t buffer = &video_buffer[video_buffer_idx];
	video_buffer_idx = !video_buffer_idx;
	video_buffer_t last = &video_buffer[video_buffer_idx];
	buffer->colour_format = (dispmode & DISPMODE_COL);
	buffer->rowstride = (vid_ppl + vid_stride) << 2;
	buffer->data = frame_start = video_base + MMIO_READ( PVR2, DISPADDR1 );
	buffer->vres = vid_lpf;
	if( interlaced ) buffer->vres <<= 1;
	switch( buffer->colour_format ) {
	case COLFMT_RGB15:
	case COLFMT_RGB16: buffer->hres = vid_ppl << 1; break;
	case COLFMT_RGB24: buffer->hres = (vid_ppl << 2) / 3; break;
	case COLFMT_RGB32: buffer->hres = vid_ppl; break;
	}
	
	if( video_driver != NULL ) {
	    if( buffer->hres != last->hres ||
		buffer->vres != last->vres ||
		buffer->colour_format != last->colour_format) {
		video_driver->set_output_format( buffer->hres, buffer->vres,
						 buffer->colour_format );
	    }
	    if( MMIO_READ( PVR2, VIDCFG2 ) & 0x08 ) { /* Blanked */
		uint32_t colour = MMIO_READ( PVR2, DISPBORDER );
		video_driver->display_blank_frame( colour );
	    } else {
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
   
    switch(reg) {
    case TAINIT:
	if( val & 0x80000000 )
	    pvr2_ta_init();
	break;
    case RENDSTART:
	if( val == 0xFFFFFFFF )
	    pvr2_render_scene();
	break;
    }
    MMIO_WRITE( PVR2, reg, val );
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



