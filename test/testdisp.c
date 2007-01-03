/**
 * $Id: testdisp.c,v 1.1 2007-01-03 09:05:13 nkeynes Exp $
 *
 * Display (2D) tests. Mainly tests video timing / sync (obviously
 * it can't actually test display output since there's no way of
 * reading the results)
 *
 * These tests use TMU2 to determine absolute time
 * Copyright (c) 2006 Nathan Keynes.
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
#include <stdlib.h>
#include <stdio.h>
#include "lib.h"
#include "asic.h"

#define PVR_BASE 0xA05F8000

#define BORDERCOL   (PVR_BASE+0x040)
#define DISPCFG1 (PVR_BASE+0x044)
#define DISPADDR1 (PVR_BASE+0x050)
#define DISPADDR2 (PVR_BASE+0x054)
#define DISPSIZE (PVR_BASE+0x05C)
#define HPOSEVENT (PVR_BASE+0x0C8)
#define VPOSEVENT (PVR_BASE+0x0CC)
#define DISPCFG2 (PVR_BASE+0x0D0)
#define HBORDER (PVR_BASE+0x0D4)
#define VSYNC (PVR_BASE+0x0D8)
#define VBORDER (PVR_BASE+0x0DC)
#define HSYNC (PVR_BASE+0x0E0)
#define DISPCFG3 (PVR_BASE+0x0E8)
#define HPOS     (PVR_BASE+0x0EC)
#define VPOS     (PVR_BASE+0x0F0)
#define SYNCSTAT (PVR_BASE+0x10C)

#define MAX_FRAME_WAIT 0x10000000

#define WAIT_LINE( a ) if( wait_line(a) != 0 ) { fprintf(stderr, "Timeout at %s:%d:%s() waiting for line %d\n", __FILE__, __LINE__, __func__, a ); return -1; }
#define WAIT_LASTLINE( a ) if( wait_lastline(a) != 0 ) { fprintf(stderr, "Last line check failed at %s:%d:%s() waiting for line %d\n", __FILE__, __LINE__, __func__, a ); return -1; }

void dump_display_regs( FILE *out )
{
    fprintf( out, "%08X DISPCFG1:  %08X\n", DISPCFG1, long_read(DISPCFG1) );
    fprintf( out, "%08X DISPCFG2:  %08X\n", DISPCFG2, long_read(DISPCFG2) );
    fprintf( out, "%08X DISPCFG3:  %08X\n", DISPCFG3, long_read(DISPCFG3) );
    fprintf( out, "%08X DISPSIZE:  %08X\n", DISPSIZE, long_read(DISPSIZE) );
    fprintf( out, "%08X HBORDER:   %08X\n", HBORDER, long_read(HBORDER) );
    fprintf( out, "%08X VBORDER:   %08X\n", VBORDER, long_read(VBORDER) );
    fprintf( out, "%08X HSYNC:     %08X\n", HSYNC, long_read(HSYNC) );
    fprintf( out, "%08X VSYNC:     %08X\n", VSYNC, long_read(VSYNC) );
    fprintf( out, "%08X DISPADDR1: %08X\n", DISPADDR1, long_read(DISPADDR1) );
    fprintf( out, "%08X DISPADDR2: %08X\n", DISPADDR2, long_read(DISPADDR2) );
    fprintf( out, "%08X HPOSEVENT: %08X\n", HPOSEVENT, long_read(HPOSEVENT) );
    fprintf( out, "%08X VPOSEVENT: %08X\n", VPOSEVENT, long_read(VPOSEVENT) );
    fprintf( out, "%08X HPOS:      %08X\n", HPOS, long_read(HPOS) );
    fprintf( out, "%08X VPOS:      %08X\n", VPOS, long_read(VPOS) );
    fprintf( out, "%08X SYNCSTAT:  %08X\n", SYNCSTAT, long_read(SYNCSTAT) );
}

uint32_t pal_settings[] = {
    DISPCFG1, 0x00000001,
    DISPCFG2, 0x00000150,
    DISPCFG3, 0x00160000,
    DISPSIZE, 0x1413BD3F,
    HBORDER, 0x008D034B,
    VBORDER, 0x00120102,
    VSYNC, 0x0270035F,
    HSYNC, 0x07D6A53F,
    HPOS, 0x000000A4,
    VPOS, 0x00120012,
    0, 0 };

void apply_display_settings( uint32_t *regs ) {
    int i;
    for( i=0; regs[i] != 0; i+=2 ) {
	long_write( regs[i], regs[i+1] );
    }
}

/**
 * Wait until the given line is being displayed (ie is set in the syncstat
 * register).
 * @return 0 if the line is reached before timeout, otherwise -1.
 */
int wait_line( int line )
{
    int i;
    for( i=0; i< MAX_FRAME_WAIT; i++ ) {
	uint32_t sync = long_read(SYNCSTAT) & 0x03FF;
	if( sync == line ) {
	    return 0;
	}
    }
    return -1;
}

/**
 * Wait until just after the last line of the frame is being displayed (according
 * to the syncstat register). After this function the current line will be 0.
 * @return 0 if the last line is the given line, otherwise -1.
 */
int wait_lastline( int line )
{
    int lastline = -1, i;
    for( i=0; i< MAX_FRAME_WAIT; i++ ) {
	uint32_t sync = long_read(SYNCSTAT) & 0x03FF;
	if( sync == 0 && lastline != -1 ) {
	    CHECK_IEQUALS( line, lastline );
	    return 0;
	}
	lastline = sync;
    }
    fprintf( stderr, "Timeout waiting for line %d\n", line );
    return -1;
}

int test_ntsc_timing() {
    
    return 0;
}


int test_pal_timing() 
{
    uint32_t line_time, field_time;
    /* Set PAL display mode */
    apply_display_settings( pal_settings );

    asic_clear();

    /* Check basic frame timings: 31.919 us per line, 19.945 ms per field  */
    /* Wait for a line 0 (either frame) */
    WAIT_LINE(0);
    timer_start();
    WAIT_LINE(1);
    line_time = timer_gettime_us();
    WAIT_LASTLINE(624);
    field_time = timer_gettime_us();
    fprintf( stdout, "Line time: %dus, frame time: %dus\n", line_time, field_time );
    // CHECK_IEQUALS( 31, line_time );
    CHECK_IEQUALS( 19949, field_time );
    dump_display_regs( stdout );
    return 0;
}


/********************************* Main **************************************/

typedef int (*test_func_t)();

test_func_t test_fns[] = { test_ntsc_timing, test_pal_timing,
			   NULL };

int main() 
{
    return run_tests( test_fns );
}
