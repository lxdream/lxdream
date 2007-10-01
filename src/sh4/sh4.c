/**
 * $Id: sh4.c,v 1.3 2007-10-01 11:51:25 nkeynes Exp $
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

#define EXV_EXCEPTION    0x100  /* General exception vector */
#define EXV_TLBMISS      0x400  /* TLB-miss exception vector */
#define EXV_INTERRUPT    0x600  /* External interrupt vector */

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
    sh4_stats_reset();
}

void sh4_stop(void)
{

}

void sh4_save_state( FILE *f )
{
    if(	sh4_module.run_time_slice == sh4_xlat_run_slice ) {
	/* If we were running with the translator, update new_pc and in_delay_slot */
	sh4r.new_pc = sh4r.pc+2;
	sh4r.in_delay_slot = FALSE;
    }

    fwrite( &sh4r, sizeof(sh4r), 1, f );
    MMU_save_state( f );
    INTC_save_state( f );
    TMU_save_state( f );
    SCIF_save_state( f );
}

int sh4_load_state( FILE * f )
{
    fread( &sh4r, sizeof(sh4r), 1, f );
    sh4r.fr_bank = &sh4r.fr[(sh4r.fpscr&FPSCR_FR)>>21][0]; // Fixup internal FR pointer
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

void sh4_set_pc( int pc )
{
    sh4r.pc = pc;
    sh4r.new_pc = pc+2;
}


/******************************* Support methods ***************************/

static void sh4_switch_banks( )
{
    uint32_t tmp[8];

    memcpy( tmp, sh4r.r, sizeof(uint32_t)*8 );
    memcpy( sh4r.r, sh4r.r_bank, sizeof(uint32_t)*8 );
    memcpy( sh4r.r_bank, tmp, sizeof(uint32_t)*8 );
}

void sh4_write_sr( uint32_t newval )
{
    if( (newval ^ sh4r.sr) & SR_RB )
        sh4_switch_banks();
    sh4r.sr = newval;
    sh4r.t = (newval&SR_T) ? 1 : 0;
    sh4r.s = (newval&SR_S) ? 1 : 0;
    sh4r.m = (newval&SR_M) ? 1 : 0;
    sh4r.q = (newval&SR_Q) ? 1 : 0;
    intc_mask_changed();
}

uint32_t sh4_read_sr( void )
{
    /* synchronize sh4r.sr with the various bitflags */
    sh4r.sr &= SR_MQSTMASK;
    if( sh4r.t ) sh4r.sr |= SR_T;
    if( sh4r.s ) sh4r.sr |= SR_S;
    if( sh4r.m ) sh4r.sr |= SR_M;
    if( sh4r.q ) sh4r.sr |= SR_Q;
    return sh4r.sr;
}



#define RAISE( x, v ) do{			\
    if( sh4r.vbr == 0 ) { \
        ERROR( "%08X: VBR not initialized while raising exception %03X, halting", sh4r.pc, x ); \
        dreamcast_stop(); return FALSE;	\
    } else { \
        sh4r.spc = sh4r.pc;	\
        sh4r.ssr = sh4_read_sr(); \
        sh4r.sgr = sh4r.r[15]; \
        MMIO_WRITE(MMU,EXPEVT,x); \
        sh4r.pc = sh4r.vbr + v; \
        sh4r.new_pc = sh4r.pc + 2; \
        sh4_write_sr( sh4r.ssr |SR_MD|SR_BL|SR_RB ); \
	if( sh4r.in_delay_slot ) { \
	    sh4r.in_delay_slot = 0; \
	    sh4r.spc -= 2; \
	} \
    } \
    return TRUE; } while(0)

/**
 * Raise a general CPU exception for the specified exception code.
 * (NOT for TRAPA or TLB exceptions)
 */
gboolean sh4_raise_exception( int code )
{
    RAISE( code, EXV_EXCEPTION );
}

gboolean sh4_raise_trap( int trap )
{
    MMIO_WRITE( MMU, TRA, trap<<2 );
    return sh4_raise_exception( EXC_TRAP );
}

gboolean sh4_raise_slot_exception( int normal_code, int slot_code ) {
    if( sh4r.in_delay_slot ) {
	return sh4_raise_exception(slot_code);
    } else {
	return sh4_raise_exception(normal_code);
    }
}

gboolean sh4_raise_tlb_exception( int code )
{
    RAISE( code, EXV_TLBMISS );
}

void sh4_accept_interrupt( void )
{
    uint32_t code = intc_accept_interrupt();
    sh4r.ssr = sh4_read_sr();
    sh4r.spc = sh4r.pc;
    sh4r.sgr = sh4r.r[15];
    sh4_write_sr( sh4r.ssr|SR_BL|SR_MD|SR_RB );
    MMIO_WRITE( MMU, INTEVT, code );
    sh4r.pc = sh4r.vbr + 0x600;
    sh4r.new_pc = sh4r.pc + 2;
    //    WARN( "Accepting interrupt %03X, from %08X => %08X", code, sh4r.spc, sh4r.pc );
}

void signsat48( void )
{
    if( ((int64_t)sh4r.mac) < (int64_t)0xFFFF800000000000LL )
	sh4r.mac = 0xFFFF800000000000LL;
    else if( ((int64_t)sh4r.mac) > (int64_t)0x00007FFFFFFFFFFFLL )
	sh4r.mac = 0x00007FFFFFFFFFFFLL;
}

void sh4_fsca( uint32_t anglei, float *fr )
{
    float angle = (((float)(anglei&0xFFFF))/65536.0) * 2 * M_PI;
    *fr++ = cosf(angle);
    *fr = sinf(angle);
}

void sh4_sleep(void)
{
    if( MMIO_READ( CPG, STBCR ) & 0x80 ) {
	sh4r.sh4_state = SH4_STATE_STANDBY;
    } else {
	sh4r.sh4_state = SH4_STATE_SLEEP;
    }
}

/**
 * Compute the matrix tranform of fv given the matrix xf.
 * Both fv and xf are word-swapped as per the sh4r.fr banks
 */
void sh4_ftrv( float *target, float *xf )
{
    float fv[4] = { target[1], target[0], target[3], target[2] };
    target[1] = xf[1] * fv[0] + xf[5]*fv[1] +
	xf[9]*fv[2] + xf[13]*fv[3];
    target[0] = xf[0] * fv[0] + xf[4]*fv[1] +
	xf[8]*fv[2] + xf[12]*fv[3];
    target[3] = xf[3] * fv[0] + xf[7]*fv[1] +
	xf[11]*fv[2] + xf[15]*fv[3];
    target[2] = xf[2] * fv[0] + xf[6]*fv[1] +
	xf[10]*fv[2] + xf[14]*fv[3];
}

