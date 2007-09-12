/**
 * $Id: sh4.c,v 1.1 2007-09-12 09:20:38 nkeynes Exp $
 * 
 * SH4 parent module for all CPU modes and SH4 peripheral
 * modules.
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

#define MODULE sh4_module
#include <math.h>
#include "dream.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#include "sh4/intc.h"
#include "mem.h"
#include "clock.h"
#include "syscall.h"

void sh4_init( void );
void sh4_reset( void );
void sh4_start( void );
void sh4_stop( void );
void sh4_save_state( FILE *f );
int sh4_load_state( FILE *f );

uint32_t sh4_run_slice( uint32_t );
uint32_t sh4_xlat_run_slice( uint32_t );

struct dreamcast_module sh4_module = { "SH4", sh4_init, sh4_reset, 
				       NULL, sh4_run_slice, sh4_stop,
				       sh4_save_state, sh4_load_state };

struct sh4_registers sh4r;
struct breakpoint_struct sh4_breakpoints[MAX_BREAKPOINTS];
int sh4_breakpoint_count = 0;

void sh4_set_use_xlat( gboolean use )
{
    if( use ) {
	xlat_cache_init();
	sh4_x86_init();
	sh4_module.run_time_slice = sh4_xlat_run_slice;
    } else {
	sh4_module.run_time_slice = sh4_run_slice;
    }
}

void sh4_init(void)
{
    register_io_regions( mmio_list_sh4mmio );
    MMU_init();
    sh4_reset();
}

void sh4_reset(void)
{
    /* zero everything out, for the sake of having a consistent state. */
    memset( &sh4r, 0, sizeof(sh4r) );

    /* Resume running if we were halted */
    sh4r.sh4_state = SH4_STATE_RUNNING;

    sh4r.pc    = 0xA0000000;
    sh4r.new_pc= 0xA0000002;
    sh4r.vbr   = 0x00000000;
    sh4r.fpscr = 0x00040001;
    sh4r.sr    = 0x700000F0;
    sh4r.fr_bank = &sh4r.fr[0][0];

    /* Mem reset will do this, but if we want to reset _just_ the SH4... */
    MMIO_WRITE( MMU, EXPEVT, EXC_POWER_RESET );

    /* Peripheral modules */
    CPG_reset();
    INTC_reset();
    MMU_reset();
    TMU_reset();
    SCIF_reset();
}

void sh4_stop(void)
{

}

void sh4_save_state( FILE *f )
{
    fwrite( &sh4r, sizeof(sh4r), 1, f );
    MMU_save_state( f );
    INTC_save_state( f );
    TMU_save_state( f );
    SCIF_save_state( f );
}

int sh4_load_state( FILE * f )
{
    fread( &sh4r, sizeof(sh4r), 1, f );
    MMU_load_state( f );
    INTC_load_state( f );
    TMU_load_state( f );
    return SCIF_load_state( f );
}


void sh4_set_breakpoint( uint32_t pc, int type )
{
    sh4_breakpoints[sh4_breakpoint_count].address = pc;
    sh4_breakpoints[sh4_breakpoint_count].type = type;
    sh4_breakpoint_count++;
}

gboolean sh4_clear_breakpoint( uint32_t pc, int type )
{
    int i;

    for( i=0; i<sh4_breakpoint_count; i++ ) {
	if( sh4_breakpoints[i].address == pc && 
	    sh4_breakpoints[i].type == type ) {
	    while( ++i < sh4_breakpoint_count ) {
		sh4_breakpoints[i-1].address = sh4_breakpoints[i].address;
		sh4_breakpoints[i-1].type = sh4_breakpoints[i].type;
	    }
	    sh4_breakpoint_count--;
	    return TRUE;
	}
    }
    return FALSE;
}

int sh4_get_breakpoint( uint32_t pc )
{
    int i;
    for( i=0; i<sh4_breakpoint_count; i++ ) {
	if( sh4_breakpoints[i].address == pc )
	    return sh4_breakpoints[i].type;
    }
    return 0;
}

