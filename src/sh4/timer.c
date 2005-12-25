/**
 * $Id: timer.c,v 1.2 2005-12-25 05:57:00 nkeynes Exp $
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

/********************************* CPG *************************************/

int32_t mmio_region_CPG_read( uint32_t reg )
{
    return MMIO_READ( CPG, reg );
}

void mmio_region_CPG_write( uint32_t reg, uint32_t val )
{
    MMIO_WRITE( CPG, reg, val );
}

/********************************** RTC *************************************/

int32_t mmio_region_RTC_read( uint32_t reg )
{
    return MMIO_READ( RTC, reg );
}

void mmio_region_RTC_write( uint32_t reg, uint32_t val )
{
    MMIO_WRITE( RTC, reg, val );
}

/********************************** TMU *************************************/

int timer_divider[3] = {16,16,16};

int32_t mmio_region_TMU_read( uint32_t reg )
{
    return MMIO_READ( TMU, reg );
}


int get_timer_div( int val )
{
    switch( val & 0x07 ) {
        case 0: return 16; /* assume peripheral clock is IC/4 */
        case 1: return 64;
        case 2: return 256;
        case 3: return 1024;
        case 4: return 4096;
    }
    return 1;
}

void mmio_region_TMU_write( uint32_t reg, uint32_t val )
{
    switch( reg ) {
        case TCR0:
            timer_divider[0] = get_timer_div(val);
            break;
        case TCR1:
            timer_divider[1] = get_timer_div(val);
            break;
        case TCR2:
            timer_divider[2] = get_timer_div(val);
            break;
    }
    MMIO_WRITE( TMU, reg, val );
}

void TMU_run_slice( uint32_t nanosecs )
{
    int tcr = MMIO_READ( TMU, TSTR );
    int cycles = nanosecs / sh4_peripheral_period;
    if( tcr & 0x01 ) {
        int count = cycles / timer_divider[0];
        int *val = MMIO_REG( TMU, TCNT0 );
        if( *val < count ) {
            MMIO_READ( TMU, TCR0 ) |= 0x100;
            /* interrupt goes here */
            count -= *val;
            *val = MMIO_READ( TMU, TCOR0 ) - count;
        } else {
            *val -= count;
        }
    }
    if( tcr & 0x02 ) {
        int count = cycles / timer_divider[1];
        int *val = MMIO_REG( TMU, TCNT1 );
        if( *val < count ) {
            MMIO_READ( TMU, TCR1 ) |= 0x100;
            /* interrupt goes here */
            count -= *val;
            *val = MMIO_READ( TMU, TCOR1 ) - count;
        } else {
            *val -= count;
        }
    }
    if( tcr & 0x04 ) {
        int count = cycles / timer_divider[2];
        int *val = MMIO_REG( TMU, TCNT2 );
        if( *val < count ) {
            MMIO_READ( TMU, TCR2 ) |= 0x100;
            /* interrupt goes here */
            count -= *val;
            *val = MMIO_READ( TMU, TCOR2 ) - count;
        } else {
            *val -= count;
        }
    }
}
