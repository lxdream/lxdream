/**
 * $Id: testdisp.c,v 1.2 2007-01-06 04:08:11 nkeynes Exp $
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
#define DISPTOTAL (PVR_BASE+0x0D8)
#define VBORDER (PVR_BASE+0x0DC)
#define SYNCTIME (PVR_BASE+0x0E0)
#define DISPCFG3 (PVR_BASE+0x0E8)
#define HPOS     (PVR_BASE+0x0EC)
#define VPOS     (PVR_BASE+0x0F0)
#define SYNCSTAT (PVR_BASE+0x10C)

#define MAX_FRAME_WAIT 0x50000

#define EVENT_RETRACE 5

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
    fprintf( out, "%08X SYNCTIME:  %08X\n", SYNCTIME, long_read(SYNCTIME) );
    fprintf( out, "%08X DISPTOTAL: %08X\n", DISPTOTAL, long_read(DISPTOTAL) );
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
    DISPTOTAL, 0x0270035F,
    SYNCTIME, 0x07D6A53F,
    HPOS, 0x000000A4,
    VPOS, 0x00120012,
    VPOSEVENT, 0x00150136,
    0, 0 };

uint32_t ntsc_settings[] = {
    DISPCFG1, 0x00000001,
    DISPCFG2, 0x00000150,
    DISPCFG3, 0x00160000,
    DISPSIZE, 0x1413BD3F,
    HBORDER, 0x007e0345,
    VBORDER, 0x00120102,
    DISPTOTAL, 0x020C0359,
    SYNCTIME, 0x07d6c63f,
    HPOS, 0x000000A4,
    VPOS, 0x00120012,
    VPOSEVENT, 0x001501FE,
    0, 0 };
    

struct timing {
    uint32_t interlaced;
    uint32_t total_lines;
    uint32_t vsync_lines;
    uint32_t line_time_us;
    uint32_t field_time_us;
    uint32_t hsync_width_us;
    uint32_t front_porch_us;
    uint32_t back_porch_us;
};

struct timing ntsc_timing = { 1, 525, 6, 31, 16641, 4, 12, 4 };
struct timing pal_timing = { 1, 625, 5, 31, 19949, 4, 12, 4 };

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
    int lastline = 0, i;
    for( i=0; i< MAX_FRAME_WAIT; i++ ) {
	uint32_t sync = long_read(SYNCSTAT) & 0x03FF;
	if( sync == 0 && lastline != 0 ) {
	    CHECK_IEQUALS( line, lastline );
	    return 0;
	}
	lastline = sync;
    }
    fprintf( stderr, "Timeout waiting for line %d\n", line );
    return -1;
}

int check_events_interlaced( ) 
{
    uint32_t status1, status2, status3;
    int i;
    for( i=0; i< MAX_FRAME_WAIT; i++ ) {
	status1 = long_read(SYNCSTAT) & 0x07FF;
	if( status1 == 0x04FF ) {
	    break;
	}
    }    
    asic_clear();
    asic_wait(EVENT_RETRACE);
    status1 = long_read(SYNCSTAT);
    asic_clear();
    asic_wait(EVENT_SCANLINE2);
    status2 = long_read(SYNCSTAT);
    asic_clear();
    asic_wait(EVENT_SCANLINE1);
    status3 = long_read(SYNCSTAT);
    CHECK_IEQUALS( 0x0000, status1 );
    CHECK_IEQUALS( 0x202A, status2 );
    CHECK_IEQUALS( 0x226C, status3 );

    for( i=0; i< MAX_FRAME_WAIT; i++ ) {
	status1 = long_read(SYNCSTAT) & 0x07FF;
	if( status1 == 0x00FF ) {
	    break;
	}
    }    
    asic_clear();
    asic_wait(EVENT_RETRACE);
    status1 = long_read(SYNCSTAT);
    asic_clear();
    asic_wait(EVENT_SCANLINE2);
    status2 = long_read(SYNCSTAT);
    asic_clear();
    asic_wait(EVENT_SCANLINE1);
    status3 = long_read(SYNCSTAT);
    fprintf( stderr, "%08X, %08X, %08X\n", status1, status2, status3 );
    CHECK_IEQUALS( 0x1400, status1 );
    CHECK_IEQUALS( 0x242B, status2 );
    CHECK_IEQUALS( 0x266D, status3 );
    
    return 0;
}

int check_timing( struct timing *t ) {
    uint32_t line_time, field_time;
    uint32_t stat;
    uint32_t last_line = t->total_lines - 1;
    int i;

    WAIT_LINE( t->total_lines - 1 );
    asic_clear();
    for( i=0; i< MAX_FRAME_WAIT; i++ ) {
	stat = long_read(SYNCSTAT) & 0x07FF;
	if( stat == 0 ) {
	    break;
	} else if( (stat & 0x03FF) != last_line ) {
	    asic_clear();
	    last_line = stat & 0x03FF;
	}
    }
    if( stat != 0 ) {
	fprintf( stderr, "Timeout waiting for line 0 field 0\n" );
	return -1;
    }
    timer_start();
    asic_clear();
    if( asic_check( EVENT_RETRACE ) != 0 ) {
	fprintf( stderr, "Failed to clear retrace event ?\n" );
	return -1;
    }
    CHECK_IEQUALS( stat, 0 ); /* VSYNC, HSYNC, no display */
    WAIT_LINE(1);
    line_time = timer_gettime_us();
    WAIT_LASTLINE(t->total_lines-1);
    field_time = timer_gettime_us();

    if( line_time != t->line_time_us ||
	field_time != t->field_time_us ) {
	fprintf( stderr, "Assertion failed: Expected Timing %d,%d but was %d,%d\n",
		 t->line_time_us, t->field_time_us, line_time, field_time );
	return -1;
    }
    return 0;
}

int test_ntsc_timing() {
    apply_display_settings( ntsc_settings );
    //    check_events_interlaced();
    asic_clear();
    uint32_t result = check_timing( &ntsc_timing );
    dump_display_regs( stdout );
    return result;
}


int test_pal_timing() 
{
    uint32_t line_time, field_time;
    /* Set PAL display mode */
    apply_display_settings( pal_settings );

    check_events_interlaced();
    asic_clear();
    uint32_t result = check_timing( &pal_timing );
    dump_display_regs( stdout );
    return result;
}


/********************************* Main **************************************/

typedef int (*test_func_t)();

test_func_t test_fns[] = { test_ntsc_timing, test_pal_timing,
			   NULL };

int main() 
{
    return run_tests( test_fns );
}
