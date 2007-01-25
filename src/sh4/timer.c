/**
 * $Id: timer.c,v 1.7 2007-01-25 10:18:42 nkeynes Exp $
 * 
 * SH4 Timer/Clock peripheral modules (CPG, TMU, RTC), combined together to
 * keep things simple (they intertwine a bit).
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

#include "dream.h"
#include "mem.h"
#include "clock.h"
#include "sh4core.h"
#include "sh4mmio.h"
#include "intc.h"

/********************************* CPG *************************************/
/* This is the base clock from which all other clocks are derived */
uint32_t sh4_input_freq = SH4_BASE_RATE;

uint32_t sh4_cpu_freq = SH4_BASE_RATE;
uint32_t sh4_bus_freq = SH4_BASE_RATE;
uint32_t sh4_peripheral_freq = SH4_BASE_RATE / 2;

uint32_t sh4_cpu_period = 1000 / SH4_BASE_RATE; /* in nanoseconds */
uint32_t sh4_bus_period = 1000 / SH4_BASE_RATE;
uint32_t sh4_peripheral_period = 2000 / SH4_BASE_RATE;

int32_t mmio_region_CPG_read( uint32_t reg )
{
    return MMIO_READ( CPG, reg );
}

/* CPU + bus dividers (note officially only the first 6 values are valid) */
int ifc_divider[8] = { 1, 2, 3, 4, 5, 8, 8, 8 };
/* Peripheral clock dividers (only first 5 are officially valid) */
int pfc_divider[8] = { 2, 3, 4, 6, 8, 8, 8, 8 };

void mmio_region_CPG_write( uint32_t reg, uint32_t val )
{
    uint32_t div;
    switch( reg ) {
    case FRQCR: /* Frequency control */
	div = ifc_divider[(val >> 6) & 0x07];
	sh4_cpu_freq = sh4_input_freq / div;
	sh4_cpu_period = 2000 * div / sh4_input_freq;
	div = ifc_divider[(val >> 3) & 0x07];
	sh4_bus_freq = sh4_input_freq / div;
	sh4_bus_period = 1000 * div / sh4_input_freq;
	div = pfc_divider[val & 0x07];
	sh4_peripheral_freq = sh4_input_freq / div;
	sh4_peripheral_period = 1000 * div / sh4_input_freq;

	/* Update everything that depends on the peripheral frequency */
	SCIF_update_line_speed();
	break;
    case WTCSR: /* Watchdog timer */
	break;
    }
	
    MMIO_WRITE( CPG, reg, val );
}

/**
 * We don't really know what the default reset value is as it's determined
 * by the mode select pins. This is the standard value that the BIOS sets,
 * however, so it works for now.
 */
void CPG_reset( )
{
    mmio_region_CPG_write( FRQCR, 0x0E0A );
}


/********************************** RTC *************************************/

uint32_t rtc_output_period;

int32_t mmio_region_RTC_read( uint32_t reg )
{
    return MMIO_READ( RTC, reg );
}

void mmio_region_RTC_write( uint32_t reg, uint32_t val )
{
    MMIO_WRITE( RTC, reg, val );
}

/********************************** TMU *************************************/

uint32_t TMU_count( int timer, uint32_t nanosecs );


#define TCR_ICPF 0x0200
#define TCR_UNF  0x0100
#define TCR_UNIE 0x0020

#define TCR_IRQ_ACTIVE (TCR_UNF|TCR_UNIE)

struct TMU_timer {
    uint32_t timer_period;
    uint32_t timer_remainder; /* left-over cycles from last count */
    uint32_t timer_run; /* cycles already run from this slice */
};

struct TMU_timer TMU_timers[3];

int32_t mmio_region_TMU_read( uint32_t reg )
{
    switch( reg ) {
    case TCNT0:
	TMU_count( 0, sh4r.slice_cycle );
	TMU_timers[0].timer_run = sh4r.slice_cycle;
	break;
    case TCNT1:
	TMU_count( 1, sh4r.slice_cycle );
	TMU_timers[1].timer_run = sh4r.slice_cycle;
	break;
    case TCNT2:
	TMU_count( 2, sh4r.slice_cycle );
	TMU_timers[2].timer_run = sh4r.slice_cycle;
	break;
    }
    return MMIO_READ( TMU, reg );
}

void TMU_set_timer_control( int timer,  int tcr )
{
    uint32_t period = 1;
    uint32_t oldtcr = MMIO_READ( TMU, TCR0 + (12*timer) );

    if( (oldtcr & TCR_UNF) == 0 ) {
	tcr = tcr & (~TCR_UNF);
    } else {
	if( (oldtcr & TCR_UNIE == 0) && 
	    (tcr & TCR_IRQ_ACTIVE) == TCR_IRQ_ACTIVE ) {
	    intc_raise_interrupt( INT_TMU_TUNI0 + timer );
	} else if( (oldtcr & TCR_UNIE) != 0 && 
		   (tcr & TCR_IRQ_ACTIVE) != TCR_IRQ_ACTIVE ) {
	    intc_clear_interrupt( INT_TMU_TUNI0 + timer );
	}
    }

    switch( tcr & 0x07 ) {
    case 0:
	period = sh4_peripheral_period << 2 ;
	break;
    case 1: 
	period = sh4_peripheral_period << 4;
	break;
    case 2:
	period = sh4_peripheral_period << 6;
	break;
    case 3: 
	period = sh4_peripheral_period << 8;
	break;
    case 4:
	period = sh4_peripheral_period << 10;
	break;
    case 5:
	/* Illegal value. */
	ERROR( "TMU %d period set to illegal value (5)", timer );
	period = sh4_peripheral_period << 12; /* for something to do */
	break;
    case 6:
	period = rtc_output_period;
	break;
    case 7:
	/* External clock... Hrm? */
	period = sh4_peripheral_period; /* I dunno... */
	break;
    }
    TMU_timers[timer].timer_period = period;

    MMIO_WRITE( TMU, TCR0 + (12*timer), tcr );
}

void TMU_start( int timer )
{
    TMU_timers[timer].timer_run = sh4r.slice_cycle;
    TMU_timers[timer].timer_remainder = 0;
}

/**
 * Stop the given timer. Run it up to the current time and leave it there.
 */
void TMU_stop( int timer )
{
    TMU_count( timer, sh4r.slice_cycle );
    TMU_timers[timer].timer_run = sh4r.slice_cycle;
}

/**
 * Count the specified timer for a given number of nanoseconds.
 */
uint32_t TMU_count( int timer, uint32_t nanosecs ) 
{
    nanosecs = nanosecs + TMU_timers[timer].timer_remainder -
	TMU_timers[timer].timer_run;
    TMU_timers[timer].timer_remainder = 
	nanosecs % TMU_timers[timer].timer_period;
    uint32_t count = nanosecs / TMU_timers[timer].timer_period;
    uint32_t value = MMIO_READ( TMU, TCNT0 + 12*timer );
    uint32_t reset = MMIO_READ( TMU, TCOR0 + 12*timer );
    if( count > value ) {
	uint32_t tcr = MMIO_READ( TMU, TCR0 + 12*timer );
	tcr |= TCR_UNF;
	count -= value;
        value = reset - (count % reset);
	MMIO_WRITE( TMU, TCR0 + 12*timer, tcr );
	if( tcr & TCR_UNIE ) 
	    intc_raise_interrupt( INT_TMU_TUNI0 + timer );
    } else {
	value -= count;
    }
    MMIO_WRITE( TMU, TCNT0 + 12*timer, value );
    return value;
}

void mmio_region_TMU_write( uint32_t reg, uint32_t val )
{
    uint32_t oldval;
    int i;
    switch( reg ) {
    case TSTR:
	oldval = MMIO_READ( TMU, TSTR );
	for( i=0; i<3; i++ ) {
	    uint32_t tmp = 1<<i;
	    if( (oldval & tmp) != 0 && (val&tmp) == 0  )
		TMU_stop(i);
	    else if( (oldval&tmp) == 0 && (val&tmp) != 0 )
		TMU_start(i);
	}
	break;
    case TCR0:
	TMU_set_timer_control( 0, val );
	return;
    case TCR1:
	TMU_set_timer_control( 1, val );
	return;
    case TCR2:
	TMU_set_timer_control( 2, val );
	return;
    }
    MMIO_WRITE( TMU, reg, val );
}

void TMU_run_slice( uint32_t nanosecs )
{
    int tcr = MMIO_READ( TMU, TSTR );
    if( tcr & 0x01 ) {
	TMU_count( 0, nanosecs );
	TMU_timers[0].timer_run = 0;
    }
    if( tcr & 0x02 ) {
	TMU_count( 1, nanosecs );
	TMU_timers[1].timer_run = 0;
    }
    if( tcr & 0x04 ) {
	TMU_count( 2, nanosecs );
	TMU_timers[2].timer_run = 0;
    }
}

void TMU_update_clocks()
{
    TMU_set_timer_control( 0, MMIO_READ( TMU, TCR0 ) );
    TMU_set_timer_control( 1, MMIO_READ( TMU, TCR1 ) );
    TMU_set_timer_control( 2, MMIO_READ( TMU, TCR2 ) );
}

void TMU_reset( )
{
    TMU_timers[0].timer_remainder = 0;
    TMU_timers[0].timer_run = 0;
    TMU_timers[1].timer_remainder = 0;
    TMU_timers[1].timer_run = 0;
    TMU_timers[2].timer_remainder = 0;
    TMU_timers[2].timer_run = 0;
    TMU_update_clocks();
}

void TMU_save_state( FILE *f ) {
    fwrite( &TMU_timers, sizeof(TMU_timers), 1, f );
}

int TMU_load_state( FILE *f ) 
{
    fread( &TMU_timers, sizeof(TMU_timers), 1, f );
    return 0;
}
