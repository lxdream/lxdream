/**
 * $Id$
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

#include <assert.h>
#include "dream.h"
#include "eventq.h"
#include "display.h"
#include "mem.h"
#include "asic.h"
#include "clock.h"
#include "pvr2/pvr2.h"
#include "pvr2/pvr2mmio.h"
#include "pvr2/scene.h"
#include "sh4/sh4.h"
#define MMIO_IMPL
#include "pvr2/pvr2mmio.h"

unsigned char *video_base;

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
static render_buffer_t pvr2_frame_buffer_to_render_buffer( frame_buffer_t frame );
uint32_t pvr2_get_sync_status();

void pvr2_display_frame( void );

static int output_colour_formats[] = { COLFMT_BGRA1555, COLFMT_RGB565, COLFMT_BGR888, COLFMT_BGRA8888 };
static int render_colour_formats[8] = {
        COLFMT_BGRA1555, COLFMT_RGB565, COLFMT_BGRA4444, COLFMT_BGRA1555,
        COLFMT_BGR888, COLFMT_BGRA8888, COLFMT_BGRA8888, COLFMT_BGRA4444 };


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
    int32_t palette_changed; /* TRUE if palette has changed since last render */
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
    int32_t interlaced;
} pvr2_state;

static gchar *save_next_render_filename;
static render_buffer_t render_buffers[MAX_RENDER_BUFFERS];
static uint32_t render_buffer_count = 0;
static render_buffer_t displayed_render_buffer = NULL;
static uint32_t displayed_border_colour = 0;

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
static void pvr2_scanline_callback( int eventid ) 
{
    asic_event( eventid );
    pvr2_update_raster_posn(sh4r.slice_cycle);
    if( eventid == EVENT_SCANLINE1 ) {
        pvr2_schedule_scanline_event( eventid, pvr2_state.irq_vpos1, 1, 0 );
    } else {
        pvr2_schedule_scanline_event( eventid, pvr2_state.irq_vpos2, 1, 0 );
    }
}

static void pvr2_gunpos_callback( int eventid ) 
{
    pvr2_update_raster_posn(sh4r.slice_cycle);
    int hpos = pvr2_state.line_remainder * pvr2_state.dot_clock / 1000000;
    MMIO_WRITE( PVR2, GUNPOS, ((pvr2_state.line_count<<16)|(hpos&0x3FF)) );
    asic_event( EVENT_MAPLE_DMA );
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
    register_event_callback( EVENT_GUNPOS, pvr2_gunpos_callback );
    video_base = mem_get_region_by_name( MEM_REGION_VIDEO );
    texcache_init();
    pvr2_reset();
    pvr2_ta_reset();
    save_next_render_filename = NULL;
    for( i=0; i<MAX_RENDER_BUFFERS; i++ ) {
        render_buffers[i] = NULL;
    }
    render_buffer_count = 0;
    displayed_render_buffer = NULL;
    displayed_border_colour = 0;
}

static void pvr2_reset( void )
{
    int i;
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
    if( display_driver ) {
        display_driver->display_blank(0);
        for( i=0; i<render_buffer_count; i++ ) {
            display_driver->destroy_render_buffer(render_buffers[i]);
            render_buffers[i] = NULL;
        }
        render_buffer_count = 0;
    }
}

void pvr2_save_render_buffer( FILE *f, render_buffer_t buffer )
{
    struct frame_buffer fbuf;

    fbuf.width = buffer->width;
    fbuf.height = buffer->height;
    fbuf.rowstride = fbuf.width*3;
    fbuf.colour_format = COLFMT_BGR888;
    fbuf.inverted = buffer->inverted;
    fbuf.data = g_malloc0( buffer->width * buffer->height * 3 );

    display_driver->read_render_buffer( fbuf.data, buffer, fbuf.rowstride, COLFMT_BGR888 );
    write_png_to_stream( f, &fbuf );
    g_free( fbuf.data );

    fwrite( &buffer->rowstride, sizeof(buffer->rowstride), 1, f );
    fwrite( &buffer->colour_format, sizeof(buffer->colour_format), 1, f );
    fwrite( &buffer->address, sizeof(buffer->address), 1, f );
    fwrite( &buffer->scale, sizeof(buffer->scale), 1, f );
    int32_t flushed = (int32_t)buffer->flushed; // Force to 32-bits for save-file consistency
    fwrite( &flushed, sizeof(flushed), 1, f );

}

render_buffer_t pvr2_load_render_buffer( FILE *f )
{
    frame_buffer_t frame = read_png_from_stream( f );
    if( frame == NULL ) {
        return NULL;
    }

    render_buffer_t buffer = pvr2_frame_buffer_to_render_buffer(frame);
    if( buffer != NULL ) {
        int32_t flushed;
        fread( &buffer->rowstride, sizeof(buffer->rowstride), 1, f );
        fread( &buffer->colour_format, sizeof(buffer->colour_format), 1, f );
        fread( &buffer->address, sizeof(buffer->address), 1, f );
        fread( &buffer->scale, sizeof(buffer->scale), 1, f );
        fread( &flushed, sizeof(flushed), 1, f );
        buffer->flushed = (gboolean)flushed;
    } else {
        fseek( f, sizeof(buffer->rowstride)+sizeof(buffer->colour_format)+
                sizeof(buffer->address)+sizeof(buffer->scale)+
                sizeof(int32_t), SEEK_CUR );
    }
    return buffer;
}




void pvr2_save_render_buffers( FILE *f )
{
    int i;
    uint32_t has_frontbuffer;
    fwrite( &render_buffer_count, sizeof(render_buffer_count), 1, f );
    if( displayed_render_buffer != NULL ) {
        has_frontbuffer = 1;
        fwrite( &has_frontbuffer, sizeof(has_frontbuffer), 1, f );
        pvr2_save_render_buffer( f, displayed_render_buffer );
    } else {
        has_frontbuffer = 0;
        fwrite( &has_frontbuffer, sizeof(has_frontbuffer), 1, f );
    }

    for( i=0; i<render_buffer_count; i++ ) {
        if( render_buffers[i] != displayed_render_buffer && render_buffers[i] != NULL ) {
            pvr2_save_render_buffer( f, render_buffers[i] );
        }
    }
}

gboolean pvr2_load_render_buffers( FILE *f )
{
    uint32_t count, has_frontbuffer;
    int i;

    fread( &count, sizeof(count), 1, f );
    if( count > MAX_RENDER_BUFFERS ) {
        return FALSE;
    }
    fread( &has_frontbuffer, sizeof(has_frontbuffer), 1, f );
    for( i=0; i<render_buffer_count; i++ ) {
        display_driver->destroy_render_buffer(render_buffers[i]);
        render_buffers[i] = NULL;
    }
    render_buffer_count = 0;

    if( has_frontbuffer ) {
        displayed_render_buffer = pvr2_load_render_buffer(f);
        display_driver->display_render_buffer( displayed_render_buffer );
        count--;
    }

    for( i=0; i<count; i++ ) {
        pvr2_load_render_buffer( f );
    }
    return TRUE;
}


static void pvr2_save_state( FILE *f )
{
    pvr2_save_render_buffers( f );
    fwrite( &pvr2_state, sizeof(pvr2_state), 1, f );
    pvr2_ta_save_state( f );
    pvr2_yuv_save_state( f );
}

static int pvr2_load_state( FILE *f )
{
    if( !pvr2_load_render_buffers(f) )
        return 1;
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

void pvr2_redraw_display()
{
    if( display_driver != NULL ) {
        if( displayed_render_buffer == NULL ) {
            display_driver->display_blank(displayed_border_colour);
        } else {
            display_driver->display_render_buffer(displayed_render_buffer);
        }
    }
}

gboolean pvr2_save_next_scene( const gchar *filename )
{
    if( save_next_render_filename != NULL ) {
        g_free( save_next_render_filename );
    } 
    save_next_render_filename = g_strdup(filename);
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
        displayed_render_buffer = NULL;
        displayed_border_colour = 0;
        display_driver->display_blank( 0 ); 
    } else if( MMIO_READ( PVR2, DISP_CFG2 ) & 0x08 ) { 
        /* Enabled but blanked - border colour */
        displayed_border_colour = MMIO_READ( PVR2, DISP_BORDER );
        displayed_render_buffer = NULL;
        display_driver->display_blank( displayed_border_colour );
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
        fbuf.inverted = FALSE;
        fbuf.data = video_base + (fbuf.address&0x00FFFFFF);

        render_buffer_t rbuf = pvr2_get_render_buffer( &fbuf );
        if( rbuf == NULL ) {
            rbuf = pvr2_frame_buffer_to_render_buffer( &fbuf );
        }
        displayed_render_buffer = rbuf;
        if( rbuf != NULL ) {
            display_driver->display_render_buffer( rbuf );
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
        if( save_next_render_filename != NULL ) {
            if( pvr2_render_save_scene(save_next_render_filename) == 0 ) {
                INFO( "Saved scene to %s", save_next_render_filename);
            }
            g_free( save_next_render_filename );
            save_next_render_filename = NULL;
        }
        pvr2_scene_read();
        render_buffer_t buffer = pvr2_next_render_buffer();
        if( buffer != NULL ) {
            pvr2_scene_render( buffer );
            pvr2_finish_render_buffer( buffer );
        }
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
        case RENDER_ALPHA_REF:
            MMIO_WRITE( PVR2, reg, val&0x000000FF );
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
        case PVRUNK7:
            MMIO_WRITE( PVR2, reg, val&0x00000001 );
            break;
        case PVRUNK8:
            MMIO_WRITE( PVR2, reg, val&0x0300FFFF );
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

void pvr2_queue_gun_event( int xpos, int ypos )
{
    pvr2_update_raster_posn(sh4r.slice_cycle);
    pvr2_schedule_scanline_event( EVENT_GUNPOS, (ypos >> 1) + pvr2_state.vsync_lines, 0,  
            (1000000 * xpos / pvr2_state.dot_clock) + pvr2_state.hsync_width_ns ); 
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
    pvr2_ta_write( (unsigned char *)&val, sizeof(uint32_t) );
}

render_buffer_t pvr2_create_render_buffer( sh4addr_t addr, int width, int height, GLuint tex_id )
{
    if( display_driver != NULL && display_driver->create_render_buffer != NULL ) {
        render_buffer_t buffer = display_driver->create_render_buffer(width,height,tex_id);
        buffer->address = addr;
        return buffer;
    }
    return NULL;
}

void pvr2_destroy_render_buffer( render_buffer_t buffer )
{
    if( !buffer->flushed )
        pvr2_render_buffer_copy_to_sh4( buffer );
     display_driver->destroy_render_buffer( buffer );
}

void pvr2_finish_render_buffer( render_buffer_t buffer )
{
    display_driver->finish_render( buffer );
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
 * Allocate a render buffer with the requested parameters.
 * The order of preference is:
 *   1. An existing buffer with the same address. (not flushed unless the new
 * size is smaller than the old one).
 *   2. An existing buffer with the same size chosen by LRU order. Old buffer
 *       is flushed to vram.
 *   3. A new buffer if one can be created.
 *   4. The current display buff
 * Note: The current display field(s) will never be overwritten except as a last
 * resort.
 */
render_buffer_t pvr2_alloc_render_buffer( sh4addr_t render_addr, int width, int height )
{
    int i;
    render_buffer_t result = NULL;

    /* Check existing buffers for an available buffer */
    for( i=0; i<render_buffer_count; i++ ) {
        if( render_buffers[i]->width == width && render_buffers[i]->height == height ) {
            /* needs to be the right dimensions */
            if( render_buffers[i]->address == render_addr ) {
                if( displayed_render_buffer == render_buffers[i] ) {
                    /* Same address, but we can't use it because the
                     * display has it. Mark it as unaddressed for later.
                     */
                    render_buffers[i]->address = -1;
                } else {
                    /* perfect */
                    result = render_buffers[i];
                    break;
                }
            } else if( render_buffers[i]->address == -1 && result == NULL && 
                    displayed_render_buffer != render_buffers[i] ) {
                result = render_buffers[i];
            }

        } else if( render_buffers[i]->address == render_addr ) {
            /* right address, wrong size - if it's larger, flush it, otherwise 
             * nuke it quietly */
            if( render_buffers[i]->width * render_buffers[i]->height >
            width*height ) {
                pvr2_render_buffer_copy_to_sh4( render_buffers[i] );
            }
            render_buffers[i]->address = -1;
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
                        render_buffers[i]->address != field2_addr &&
                        render_buffers[i] != displayed_render_buffer ) {
                    /* Never throw away the current "front buffer(s)" */
                    result = render_buffers[i];
                    if( !result->flushed ) {
                        pvr2_render_buffer_copy_to_sh4( result );
                    }
                    if( result->width != width || result->height != height ) {
                        display_driver->destroy_render_buffer(render_buffers[i]);
                        result = display_driver->create_render_buffer(width,height,0);
                        render_buffers[i] = result;
                    }
                    break;
                }
            }
        } else {
            result = display_driver->create_render_buffer(width,height,0);
            if( result != NULL ) { 
                render_buffers[render_buffer_count++] = result;
            }
        }
    }

    if( result != NULL ) {
        result->address = render_addr;
    }
    return result;
}

/**
 * Allocate a render buffer based on the current rendering settings
 */
render_buffer_t pvr2_next_render_buffer()
{
    render_buffer_t result = NULL;
    uint32_t render_addr = MMIO_READ( PVR2, RENDER_ADDR1 );
    uint32_t render_mode = MMIO_READ( PVR2, RENDER_MODE );
    uint32_t render_scale = MMIO_READ( PVR2, RENDER_SCALER );
    uint32_t render_stride = MMIO_READ( PVR2, RENDER_SIZE ) << 3;

    int width = pvr2_scene_buffer_width();
    int height = pvr2_scene_buffer_height();
    int colour_format = render_colour_formats[render_mode&0x07];

    if( render_addr & 0x01000000 ) { /* vram64 */
        render_addr = (render_addr & 0x00FFFFFF) + PVR2_RAM_BASE_INT;
        result = texcache_get_render_buffer( render_addr, colour_format, width, height );
    } else { /* vram32 */
        render_addr = (render_addr & 0x00FFFFFF) + PVR2_RAM_BASE;
        result = pvr2_alloc_render_buffer( render_addr, width, height );
    }

    /* Setup the buffer */
    if( result != NULL ) {
        result->rowstride = render_stride;
        result->colour_format = colour_format;
        result->scale = render_scale;
        result->size = width * height * colour_formats[colour_format].bpp;
        result->flushed = FALSE;
        result->inverted = TRUE; // render buffers are inverted normally
    }
    return result;
}

static render_buffer_t pvr2_frame_buffer_to_render_buffer( frame_buffer_t frame )
{
    render_buffer_t result = pvr2_alloc_render_buffer( frame->address, frame->width, frame->height );
    if( result != NULL ) {
        int bpp = colour_formats[frame->colour_format].bpp;
        result->rowstride = frame->rowstride;
        result->colour_format = frame->colour_format;
        result->scale = 0x400;
        result->size = frame->width * frame->height * bpp;
        result->flushed = TRUE;
        result->inverted = frame->inverted;
        display_driver->load_frame_buffer( frame, result );
    }
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
        if( bufaddr != -1 && bufaddr <= address && 
                (bufaddr + render_buffers[i]->size) > address ) {
            if( !render_buffers[i]->flushed ) {
                pvr2_render_buffer_copy_to_sh4( render_buffers[i] );
            }
            if( isWrite ) {
                render_buffers[i]->address = -1; /* Invalid */
            }
            return TRUE; /* should never have overlapping buffers */
        }
    }
    return FALSE;
}
