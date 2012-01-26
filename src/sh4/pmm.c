/**
 * $Id$
 * 
 * PMM (performance counter) module
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

#include "sh4/sh4mmio.h"
#include "sh4/sh4core.h"
#include "clock.h"

/*
 * Performance counter list from Paul Mundt's OProfile patch
 * Currently only 0x23 is actually supported, since it doesn't require any
 * actual instrumentation
 * 
 *     0x01            Operand read access
 *     0x02            Operand write access
 *     0x03            UTLB miss
 *     0x04            Operand cache read miss
 *     0x05            Operand cache write miss
 *     0x06            Instruction fetch (w/ cache)
 *     0x07            Instruction TLB miss
 *     0x08            Instruction cache miss
 *     0x09            All operand accesses
 *     0x0a            All instruction accesses
 *     0x0b            OC RAM operand access
 *     0x0d            On-chip I/O space access
 *     0x0e            Operand access (r/w)
 *     0x0f            Operand cache miss (r/w)
 *     0x10            Branch instruction
 *     0x11            Branch taken
 *     0x12            BSR/BSRF/JSR
 *     0x13            Instruction execution
 *     0x14            Instruction execution in parallel
 *     0x15            FPU Instruction execution
 *     0x16            Interrupt
 *     0x17            NMI
 *     0x18            trapa instruction execution
 *     0x19            UBCA match
 *     0x1a            UBCB match
 *     0x21            Instruction cache fill
 *     0x22            Operand cache fill
 *     0x23            Elapsed time
 *     0x24            Pipeline freeze by I-cache miss
 *     0x25            Pipeline freeze by D-cache miss
 *     0x27            Pipeline freeze by branch instruction
 *     0x28            Pipeline freeze by CPU register
 *     0x29            Pipeline freeze by FPU
 */
struct PMM_counter_struct {
    uint64_t count;
    uint32_t mode; /* if running only, otherwise 0 */
    uint32_t runfor;
};

static struct PMM_counter_struct PMM_counter[2] = {{0,0},{0,0}};

void PMM_reset(void)
{
    PMM_counter[0].count = 0;
    PMM_counter[0].mode = 0;
    PMM_counter[0].runfor = 0;
    PMM_counter[1].count = 0;
    PMM_counter[1].mode = 0;
    PMM_counter[1].runfor = 0;
}

void PMM_save_state( FILE *f ) {
    fwrite( &PMM_counter, sizeof(PMM_counter), 1, f );
}

int PMM_load_state( FILE *f ) 
{
    fread( &PMM_counter, sizeof(PMM_counter), 1, f );
    return 0;
}

void PMM_count( int ctr, uint32_t runfor )
{
    uint32_t delta = runfor - PMM_counter[ctr].runfor;

    switch( PMM_counter[ctr].mode ) {
    case 0x23:
        PMM_counter[ctr].count += (delta / (1000/SH4_BASE_RATE)); 
        break;
    default:
        break;
    }
    
    PMM_counter[ctr].runfor = runfor;
}

uint32_t PMM_run_slice( uint32_t nanosecs )
{
    PMM_count( 0, nanosecs );
    PMM_count( 1, nanosecs );
    PMM_counter[0].runfor = 0;
    PMM_counter[1].runfor = 0;
    return nanosecs;
}

void PMM_write_control( int ctr, uint32_t val )
{
    int is_running = ((val & PMCR_RUNNING) == PMCR_RUNNING);

    PMM_count(ctr, sh4r.slice_cycle);
    if( PMM_counter[ctr].mode == 0 && (val & PMCR_PMCLR) != 0 ) {
        PMM_counter[ctr].count = 0;
    }
    if( is_running ) {
        int mode = val & 0x3F;
        if( mode != PMM_counter[ctr].mode ) {
            /* Instrumentation setup goes here */
            PMM_counter[ctr].mode = mode;
        }
    } else if( PMM_counter[ctr].mode != 0 ) {
        /* Instrumentation removal goes here */
        PMM_counter[ctr].mode = 0;
    }
}

MMIO_REGION_READ_FN( PMM, reg )
{   
    switch( reg & 0x1F ) {
    case 0: return 0; /* not a register */
    case PMCTR1H: 
        PMM_count(0, sh4r.slice_cycle);
        return ((uint32_t)(PMM_counter[0].count >> 32)) & 0x0000FFFF;
    case PMCTR1L: 
        PMM_count(0, sh4r.slice_cycle);
        return (uint32_t)PMM_counter[0].count;
    case PMCTR2H: 
        PMM_count(1, sh4r.slice_cycle);
        return ((uint32_t)(PMM_counter[1].count >> 32)) & 0x0000FFFF;
    default: 
        PMM_count(1, sh4r.slice_cycle);
        return (uint32_t)PMM_counter[1].count;
    }
}

MMIO_REGION_WRITE_FN( PMM, reg, val )
{
    /* Read-only */
}

MMIO_REGION_READ_DEFSUBFNS(PMM)
