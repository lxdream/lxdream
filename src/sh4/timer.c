/**
 * $Id$
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

#include <assert.h>
#include "lxdream.h"
#include "mem.h"
#include "clock.h"
#include "eventq.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#include "sh4/intc.h"

/********************************* CPG *************************************/
/* This is the base clock from which all other clocks are derived. 
 * Note: The real clock runs at 33Mhz, which is multiplied by the PLL to
 * run the instruction clock at 200Mhz. For sake of simplicity/precision,
 * we instead use 200Mhz as the base rate and divide everything down instead.
 **/
uint32_t sh4_input_freq = SH4_BASE_RATE;

uint32_t sh4_cpu_multiplier = 2000; /* = 0.5 * frequency */

uint32_t sh4_cpu_freq = SH4_BASE_RATE;
uint32_t sh4_bus_freq = SH4_BASE_RATE / 2;
uint32_t sh4_peripheral_freq = SH4_BASE_RATE / 4;

uint32_t sh4_cpu_period = 1000 / SH4_BASE_RATE; /* in nanoseconds */
uint32_t sh4_bus_period = 2* 1000 / SH4_BASE_RATE;
uint32_t sh4_peripheral_period = 4 * 1000 / SH4_BASE_RATE;

MMIO_REGION_READ_FN( CPG, reg )
{
    return MMIO_READ( CPG, reg&0xFFF );
}
MMIO_REGION_READ_DEFSUBFNS(CPG)

/* CPU + bus dividers (note officially only the first 6 values are valid) */
int ifc_divider[8] = { 1, 2, 3, 4, 5, 8, 8, 8 };
/* Peripheral clock dividers (only first 5 are officially valid) */
int pfc_divider[8] = { 2, 3, 4, 6, 8, 8, 8, 8 };

MMIO_REGION_WRITE_FN( CPG, reg, val )
{
    uint32_t div;
    uint32_t primary_clock = sh4_input_freq;
    reg &= 0xFFF;
    switch( reg ) {
    case FRQCR: /* Frequency control */
        if( (val & FRQCR_PLL1EN) == 0 )
            primary_clock /= 6;
        div = ifc_divider[(val >> 6) & 0x07];
        sh4_cpu_freq = primary_clock / div;
        sh4_cpu_period = sh4_cpu_multiplier * div / sh4_input_freq;
        div = ifc_divider[(val >> 3) & 0x07];
        sh4_bus_freq = primary_clock / div;
        sh4_bus_period = 1000 * div / sh4_input_freq;
        div = pfc_divider[val & 0x07];
        sh4_peripheral_freq = primary_clock / div;
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

MMIO_REGION_READ_FN( RTC, reg )
{
    return MMIO_READ( RTC, reg &0xFFF );
}
MMIO_REGION_READ_DEFSUBFNS(RTC)

MMIO_REGION_WRITE_FN( RTC, reg, val )
{
    MMIO_WRITE( RTC, reg &0xFFF, val );
}

/********************************** TMU *************************************/

#define TCR_ICPF 0x0200
#define TCR_UNF  0x0100
#define TCR_UNIE 0x0020

#define TCR_IRQ_ACTIVE (TCR_UNF|TCR_UNIE)

#define TMU_IS_RUNNING(timer)  (MMIO_READ(TMU,TSTR) & (1<<timer))

struct TMU_timer {
    uint32_t timer_period;
    uint32_t timer_remainder; /* left-over cycles from last count */
    uint32_t timer_run; /* cycles already run from this slice */
};

static struct TMU_timer TMU_timers[3];

uint32_t TMU_count( int timer, uint32_t nanosecs );
void TMU_schedule_timer( int timer );

void TMU_event_callback( int eventid )
{
    TMU_count( eventid - EVENT_TMU0, sh4r.slice_cycle );
    assert( MMIO_READ( TMU, TCR0 + (eventid - EVENT_TMU0)*12 ) & 0x100 );
}

void TMU_init(void)
{
    register_event_callback( EVENT_TMU0, TMU_event_callback );
    register_event_callback( EVENT_TMU1, TMU_event_callback );
    register_event_callback( EVENT_TMU2, TMU_event_callback );
}    

void TMU_dump(unsigned timer)
{
    fprintf(stderr, "Timer %d: %s %08x/%08x %dns run: %08X - %08X\n",
            timer, TMU_IS_RUNNING(timer) ? "running" : "stopped",
            MMIO_READ(TMU, TCNT0 + (timer*12)), MMIO_READ(TMU, TCOR0 + (timer*12)),
            TMU_timers[timer].timer_period,
            TMU_timers[timer].timer_run,
            TMU_timers[timer].timer_remainder );
}


void TMU_set_timer_control( int timer,  int tcr )
{
    uint32_t period = 1;
    uint32_t oldtcr = MMIO_READ( TMU, TCR0 + (12*timer) );

    if( (oldtcr & TCR_UNF) == 0 ) {
        tcr = tcr & (~TCR_UNF);
    } else {
        if( ((oldtcr & TCR_UNIE) == 0) && 
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

    if( period != TMU_timers[timer].timer_period ) {
        if( TMU_IS_RUNNING(timer) ) {
            /* If we're changing clock speed while counting, sync up and reschedule */
            TMU_count(timer, sh4r.slice_cycle);
            TMU_timers[timer].timer_period = period;
            TMU_schedule_timer(timer);
        } else {
            TMU_timers[timer].timer_period = period;
        }
    }

    MMIO_WRITE( TMU, TCR0 + (12*timer), tcr );
}

void TMU_schedule_timer( int timer )
{
    uint64_t duration = ((uint64_t)((uint32_t)(MMIO_READ( TMU, TCNT0 + 12*timer )))+1) * 
    (uint64_t)TMU_timers[timer].timer_period - TMU_timers[timer].timer_remainder;
    event_schedule_long( EVENT_TMU0+timer, (uint32_t)(duration / 1000000000), 
                         (uint32_t)(duration % 1000000000) );
//    if( timer == 2 ) {
//        WARN( "Schedule timer %d: %lldns", timer, duration );
//        TMU_dump(timer);
//    }
}

void TMU_start( int timer )
{
    TMU_timers[timer].timer_run = sh4r.slice_cycle;
    TMU_timers[timer].timer_remainder = 0;
    TMU_schedule_timer( timer );
}

/**
 * Stop the given timer. Run it up to the current time and leave it there.
 */
void TMU_stop( int timer )
{
    TMU_count( timer, sh4r.slice_cycle );
    event_cancel( EVENT_TMU0+timer );
}

/**
 * Count the specified timer for a given number of nanoseconds.
 */
uint32_t TMU_count( int timer, uint32_t nanosecs ) 
{
    uint32_t run_ns = nanosecs + TMU_timers[timer].timer_remainder -
    TMU_timers[timer].timer_run;
    TMU_timers[timer].timer_remainder = 
        run_ns % TMU_timers[timer].timer_period;
    TMU_timers[timer].timer_run = nanosecs;
    uint32_t count = run_ns / TMU_timers[timer].timer_period;
    uint32_t value = MMIO_READ( TMU, TCNT0 + 12*timer );
    uint32_t reset = MMIO_READ( TMU, TCOR0 + 12*timer );
//    if( timer == 2 )
//        WARN( "Counting timer %d: %d ns, %d ticks", timer, run_ns, count );
    if( count > value ) {
        uint32_t tcr = MMIO_READ( TMU, TCR0 + 12*timer );
        tcr |= TCR_UNF;
        count -= value;
        value = reset - (count % reset) + 1;
        MMIO_WRITE( TMU, TCR0 + 12*timer, tcr );
        if( tcr & TCR_UNIE ) 
            intc_raise_interrupt( INT_TMU_TUNI0 + timer );
        MMIO_WRITE( TMU, TCNT0 + 12*timer, value );
//        if( timer == 2 )
//            WARN( "Underflowed timer %d", timer );
        TMU_schedule_timer(timer);
    } else {
        value -= count;
        MMIO_WRITE( TMU, TCNT0 + 12*timer, value );
    }
    return value;
}

MMIO_REGION_READ_FN( TMU, reg )
{
    reg &= 0xFFF;
    switch( reg ) {
    case TCNT0:
        if( TMU_IS_RUNNING(0) )
            TMU_count( 0, sh4r.slice_cycle );
        break;
    case TCNT1:
        if( TMU_IS_RUNNING(1) )
            TMU_count( 1, sh4r.slice_cycle );
        break;
    case TCNT2:
        if( TMU_IS_RUNNING(2) )
            TMU_count( 2, sh4r.slice_cycle );
        break;
    }
    return MMIO_READ( TMU, reg );
}
MMIO_REGION_READ_DEFSUBFNS(TMU)


MMIO_REGION_WRITE_FN( TMU, reg, val )
{
    uint32_t oldval;
    int i;
    reg &= 0xFFF;
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
    case TCNT0:
        MMIO_WRITE( TMU, reg, val );
        if( TMU_IS_RUNNING(0) ) { // reschedule
            TMU_timers[0].timer_run = sh4r.slice_cycle;
            TMU_schedule_timer( 0 );
        }
        return;
    case TCNT1:
        MMIO_WRITE( TMU, reg, val );
        if( TMU_IS_RUNNING(1) ) { // reschedule
            TMU_timers[1].timer_run = sh4r.slice_cycle;
            TMU_schedule_timer( 1 );
        }
        return;
    case TCNT2:
        MMIO_WRITE( TMU, reg, val );
        if( TMU_IS_RUNNING(2) ) { // reschedule
            TMU_timers[2].timer_run = sh4r.slice_cycle;
            TMU_schedule_timer( 2 );
        }
        return;
    }
    MMIO_WRITE( TMU, reg, val );
}

void TMU_count_all( uint32_t nanosecs )
{
    int tcr = MMIO_READ( TMU, TSTR );
    if( tcr & 0x01 ) {
        TMU_count( 0, nanosecs );
    }
    if( tcr & 0x02 ) {
        TMU_count( 1, nanosecs );
    }
    if( tcr & 0x04 ) {
        TMU_count( 2, nanosecs );
    }
}

void TMU_run_slice( uint32_t nanosecs )
{
    TMU_count_all( nanosecs );
    TMU_timers[0].timer_run = 0;
    TMU_timers[1].timer_run = 0;
    TMU_timers[2].timer_run = 0;
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
