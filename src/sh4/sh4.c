/**
 * $Id$
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
#include <setjmp.h>
#include <assert.h>
#include "lxdream.h"
#include "dreamcast.h"
#include "mem.h"
#include "clock.h"
#include "eventq.h"
#include "syscall.h"
#include "sh4/intc.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#include "sh4/sh4stat.h"
#include "sh4/sh4trans.h"
#include "sh4/xltcache.h"

void sh4_init( void );
void sh4_xlat_init( void );
void sh4_reset( void );
void sh4_start( void );
void sh4_stop( void );
void sh4_save_state( FILE *f );
int sh4_load_state( FILE *f );

uint32_t sh4_run_slice( uint32_t );
uint32_t sh4_xlat_run_slice( uint32_t );

struct dreamcast_module sh4_module = { "SH4", sh4_init, sh4_reset, 
        sh4_start, sh4_run_slice, sh4_stop,
        sh4_save_state, sh4_load_state };

struct sh4_registers sh4r __attribute__((aligned(16)));
struct breakpoint_struct sh4_breakpoints[MAX_BREAKPOINTS];
int sh4_breakpoint_count = 0;

gboolean sh4_starting = FALSE;
static gboolean sh4_use_translator = FALSE;
static jmp_buf sh4_exit_jmp_buf;
static gboolean sh4_running = FALSE;
struct sh4_icache_struct sh4_icache = { NULL, -1, -1, 0 };

void sh4_translate_set_enabled( gboolean use )
{
    // No-op if the translator was not built
#ifdef SH4_TRANSLATOR
    if( use ) {
        sh4_translate_init();
    }
    sh4_use_translator = use;
#endif
}

gboolean sh4_translate_is_enabled()
{
    return sh4_use_translator;
}

void sh4_init(void)
{
    register_io_regions( mmio_list_sh4mmio );
    TMU_init();
    xlat_cache_init();
    sh4_mem_init();
    sh4_reset();
#ifdef ENABLE_SH4STATS
    sh4_stats_reset();
#endif
}

void sh4_start(void)
{
    sh4_starting = TRUE;
}

void sh4_reset(void)
{
    if(	sh4_use_translator ) {
        xlat_flush_cache();
    }

    /* zero everything out, for the sake of having a consistent state. */
    memset( &sh4r, 0, sizeof(sh4r) );

    /* Resume running if we were halted */
    sh4r.sh4_state = SH4_STATE_RUNNING;

    sh4r.pc    = 0xA0000000;
    sh4r.new_pc= 0xA0000002;
    sh4r.vbr   = 0x00000000;
    sh4r.fpscr = 0x00040001;
    sh4r.sr    = 0x700000F0;

    /* Mem reset will do this, but if we want to reset _just_ the SH4... */
    MMIO_WRITE( MMU, EXPEVT, EXC_POWER_RESET );

    /* Peripheral modules */
    CPG_reset();
    INTC_reset();
    MMU_reset();
    PMM_reset();
    TMU_reset();
    SCIF_reset();

#ifdef ENABLE_SH4STATS
    sh4_stats_reset();
#endif
}

void sh4_stop(void)
{
    if(	sh4_use_translator ) {
        /* If we were running with the translator, update new_pc and in_delay_slot */
        sh4r.new_pc = sh4r.pc+2;
        sh4r.in_delay_slot = FALSE;
    }

}

/**
 * Execute a timeslice using translated code only (ie translate/execute loop)
 */
uint32_t sh4_run_slice( uint32_t nanosecs ) 
{
    sh4r.slice_cycle = 0;

    if( sh4r.sh4_state != SH4_STATE_RUNNING ) {
        sh4_sleep_run_slice(nanosecs);
    }

    /* Setup for sudden vm exits */
    switch( setjmp(sh4_exit_jmp_buf) ) {
    case CORE_EXIT_BREAKPOINT:
        sh4_clear_breakpoint( sh4r.pc, BREAK_ONESHOT );
        /* fallthrough */
    case CORE_EXIT_HALT:
        if( sh4r.sh4_state != SH4_STATE_STANDBY ) {
            TMU_run_slice( sh4r.slice_cycle );
            SCIF_run_slice( sh4r.slice_cycle );
            PMM_run_slice( sh4r.slice_cycle );
            dreamcast_stop();
            return sh4r.slice_cycle;
        }
    case CORE_EXIT_SYSRESET:
        dreamcast_reset();
        break;
    case CORE_EXIT_SLEEP:
        sh4_sleep_run_slice(nanosecs);
        break;  
    case CORE_EXIT_FLUSH_ICACHE:
#ifdef SH4_TRANSLATOR
        xlat_flush_cache();
#endif
        break;
    }

    sh4_running = TRUE;
    
    /* Execute the core's real slice */
#ifdef SH4_TRANSLATOR
    if( sh4_use_translator ) {
        sh4_translate_run_slice(nanosecs);
    } else {
        sh4_emulate_run_slice(nanosecs);
    }
#else
    sh4_emulate_run_slice(nanosecs);
#endif
    
    /* And finish off the peripherals afterwards */

    sh4_running = FALSE;
    sh4_starting = FALSE;
    sh4r.slice_cycle = nanosecs;
    if( sh4r.sh4_state != SH4_STATE_STANDBY ) {
        TMU_run_slice( nanosecs );
        SCIF_run_slice( nanosecs );
        PMM_run_slice( sh4r.slice_cycle );
    }
    return nanosecs;   
}

void sh4_core_exit( int exit_code )
{
    if( sh4_running ) {
#ifdef SH4_TRANSLATOR
        if( sh4_use_translator ) {
            sh4_translate_exit_recover();
        }
#endif
        // longjmp back into sh4_run_slice
        sh4_running = FALSE;
        longjmp(sh4_exit_jmp_buf, exit_code);
    }
}

void sh4_flush_icache()
{
#ifdef SH4_TRANSLATOR
    // FIXME: Special case needs to be generalized
    if( sh4_use_translator ) {
        if( sh4_translate_flush_cache() ) {
            longjmp(sh4_exit_jmp_buf, CORE_EXIT_CONTINUE);
        }
    }
#endif
}

void sh4_save_state( FILE *f )
{
    if(	sh4_use_translator ) {
        /* If we were running with the translator, update new_pc and in_delay_slot */
        sh4r.new_pc = sh4r.pc+2;
        sh4r.in_delay_slot = FALSE;
    }

    fwrite( &sh4r, offsetof(struct sh4_registers, xlat_sh4_mode), 1, f );
    MMU_save_state( f );
    CCN_save_state( f );
    PMM_save_state( f );
    INTC_save_state( f );
    TMU_save_state( f );
    SCIF_save_state( f );
}

int sh4_load_state( FILE * f )
{
    if(	sh4_use_translator ) {
        xlat_flush_cache();
    }
    fread( &sh4r, offsetof(struct sh4_registers, xlat_sh4_mode), 1, f );
    sh4r.xlat_sh4_mode = (sh4r.sr & SR_MD) | (sh4r.fpscr & (FPSCR_SZ|FPSCR_PR));
    MMU_load_state( f );
    CCN_load_state( f );
    PMM_load_state( f );
    INTC_load_state( f );
    TMU_load_state( f );
    return SCIF_load_state( f );
}

void sh4_set_breakpoint( uint32_t pc, breakpoint_type_t type )
{
    sh4_breakpoints[sh4_breakpoint_count].address = pc;
    sh4_breakpoints[sh4_breakpoint_count].type = type;
    if( sh4_use_translator ) {
        xlat_invalidate_word( pc );
    }
    sh4_breakpoint_count++;
}

gboolean sh4_clear_breakpoint( uint32_t pc, breakpoint_type_t type )
{
    int i;

    for( i=0; i<sh4_breakpoint_count; i++ ) {
        if( sh4_breakpoints[i].address == pc && 
                sh4_breakpoints[i].type == type ) {
            while( ++i < sh4_breakpoint_count ) {
                sh4_breakpoints[i-1].address = sh4_breakpoints[i].address;
                sh4_breakpoints[i-1].type = sh4_breakpoints[i].type;
            }
            if( sh4_use_translator ) {
                xlat_invalidate_word( pc );
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

void FASTCALL sh4_switch_fr_banks()
{
    int i;
    for( i=0; i<16; i++ ) {
        float tmp = sh4r.fr[0][i];
        sh4r.fr[0][i] = sh4r.fr[1][i];
        sh4r.fr[1][i] = tmp;
    }
}

void FASTCALL sh4_write_sr( uint32_t newval )
{
    int oldbank = (sh4r.sr&SR_MDRB) == SR_MDRB;
    int newbank = (newval&SR_MDRB) == SR_MDRB;
    if( oldbank != newbank )
        sh4_switch_banks();
    sh4r.sr = newval & SR_MASK;
    sh4r.t = (newval&SR_T) ? 1 : 0;
    sh4r.s = (newval&SR_S) ? 1 : 0;
    sh4r.m = (newval&SR_M) ? 1 : 0;
    sh4r.q = (newval&SR_Q) ? 1 : 0;
    sh4r.xlat_sh4_mode = (sh4r.sr & SR_MD) | (sh4r.fpscr & (FPSCR_SZ|FPSCR_PR));
    intc_mask_changed();
}

void FASTCALL sh4_write_fpscr( uint32_t newval )
{
    if( (sh4r.fpscr ^ newval) & FPSCR_FR ) {
        sh4_switch_fr_banks();
    }
    sh4r.fpscr = newval & FPSCR_MASK;
    sh4r.xlat_sh4_mode = (sh4r.sr & SR_MD) | (sh4r.fpscr & (FPSCR_SZ|FPSCR_PR));
}

uint32_t FASTCALL sh4_read_sr( void )
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
        sh4_core_exit(CORE_EXIT_HALT); return FALSE;	\
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
gboolean FASTCALL sh4_raise_exception( int code )
{
    RAISE( code, EXV_EXCEPTION );
}

/**
 * Raise a CPU reset exception with the specified exception code.
 */
gboolean FASTCALL sh4_raise_reset( int code )
{
    // FIXME: reset modules as per "manual reset"
    sh4_reset();
    MMIO_WRITE(MMU,EXPEVT,code);
    sh4r.vbr = 0;
    sh4r.pc = 0xA0000000;
    sh4r.new_pc = sh4r.pc + 2;
    sh4_write_sr( (sh4r.sr|SR_MD|SR_BL|SR_RB|SR_IMASK)
                  &(~SR_FD) );
    return TRUE;
}

gboolean FASTCALL sh4_raise_trap( int trap )
{
    MMIO_WRITE( MMU, TRA, trap<<2 );
    RAISE( EXC_TRAP, EXV_EXCEPTION );
}

gboolean FASTCALL sh4_raise_slot_exception( int normal_code, int slot_code ) {
    if( sh4r.in_delay_slot ) {
        return sh4_raise_exception(slot_code);
    } else {
        return sh4_raise_exception(normal_code);
    }
}

gboolean FASTCALL sh4_raise_tlb_exception( int code )
{
    RAISE( code, EXV_TLBMISS );
}

void FASTCALL sh4_accept_interrupt( void )
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

void FASTCALL signsat48( void )
{
    if( ((int64_t)sh4r.mac) < (int64_t)0xFFFF800000000000LL )
        sh4r.mac = 0xFFFF800000000000LL;
    else if( ((int64_t)sh4r.mac) > (int64_t)0x00007FFFFFFFFFFFLL )
        sh4r.mac = 0x00007FFFFFFFFFFFLL;
}

void FASTCALL sh4_fsca( uint32_t anglei, float *fr )
{
    float angle = (((float)(anglei&0xFFFF))/65536.0) * 2 * M_PI;
    *fr++ = cosf(angle);
    *fr = sinf(angle);
}

/**
 * Enter sleep mode (eg by executing a SLEEP instruction).
 * Sets sh4_state appropriately and ensures any stopping peripheral modules
 * are up to date.
 */
void FASTCALL sh4_sleep(void)
{
    if( MMIO_READ( CPG, STBCR ) & 0x80 ) {
        sh4r.sh4_state = SH4_STATE_STANDBY;
        /* Bring all running peripheral modules up to date, and then halt them. */
        TMU_run_slice( sh4r.slice_cycle );
        SCIF_run_slice( sh4r.slice_cycle );
        PMM_run_slice( sh4r.slice_cycle );
    } else {
        if( MMIO_READ( CPG, STBCR2 ) & 0x80 ) {
            sh4r.sh4_state = SH4_STATE_DEEP_SLEEP;
            /* Halt DMAC but other peripherals still running */

        } else {
            sh4r.sh4_state = SH4_STATE_SLEEP;
        }
    }
    sh4_core_exit( CORE_EXIT_SLEEP );
}

/**
 * Wakeup following sleep mode (IRQ or reset). Sets state back to running,
 * and restarts any peripheral devices that were stopped.
 */
void sh4_wakeup(void)
{
    switch( sh4r.sh4_state ) {
    case SH4_STATE_STANDBY:
        break;
    case SH4_STATE_DEEP_SLEEP:
        break;
    case SH4_STATE_SLEEP:
        break;
    }
    sh4r.sh4_state = SH4_STATE_RUNNING;
}

/**
 * Run a time slice (or portion of a timeslice) while the SH4 is sleeping.
 * Returns when either the SH4 wakes up (interrupt received) or the end of
 * the slice is reached. Updates sh4.slice_cycle with the exit time and
 * returns the same value.
 */
uint32_t sh4_sleep_run_slice( uint32_t nanosecs )
{
    int sleep_state = sh4r.sh4_state;
    assert( sleep_state != SH4_STATE_RUNNING );

    while( sh4r.event_pending < nanosecs ) {
        sh4r.slice_cycle = sh4r.event_pending;
        if( sh4r.event_types & PENDING_EVENT ) {
            event_execute();
        }
        if( sh4r.event_types & PENDING_IRQ ) {
            sh4_wakeup();
            return sh4r.slice_cycle;
        }
    }
    sh4r.slice_cycle = nanosecs;
    return sh4r.slice_cycle;
}


/**
 * Compute the matrix tranform of fv given the matrix xf.
 * Both fv and xf are word-swapped as per the sh4r.fr banks
 */
void FASTCALL sh4_ftrv( float *target )
{
    float fv[4] = { target[1], target[0], target[3], target[2] };
    target[1] = sh4r.fr[1][1] * fv[0] + sh4r.fr[1][5]*fv[1] +
    sh4r.fr[1][9]*fv[2] + sh4r.fr[1][13]*fv[3];
    target[0] = sh4r.fr[1][0] * fv[0] + sh4r.fr[1][4]*fv[1] +
    sh4r.fr[1][8]*fv[2] + sh4r.fr[1][12]*fv[3];
    target[3] = sh4r.fr[1][3] * fv[0] + sh4r.fr[1][7]*fv[1] +
    sh4r.fr[1][11]*fv[2] + sh4r.fr[1][15]*fv[3];
    target[2] = sh4r.fr[1][2] * fv[0] + sh4r.fr[1][6]*fv[1] +
    sh4r.fr[1][10]*fv[2] + sh4r.fr[1][14]*fv[3];
}

gboolean sh4_has_page( sh4vma_t vma )
{
    sh4addr_t addr = mmu_vma_to_phys_disasm(vma);
    return addr != MMU_VMA_ERROR && mem_has_page(addr);
}
