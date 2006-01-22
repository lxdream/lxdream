/**
 * $Id: pvr2.c,v 1.13 2006-01-22 22:38:51 nkeynes Exp $
 *
 * PVR2 (Video) MMIO and supporting functions.
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
void pvr2_next_frame( void );

struct dreamcast_module pvr2_module = { "PVR2", pvr2_init, NULL, NULL, 
					pvr2_run_slice, NULL,
					NULL, NULL };

void pvr2_init( void )
{
    register_io_region( &mmio_region_PVR2 );
    register_io_region( &mmio_region_PVR2PAL );
    register_io_region( &mmio_region_PVR2TA );
    video_base = mem_get_region_by_name( MEM_REGION_VIDEO );
}

uint32_t pvr2_time_counter = 0;
uint32_t pvr2_time_per_frame = 20000000;

uint32_t pvr2_run_slice( uint32_t nanosecs ) 
{
    pvr2_time_counter += nanosecs;
    while( pvr2_time_counter >= pvr2_time_per_frame ) {
	pvr2_next_frame();
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
void pvr2_next_frame( void )
{
    if( bChanged ) {
        int dispsize = MMIO_READ( PVR2, DISPSIZE );
        int dispmode = MMIO_READ( PVR2, DISPMODE );
        int vidcfg = MMIO_READ( PVR2, VIDCFG );
        vid_stride = ((dispsize & DISPSIZE_MODULO) >> 20) - 1;
        vid_lpf = ((dispsize & DISPSIZE_LPF) >> 10) + 1;
        vid_ppl = ((dispsize & DISPSIZE_PPL)) + 1;
        vid_col = (dispmode & DISPMODE_COL);
        frame_start = video_base + MMIO_READ( PVR2, DISPADDR1 );
        interlaced = (vidcfg & VIDCFG_I ? 1 : 0);
        bEnabled = (dispmode & DISPMODE_DE) && (vidcfg & VIDCFG_VO ) ? 1 : 0;
        vid_size = (vid_ppl * vid_lpf) << (interlaced ? 3 : 2);
        vid_hres = vid_ppl;
        vid_vres = vid_lpf;
        if( interlaced ) vid_vres <<= 1;
        switch( vid_col ) {
            case MODE_RGB15:
            case MODE_RGB16: vid_hres <<= 1; break;
            case MODE_RGB24: vid_hres *= 3; break;
            case MODE_RGB32: vid_hres <<= 2; break;
        }
        vid_hres >>= 2;
        video_update_size( vid_hres, vid_vres, vid_col );
        bChanged = 0;
    }
    if( bEnabled ) {
	if( MMIO_READ( PVR2, VIDCFG2 ) & 0x08 ) {
	    /* Blanked */
	    uint32_t colour = MMIO_READ( PVR2, BORDERCOL );
	    video_fill( colour );
	} else {
	    /* Assume bit depths match for now... */
	    memcpy( video_data, frame_start, vid_size );
	}
    } else {
        memset( video_data, 0, vid_size );
    }
    video_update_frame();
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
    case DISPSIZE: bChanged = 1;
    case DISPMODE: bChanged = 1;
    case DISPADDR1: bChanged = 1;
    case DISPADDR2: bChanged = 1;
    case VIDCFG: bChanged = 1;
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


void pvr2_render_scene( void )
{
    /* Actual rendering goes here :) */
    asic_event( EVENT_PVR_RENDER_DONE );
    DEBUG( "Rendered frame %d", video_frame_count );
}

/** Tile Accelerator */

struct tacmd {
    uint32_t command;
    uint32_t param1;
    uint32_t param2;
    uint32_t texture;
    float alpha;
    float red;
    float green;
    float blue;
};

int32_t mmio_region_PVR2TA_read( uint32_t reg )
{
    return 0xFFFFFFFF;
}

char pvr2ta_remainder[8];

void mmio_region_PVR2TA_write( uint32_t reg, uint32_t val )
{
    DEBUG( "Direct write to TA %08X", val );
}

unsigned int pvr2_last_poly_type = 0;

void pvr2ta_write( char *buf, uint32_t length )
{
    int i;
    struct tacmd *cmd_list = (struct tacmd *)buf;
    int count = length >> 5;
    for( i=0; i<count; i++ ){
	unsigned int type = (cmd_list[i].command >> 24) & 0xFF;
	DEBUG( "PVR2 cmd: %08X %08X %08X", cmd_list[i].command, cmd_list[i].param1, cmd_list[i].param2 );
	if( type == 0 ) {
	    /* End of list */
	    switch( pvr2_last_poly_type ) {
	    case 0x80: /* Opaque polys */
		asic_event( EVENT_PVR_OPAQUE_DONE );
		break;
	    case 0x81: /* Opaque poly modifier */
		asic_event( EVENT_PVR_OPAQUEMOD_DONE );
		break;
	    case 0x82: /* Transparent polys */
		asic_event( EVENT_PVR_TRANS_DONE );
		break;
	    case 0x83: /* Transparent poly modifier */
		asic_event( EVENT_PVR_TRANSMOD_DONE );
		break;
	    case 0x84: /* Punchthrough */
		asic_event( EVENT_PVR_PUNCHOUT_DONE );
		break;
	    }
	    pvr2_last_poly_type = 0;
	} else if( type >= 0x80 && type <= 0x84 ) {
	    pvr2_last_poly_type = type;
	}
    }
}

