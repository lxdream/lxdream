/**
 * $Id: sh4core.c,v 1.40 2007-01-23 08:17:06 nkeynes Exp $
 * 
 * SH4 emulation core, and parent module for all the SH4 peripheral
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

#define SH4_CALLTRACE 1

#define MAX_INT 0x7FFFFFFF
#define MIN_INT 0x80000000
#define MAX_INTF 2147483647.0
#define MIN_INTF -2147483648.0

/* CPU-generated exception code/vector pairs */
#define EXC_POWER_RESET  0x000 /* vector special */
#define EXC_MANUAL_RESET 0x020
#define EXC_READ_ADDR_ERR 0x0E0
#define EXC_WRITE_ADDR_ERR 0x100
#define EXC_SLOT_ILLEGAL 0x1A0
#define EXC_ILLEGAL      0x180
#define EXC_TRAP         0x160
#define EXC_FPDISABLE    0x800
#define EXC_SLOT_FPDISABLE 0x820

#define EXV_EXCEPTION    0x100  /* General exception vector */
#define EXV_TLBMISS      0x400  /* TLB-miss exception vector */
#define EXV_INTERRUPT    0x600  /* External interrupt vector */

/********************** SH4 Module Definition ****************************/

void sh4_init( void );
void sh4_reset( void );
uint32_t sh4_run_slice( uint32_t );
void sh4_start( void );
void sh4_stop( void );
void sh4_save_state( FILE *f );
int sh4_load_state( FILE *f );
static void sh4_accept_interrupt( void );

struct dreamcast_module sh4_module = { "SH4", sh4_init, sh4_reset, 
				       NULL, sh4_run_slice, sh4_stop,
				       sh4_save_state, sh4_load_state };

struct sh4_registers sh4r;

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

    /* Mem reset will do this, but if we want to reset _just_ the SH4... */
    MMIO_WRITE( MMU, EXPEVT, EXC_POWER_RESET );

    /* Peripheral modules */
    CPG_reset();
    INTC_reset();
    MMU_reset();
    TMU_reset();
    SCIF_reset();
}

static struct breakpoint_struct sh4_breakpoints[MAX_BREAKPOINTS];
static int sh4_breakpoint_count = 0;
static uint16_t *sh4_icache = NULL;
static uint32_t sh4_icache_addr = 0;

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

uint32_t sh4_run_slice( uint32_t nanosecs ) 
{
    int i;
    sh4r.slice_cycle = 0;

    if( sh4r.sh4_state != SH4_STATE_RUNNING ) {
	if( sh4r.event_pending < nanosecs ) {
	    sh4r.sh4_state = SH4_STATE_RUNNING;
	    sh4r.slice_cycle = sh4r.event_pending;
	}
    }

    if( sh4_breakpoint_count == 0 ) {
	for( ; sh4r.slice_cycle < nanosecs; sh4r.slice_cycle += sh4_cpu_period ) {
	    if( SH4_EVENT_PENDING() ) {
		if( sh4r.event_types & PENDING_EVENT ) {
		    event_execute();
		}
		/* Eventq execute may (quite likely) deliver an immediate IRQ */
		if( sh4r.event_types & PENDING_IRQ ) {
		    sh4_accept_interrupt();
		}
	    }
	    if( !sh4_execute_instruction() ) {
		break;
	    }
	}
    } else {
	for( ;sh4r.slice_cycle < nanosecs; sh4r.slice_cycle += sh4_cpu_period ) {
	    if( SH4_EVENT_PENDING() ) {
		if( sh4r.event_types & PENDING_EVENT ) {
		    event_execute();
		}
		/* Eventq execute may (quite likely) deliver an immediate IRQ */
		if( sh4r.event_types & PENDING_IRQ ) {
		    sh4_accept_interrupt();
		}
	    }
                 
	    if( !sh4_execute_instruction() )
		break;
#ifdef ENABLE_DEBUG_MODE
	    for( i=0; i<sh4_breakpoint_count; i++ ) {
		if( sh4_breakpoints[i].address == sh4r.pc ) {
		    break;
		}
	    }
	    if( i != sh4_breakpoint_count ) {
		dreamcast_stop();
		if( sh4_breakpoints[i].type == BREAK_ONESHOT )
		    sh4_clear_breakpoint( sh4r.pc, BREAK_ONESHOT );
		break;
	    }
#endif	
	}
    }

    /* If we aborted early, but the cpu is still technically running,
     * we're doing a hard abort - cut the timeslice back to what we
     * actually executed
     */
    if( sh4r.slice_cycle != nanosecs && sh4r.sh4_state == SH4_STATE_RUNNING ) {
	nanosecs = sh4r.slice_cycle;
    }
    if( sh4r.sh4_state != SH4_STATE_STANDBY ) {
	TMU_run_slice( nanosecs );
	SCIF_run_slice( nanosecs );
    }
    return nanosecs;
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

/********************** SH4 emulation core  ****************************/

void sh4_set_pc( int pc )
{
    sh4r.pc = pc;
    sh4r.new_pc = pc+2;
}

#define UNDEF(ir) return sh4_raise_slot_exception(EXC_ILLEGAL, EXC_SLOT_ILLEGAL)
#define UNIMP(ir) do{ ERROR( "Halted on unimplemented instruction at %08x, opcode = %04x", sh4r.pc, ir ); dreamcast_stop(); return FALSE; }while(0)

#if(SH4_CALLTRACE == 1)
#define MAX_CALLSTACK 32
static struct call_stack {
    sh4addr_t call_addr;
    sh4addr_t target_addr;
    sh4addr_t stack_pointer;
} call_stack[MAX_CALLSTACK];

static int call_stack_depth = 0;
int sh4_call_trace_on = 0;

static inline trace_call( sh4addr_t source, sh4addr_t dest ) 
{
    if( call_stack_depth < MAX_CALLSTACK ) {
	call_stack[call_stack_depth].call_addr = source;
	call_stack[call_stack_depth].target_addr = dest;
	call_stack[call_stack_depth].stack_pointer = sh4r.r[15];
    }
    call_stack_depth++;
}

static inline trace_return( sh4addr_t source, sh4addr_t dest )
{
    if( call_stack_depth > 0 ) {
	call_stack_depth--;
    }
}

void fprint_stack_trace( FILE *f )
{
    int i = call_stack_depth -1;
    if( i >= MAX_CALLSTACK )
	i = MAX_CALLSTACK - 1;
    for( ; i >= 0; i-- ) {
	fprintf( f, "%d. Call from %08X => %08X, SP=%08X\n", 
		 (call_stack_depth - i), call_stack[i].call_addr,
		 call_stack[i].target_addr, call_stack[i].stack_pointer );
    }
}

#define TRACE_CALL( source, dest ) trace_call(source, dest)
#define TRACE_RETURN( source, dest ) trace_return(source, dest)
#else
#define TRACE_CALL( dest, rts ) 
#define TRACE_RETURN( source, dest )
#endif

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
        sh4_load_sr( sh4r.ssr |SR_MD|SR_BL|SR_RB ); \
	if( sh4r.in_delay_slot ) { \
	    sh4r.in_delay_slot = 0; \
	    sh4r.spc -= 2; \
	} \
    } \
    return TRUE; } while(0)

#define MEM_READ_BYTE( addr ) sh4_read_byte(addr)
#define MEM_READ_WORD( addr ) sh4_read_word(addr)
#define MEM_READ_LONG( addr ) sh4_read_long(addr)
#define MEM_WRITE_BYTE( addr, val ) sh4_write_byte(addr, val)
#define MEM_WRITE_WORD( addr, val ) sh4_write_word(addr, val)
#define MEM_WRITE_LONG( addr, val ) sh4_write_long(addr, val)

#define FP_WIDTH (IS_FPU_DOUBLESIZE() ? 8 : 4)

#define MEM_FP_READ( addr, reg ) sh4_read_float( addr, reg );
#define MEM_FP_WRITE( addr, reg ) sh4_write_float( addr, reg );

#define CHECKPRIV() if( !IS_SH4_PRIVMODE() ) return sh4_raise_slot_exception( EXC_ILLEGAL, EXC_SLOT_ILLEGAL )
#define CHECKRALIGN16(addr) if( (addr)&0x01 ) return sh4_raise_exception( EXC_READ_ADDR_ERR )
#define CHECKRALIGN32(addr) if( (addr)&0x03 ) return sh4_raise_exception( EXC_READ_ADDR_ERR )
#define CHECKWALIGN16(addr) if( (addr)&0x01 ) return sh4_raise_exception( EXC_WRITE_ADDR_ERR )
#define CHECKWALIGN32(addr) if( (addr)&0x03 ) return sh4_raise_exception( EXC_WRITE_ADDR_ERR )

#define CHECKFPUEN() if( !IS_FPU_ENABLED() ) return sh4_raise_slot_exception( EXC_FPDISABLE, EXC_SLOT_FPDISABLE )
#define CHECKDEST(p) if( (p) == 0 ) { ERROR( "%08X: Branch/jump to NULL, CPU halted", sh4r.pc ); dreamcast_stop(); return FALSE; }
#define CHECKSLOTILLEGAL() if(sh4r.in_delay_slot) return sh4_raise_exception(EXC_SLOT_ILLEGAL)

static void sh4_switch_banks( )
{
    uint32_t tmp[8];

    memcpy( tmp, sh4r.r, sizeof(uint32_t)*8 );
    memcpy( sh4r.r, sh4r.r_bank, sizeof(uint32_t)*8 );
    memcpy( sh4r.r_bank, tmp, sizeof(uint32_t)*8 );
}

static void sh4_load_sr( uint32_t newval )
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

static void sh4_write_float( uint32_t addr, int reg )
{
    if( IS_FPU_DOUBLESIZE() ) {
	if( reg & 1 ) {
	    sh4_write_long( addr, *((uint32_t *)&XF((reg)&0x0E)) );
	    sh4_write_long( addr+4, *((uint32_t *)&XF(reg)) );
	} else {
	    sh4_write_long( addr, *((uint32_t *)&FR(reg)) ); 
	    sh4_write_long( addr+4, *((uint32_t *)&FR((reg)|0x01)) );
	}
    } else {
	sh4_write_long( addr, *((uint32_t *)&FR((reg))) );
    }
}

static void sh4_read_float( uint32_t addr, int reg )
{
    if( IS_FPU_DOUBLESIZE() ) {
	if( reg & 1 ) {
	    *((uint32_t *)&XF((reg) & 0x0E)) = sh4_read_long(addr);
	    *((uint32_t *)&XF(reg)) = sh4_read_long(addr+4);
	} else {
	    *((uint32_t *)&FR(reg)) = sh4_read_long(addr);
	    *((uint32_t *)&FR((reg) | 0x01)) = sh4_read_long(addr+4);
	}
    } else {
	*((uint32_t *)&FR(reg)) = sh4_read_long(addr);
    }
}

static uint32_t sh4_read_sr( void )
{
    /* synchronize sh4r.sr with the various bitflags */
    sh4r.sr &= SR_MQSTMASK;
    if( sh4r.t ) sh4r.sr |= SR_T;
    if( sh4r.s ) sh4r.sr |= SR_S;
    if( sh4r.m ) sh4r.sr |= SR_M;
    if( sh4r.q ) sh4r.sr |= SR_Q;
    return sh4r.sr;
}

/**
 * Raise a general CPU exception for the specified exception code.
 * (NOT for TRAPA or TLB exceptions)
 */
gboolean sh4_raise_exception( int code )
{
    RAISE( code, EXV_EXCEPTION );
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

static void sh4_accept_interrupt( void )
{
    uint32_t code = intc_accept_interrupt();
    sh4r.ssr = sh4_read_sr();
    sh4r.spc = sh4r.pc;
    sh4r.sgr = sh4r.r[15];
    sh4_load_sr( sh4r.ssr|SR_BL|SR_MD|SR_RB );
    MMIO_WRITE( MMU, INTEVT, code );
    sh4r.pc = sh4r.vbr + 0x600;
    sh4r.new_pc = sh4r.pc + 2;
    //    WARN( "Accepting interrupt %03X, from %08X => %08X", code, sh4r.spc, sh4r.pc );
}

gboolean sh4_execute_instruction( void )
{
    uint32_t pc;
    unsigned short ir;
    uint32_t tmp;
    uint64_t tmpl;
    float ftmp;
    double dtmp;
    
#define R0 sh4r.r[0]
#define FR0 FR(0)
#define DR0 DR(0)
#define RN(ir) sh4r.r[(ir&0x0F00)>>8]
#define RN_BANK(ir) sh4r.r_bank[(ir&0x0070)>>4]
#define RM(ir) sh4r.r[(ir&0x00F0)>>4]
#define DISP4(ir) (ir&0x000F) /* 4-bit displacements are *NOT* sign-extended */
#define DISP8(ir) (ir&0x00FF)
#define PCDISP8(ir) SIGNEXT8(ir&0x00FF)
#define IMM8(ir) SIGNEXT8(ir&0x00FF)
#define UIMM8(ir) (ir&0x00FF) /* Unsigned immmediate */
#define DISP12(ir) SIGNEXT12(ir&0x0FFF)
#define FRNn(ir) ((ir&0x0F00)>>8)
#define FRMn(ir) ((ir&0x00F0)>>4)
#define DRNn(ir) ((ir&0x0E00)>>9)
#define DRMn(ir) ((ir&0x00E0)>>5)
#define FVN(ir) ((ir&0x0C00)>>8)
#define FVM(ir) ((ir&0x0300)>>6)
#define FRN(ir) FR(FRNn(ir))
#define FRM(ir) FR(FRMn(ir))
#define FRNi(ir) (*((uint32_t *)&FR(FRNn(ir))))
#define FRMi(ir) (*((uint32_t *)&FR(FRMn(ir))))
#define DRN(ir) DRb(DRNn(ir), ir&0x0100)
#define DRM(ir) DRb(DRMn(ir),ir&0x0010)
#define DRNi(ir) (*((uint64_t *)&DR(FRNn(ir))))
#define DRMi(ir) (*((uint64_t *)&DR(FRMn(ir))))
#define FPULf   *((float *)&sh4r.fpul)
#define FPULi    (sh4r.fpul)

    pc = sh4r.pc;
    if( pc > 0xFFFFFF00 ) {
	/* SYSCALL Magic */
	syscall_invoke( pc );
	sh4r.in_delay_slot = 0;
	pc = sh4r.pc = sh4r.pr;
	sh4r.new_pc = sh4r.pc + 2;
    }
    CHECKRALIGN16(pc);

    /* Read instruction */
    uint32_t pageaddr = pc >> 12;
    if( sh4_icache != NULL && pageaddr == sh4_icache_addr ) {
	ir = sh4_icache[(pc&0xFFF)>>1];
    } else {
	sh4_icache = (uint16_t *)mem_get_page(pc);
	if( ((uint32_t)sh4_icache) < MAX_IO_REGIONS ) {
	    /* If someone's actually been so daft as to try to execute out of an IO
	     * region, fallback on the full-blown memory read
	     */
	    sh4_icache = NULL;
	    ir = MEM_READ_WORD(pc);
	} else {
	    sh4_icache_addr = pageaddr;
	    ir = sh4_icache[(pc&0xFFF)>>1];
	}
    }
    
    switch( (ir&0xF000)>>12 ) {
        case 0: /* 0000nnnnmmmmxxxx */
            switch( ir&0x000F ) {
                case 2:
                    switch( (ir&0x00F0)>>4 ) {
                        case 0: /* STC     SR, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4_read_sr();
                            break;
                        case 1: /* STC     GBR, Rn */
                            RN(ir) = sh4r.gbr;
                            break;
                        case 2: /* STC     VBR, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4r.vbr;
                            break;
                        case 3: /* STC     SSR, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4r.ssr;
                            break;
                        case 4: /* STC     SPC, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4r.spc;
                            break;
                        case 8: case 9: case 10: case 11: case 12: case 13:
                        case 14: case 15:/* STC     Rm_bank, Rn */
                            CHECKPRIV();
                            RN(ir) = RN_BANK(ir);
                            break;
                        default: UNDEF(ir);
                    }
                    break;
                case 3:
                    switch( (ir&0x00F0)>>4 ) {
                        case 0: /* BSRF    Rn */
                            CHECKSLOTILLEGAL();
                            CHECKDEST( pc + 4 + RN(ir) );
                            sh4r.in_delay_slot = 1;
                            sh4r.pr = sh4r.pc + 4;
                            sh4r.pc = sh4r.new_pc;
                            sh4r.new_pc = pc + 4 + RN(ir);
			    TRACE_CALL( pc, sh4r.new_pc );
                            return TRUE;
                        case 2: /* BRAF    Rn */
                            CHECKSLOTILLEGAL();
                            CHECKDEST( pc + 4 + RN(ir) );
                            sh4r.in_delay_slot = 1;
                            sh4r.pc = sh4r.new_pc;
                            sh4r.new_pc = pc + 4 + RN(ir);
                            return TRUE;
                        case 8: /* PREF    [Rn] */
                            tmp = RN(ir);
                            if( (tmp & 0xFC000000) == 0xE0000000 ) {
                                /* Store queue operation */
                                int queue = (tmp&0x20)>>2;
                                int32_t *src = &sh4r.store_queue[queue];
                                uint32_t hi = (MMIO_READ( MMU, (queue == 0 ? QACR0 : QACR1) ) & 0x1C) << 24;
                                uint32_t target = tmp&0x03FFFFE0 | hi;
                                mem_copy_to_sh4( target, src, 32 );
                            }
                            break;
                        case 9: /* OCBI    [Rn] */
                        case 10:/* OCBP    [Rn] */
                        case 11:/* OCBWB   [Rn] */
                            /* anything? */
                            break;
                        case 12:/* MOVCA.L R0, [Rn] */
			    tmp = RN(ir);
			    CHECKWALIGN32(tmp);
			    MEM_WRITE_LONG( tmp, R0 );
			    break;
                        default: UNDEF(ir);
                    }
                    break;
                case 4: /* MOV.B   Rm, [R0 + Rn] */
                    MEM_WRITE_BYTE( R0 + RN(ir), RM(ir) );
                    break;
                case 5: /* MOV.W   Rm, [R0 + Rn] */
		    CHECKWALIGN16( R0 + RN(ir) );
                    MEM_WRITE_WORD( R0 + RN(ir), RM(ir) );
                    break;
                case 6: /* MOV.L   Rm, [R0 + Rn] */
		    CHECKWALIGN32( R0 + RN(ir) );
                    MEM_WRITE_LONG( R0 + RN(ir), RM(ir) );
                    break;
                case 7: /* MUL.L   Rm, Rn */
                    sh4r.mac = (sh4r.mac&0xFFFFFFFF00000000LL) |
                        (RM(ir) * RN(ir));
                    break;
                case 8: 
                    switch( (ir&0x0FF0)>>4 ) {
                        case 0: /* CLRT    */
                            sh4r.t = 0;
                            break;
                        case 1: /* SETT    */
                            sh4r.t = 1;
                            break;
                        case 2: /* CLRMAC  */
                            sh4r.mac = 0;
                            break;
                        case 3: /* LDTLB   */
                            break;
                        case 4: /* CLRS    */
                            sh4r.s = 0;
                            break;
                        case 5: /* SETS    */
                            sh4r.s = 1;
                            break;
                        default: UNDEF(ir);
                    }
                    break;
                case 9: 
                    if( (ir&0x00F0) == 0x20 ) /* MOVT    Rn */
                        RN(ir) = sh4r.t;
                    else if( ir == 0x0019 ) /* DIV0U   */
                        sh4r.m = sh4r.q = sh4r.t = 0;
                    else if( ir == 0x0009 )
                        /* NOP     */;
                    else UNDEF(ir);
                    break;
                case 10:
                    switch( (ir&0x00F0) >> 4 ) {
                        case 0: /* STS     MACH, Rn */
                            RN(ir) = sh4r.mac >> 32;
                            break;
                        case 1: /* STS     MACL, Rn */
                            RN(ir) = (uint32_t)sh4r.mac;
                            break;
                        case 2: /* STS     PR, Rn */
                            RN(ir) = sh4r.pr;
                            break;
                        case 3: /* STC     SGR, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4r.sgr;
                            break;
                        case 5:/* STS      FPUL, Rn */
                            RN(ir) = sh4r.fpul;
                            break;
                        case 6: /* STS     FPSCR, Rn */
                            RN(ir) = sh4r.fpscr;
                            break;
                        case 15:/* STC     DBR, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4r.dbr;
                            break;
                        default: UNDEF(ir);
                    }
                    break;
                case 11:
                    switch( (ir&0x0FF0)>>4 ) {
                        case 0: /* RTS     */
                            CHECKSLOTILLEGAL();
                            CHECKDEST( sh4r.pr );
                            sh4r.in_delay_slot = 1;
                            sh4r.pc = sh4r.new_pc;
                            sh4r.new_pc = sh4r.pr;
                            TRACE_RETURN( pc, sh4r.new_pc );
                            return TRUE;
                        case 1: /* SLEEP   */
			    if( MMIO_READ( CPG, STBCR ) & 0x80 ) {
				sh4r.sh4_state = SH4_STATE_STANDBY;
			    } else {
				sh4r.sh4_state = SH4_STATE_SLEEP;
			    }
			    return FALSE; /* Halt CPU */
                        case 2: /* RTE     */
                            CHECKPRIV();
                            CHECKDEST( sh4r.spc );
                            CHECKSLOTILLEGAL();
                            sh4r.in_delay_slot = 1;
                            sh4r.pc = sh4r.new_pc;
                            sh4r.new_pc = sh4r.spc;
                            sh4_load_sr( sh4r.ssr );
                            return TRUE;
                        default:UNDEF(ir);
                    }
                    break;
                case 12:/* MOV.B   [R0+R%d], R%d */
                    RN(ir) = MEM_READ_BYTE( R0 + RM(ir) );
                    break;
                case 13:/* MOV.W   [R0+R%d], R%d */
		    CHECKRALIGN16( R0 + RM(ir) );
                    RN(ir) = MEM_READ_WORD( R0 + RM(ir) );
                    break;
                case 14:/* MOV.L   [R0+R%d], R%d */
		    CHECKRALIGN32( R0 + RM(ir) );
                    RN(ir) = MEM_READ_LONG( R0 + RM(ir) );
                    break;
                case 15:/* MAC.L   [Rm++], [Rn++] */
		    CHECKRALIGN32( RM(ir) );
		    CHECKRALIGN32( RN(ir) );
                    tmpl = ( SIGNEXT32(MEM_READ_LONG(RM(ir))) *
                                  SIGNEXT32(MEM_READ_LONG(RN(ir))) );
                    if( sh4r.s ) {
                        /* 48-bit Saturation. Yuch */
                        tmpl += SIGNEXT48(sh4r.mac);
                        if( tmpl < 0xFFFF800000000000LL )
                            tmpl = 0xFFFF800000000000LL;
                        else if( tmpl > 0x00007FFFFFFFFFFFLL )
                            tmpl = 0x00007FFFFFFFFFFFLL;
                        sh4r.mac = (sh4r.mac&0xFFFF000000000000LL) |
                            (tmpl&0x0000FFFFFFFFFFFFLL);
                    } else sh4r.mac = tmpl;
                    
                    RM(ir) += 4;
                    RN(ir) += 4;
                    
                    break;
                default: UNDEF(ir);
            }
            break;
        case 1: /* 0001nnnnmmmmdddd */
            /* MOV.L   Rm, [Rn + disp4*4] */
	    tmp = RN(ir) + (DISP4(ir)<<2);
	    CHECKWALIGN32( tmp );
            MEM_WRITE_LONG( tmp, RM(ir) );
            break;
        case 2: /* 0010nnnnmmmmxxxx */
            switch( ir&0x000F ) {
                case 0: /* MOV.B   Rm, [Rn] */
                    MEM_WRITE_BYTE( RN(ir), RM(ir) );
                    break;
                case 1: /* MOV.W   Rm, [Rn] */
               	    CHECKWALIGN16( RN(ir) );
		    MEM_WRITE_WORD( RN(ir), RM(ir) );
                    break;
                case 2: /* MOV.L   Rm, [Rn] */
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), RM(ir) );
                    break;
                case 3: UNDEF(ir);
                    break;
                case 4: /* MOV.B   Rm, [--Rn] */
                    RN(ir) --;
                    MEM_WRITE_BYTE( RN(ir), RM(ir) );
                    break;
                case 5: /* MOV.W   Rm, [--Rn] */
                    RN(ir) -= 2;
		    CHECKWALIGN16( RN(ir) );
                    MEM_WRITE_WORD( RN(ir), RM(ir) );
                    break;
                case 6: /* MOV.L   Rm, [--Rn] */
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), RM(ir) );
                    break;
                case 7: /* DIV0S   Rm, Rn */
                    sh4r.q = RN(ir)>>31;
                    sh4r.m = RM(ir)>>31;
                    sh4r.t = sh4r.q ^ sh4r.m;
                    break;
                case 8: /* TST     Rm, Rn */
                    sh4r.t = (RN(ir)&RM(ir) ? 0 : 1);
                    break;
                case 9: /* AND     Rm, Rn */
                    RN(ir) &= RM(ir);
                    break;
                case 10:/* XOR     Rm, Rn */
                    RN(ir) ^= RM(ir);
                    break;
                case 11:/* OR      Rm, Rn */
                    RN(ir) |= RM(ir);
                    break;
                case 12:/* CMP/STR Rm, Rn */
                    /* set T = 1 if any byte in RM & RN is the same */
                    tmp = RM(ir) ^ RN(ir);
                    sh4r.t = ((tmp&0x000000FF)==0 || (tmp&0x0000FF00)==0 ||
                              (tmp&0x00FF0000)==0 || (tmp&0xFF000000)==0)?1:0;
                    break;
                case 13:/* XTRCT   Rm, Rn */
                    RN(ir) = (RN(ir)>>16) | (RM(ir)<<16);
                    break;
                case 14:/* MULU.W  Rm, Rn */
                    sh4r.mac = (sh4r.mac&0xFFFFFFFF00000000LL) |
                        (uint32_t)((RM(ir)&0xFFFF) * (RN(ir)&0xFFFF));
                    break;
                case 15:/* MULS.W  Rm, Rn */
                    sh4r.mac = (sh4r.mac&0xFFFFFFFF00000000LL) |
                        (uint32_t)(SIGNEXT32(RM(ir)&0xFFFF) * SIGNEXT32(RN(ir)&0xFFFF));
                    break;
            }
            break;
        case 3: /* 0011nnnnmmmmxxxx */
            switch( ir&0x000F ) {
                case 0: /* CMP/EQ  Rm, Rn */
                    sh4r.t = ( RM(ir) == RN(ir) ? 1 : 0 );
                    break;
                case 2: /* CMP/HS  Rm, Rn */
                    sh4r.t = ( RN(ir) >= RM(ir) ? 1 : 0 );
                    break;
                case 3: /* CMP/GE  Rm, Rn */
                    sh4r.t = ( ((int32_t)RN(ir)) >= ((int32_t)RM(ir)) ? 1 : 0 );
                    break;
                case 4: { /* DIV1    Rm, Rn */
                    /* This is just from the sh4p manual with some
                     * simplifications (someone want to check it's correct? :)
                     * Why they couldn't just provide a real DIV instruction...
                     * Please oh please let the translator batch these things
                     * up into a single DIV... */
                    uint32_t tmp0, tmp1, tmp2, dir;

                    dir = sh4r.q ^ sh4r.m;
                    sh4r.q = (RN(ir) >> 31);
                    tmp2 = RM(ir);
                    RN(ir) = (RN(ir) << 1) | sh4r.t;
                    tmp0 = RN(ir);
                    if( dir ) {
                        RN(ir) += tmp2;
                        tmp1 = (RN(ir)<tmp0 ? 1 : 0 );
                    } else {
                        RN(ir) -= tmp2;
                        tmp1 = (RN(ir)>tmp0 ? 1 : 0 );
                    }
                    sh4r.q ^= sh4r.m ^ tmp1;
                    sh4r.t = ( sh4r.q == sh4r.m ? 1 : 0 );
                    break; }
                case 5: /* DMULU.L Rm, Rn */
                    sh4r.mac = ((uint64_t)RM(ir)) * ((uint64_t)RN(ir));
                    break;
                case 6: /* CMP/HI  Rm, Rn */
                    sh4r.t = ( RN(ir) > RM(ir) ? 1 : 0 );
                    break;
                case 7: /* CMP/GT  Rm, Rn */
                    sh4r.t = ( ((int32_t)RN(ir)) > ((int32_t)RM(ir)) ? 1 : 0 );
                    break;
                case 8: /* SUB     Rm, Rn */
                    RN(ir) -= RM(ir);
                    break;
                case 10:/* SUBC    Rm, Rn */
                    tmp = RN(ir);
                    RN(ir) = RN(ir) - RM(ir) - sh4r.t;
                    sh4r.t = (RN(ir) > tmp || (RN(ir) == tmp && sh4r.t == 1));
                    break;
                case 11:/* SUBV    Rm, Rn */
                    UNIMP(ir);
                    break;
                case 12:/* ADD     Rm, Rn */
                    RN(ir) += RM(ir);
                    break;
                case 13:/* DMULS.L Rm, Rn */
                    sh4r.mac = SIGNEXT32(RM(ir)) * SIGNEXT32(RN(ir));
                    break;
                case 14:/* ADDC    Rm, Rn */
                    tmp = RN(ir);
                    RN(ir) += RM(ir) + sh4r.t;
                    sh4r.t = ( RN(ir) < tmp || (RN(ir) == tmp && sh4r.t != 0) ? 1 : 0 );
                    break;
                case 15:/* ADDV    Rm, Rn */
		    tmp = RN(ir) + RM(ir);
		    sh4r.t = ( (RN(ir)>>31) == (RM(ir)>>31) && ((RN(ir)>>31) != (tmp>>31)) );
		    RN(ir) = tmp;
                    break;
                default: UNDEF(ir);
            }
            break;
        case 4: /* 0100nnnnxxxxxxxx */
            switch( ir&0x00FF ) {
                case 0x00: /* SHLL    Rn */
                    sh4r.t = RN(ir) >> 31;
                    RN(ir) <<= 1;
                    break;
                case 0x01: /* SHLR    Rn */
                    sh4r.t = RN(ir) & 0x00000001;
                    RN(ir) >>= 1;
                    break;
                case 0x02: /* STS.L   MACH, [--Rn] */
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), (sh4r.mac>>32) );
                    break;
                case 0x03: /* STC.L   SR, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), sh4_read_sr() );
                    break;
                case 0x04: /* ROTL    Rn */
                    sh4r.t = RN(ir) >> 31;
                    RN(ir) <<= 1;
                    RN(ir) |= sh4r.t;
                    break;
                case 0x05: /* ROTR    Rn */
                    sh4r.t = RN(ir) & 0x00000001;
                    RN(ir) >>= 1;
                    RN(ir) |= (sh4r.t << 31);
                    break;
                case 0x06: /* LDS.L   [Rn++], MACH */
		    CHECKRALIGN32( RN(ir) );
                    sh4r.mac = (sh4r.mac & 0x00000000FFFFFFFF) |
                        (((uint64_t)MEM_READ_LONG(RN(ir)))<<32);
                    RN(ir) += 4;
                    break;
                case 0x07: /* LDC.L   [Rn++], SR */
		    CHECKSLOTILLEGAL();
                    CHECKPRIV();
		    CHECKWALIGN32( RN(ir) );
                    sh4_load_sr( MEM_READ_LONG(RN(ir)) );
                    RN(ir) +=4;
                    break;
                case 0x08: /* SHLL2   Rn */
                    RN(ir) <<= 2;
                    break;
                case 0x09: /* SHLR2   Rn */
                    RN(ir) >>= 2;
                    break;
                case 0x0A: /* LDS     Rn, MACH */
                    sh4r.mac = (sh4r.mac & 0x00000000FFFFFFFF) |
                        (((uint64_t)RN(ir))<<32);
                    break;
                case 0x0B: /* JSR     [Rn] */
                    CHECKDEST( RN(ir) );
                    CHECKSLOTILLEGAL();
                    sh4r.in_delay_slot = 1;
                    sh4r.pc = sh4r.new_pc;
                    sh4r.new_pc = RN(ir);
                    sh4r.pr = pc + 4;
		    TRACE_CALL( pc, sh4r.new_pc );
                    return TRUE;
                case 0x0E: /* LDC     Rn, SR */
		    CHECKSLOTILLEGAL();
                    CHECKPRIV();
                    sh4_load_sr( RN(ir) );
                    break;
                case 0x10: /* DT      Rn */
                    RN(ir) --;
                    sh4r.t = ( RN(ir) == 0 ? 1 : 0 );
                    break;
                case 0x11: /* CMP/PZ  Rn */
                    sh4r.t = ( ((int32_t)RN(ir)) >= 0 ? 1 : 0 );
                    break;
                case 0x12: /* STS.L   MACL, [--Rn] */
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), (uint32_t)sh4r.mac );
                    break;
                case 0x13: /* STC.L   GBR, [--Rn] */
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), sh4r.gbr );
                    break;
                case 0x15: /* CMP/PL  Rn */
                    sh4r.t = ( ((int32_t)RN(ir)) > 0 ? 1 : 0 );
                    break;
                case 0x16: /* LDS.L   [Rn++], MACL */
		    CHECKRALIGN32( RN(ir) );
                    sh4r.mac = (sh4r.mac & 0xFFFFFFFF00000000LL) |
                        (uint64_t)((uint32_t)MEM_READ_LONG(RN(ir)));
                    RN(ir) += 4;
                    break;
                case 0x17: /* LDC.L   [Rn++], GBR */
		    CHECKRALIGN32( RN(ir) );
                    sh4r.gbr = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x18: /* SHLL8   Rn */
                    RN(ir) <<= 8;
                    break;
                case 0x19: /* SHLR8   Rn */
                    RN(ir) >>= 8;
                    break;
                case 0x1A: /* LDS     Rn, MACL */
                    sh4r.mac = (sh4r.mac & 0xFFFFFFFF00000000LL) |
                        (uint64_t)((uint32_t)(RN(ir)));
                    break;
                case 0x1B: /* TAS.B   [Rn] */
                    tmp = MEM_READ_BYTE( RN(ir) );
                    sh4r.t = ( tmp == 0 ? 1 : 0 );
                    MEM_WRITE_BYTE( RN(ir), tmp | 0x80 );
                    break;
                case 0x1E: /* LDC     Rn, GBR */
                    sh4r.gbr = RN(ir);
                    break;
                case 0x20: /* SHAL    Rn */
                    sh4r.t = RN(ir) >> 31;
                    RN(ir) <<= 1;
                    break;
                case 0x21: /* SHAR    Rn */
                    sh4r.t = RN(ir) & 0x00000001;
                    RN(ir) = ((int32_t)RN(ir)) >> 1;
                    break;
                case 0x22: /* STS.L   PR, [--Rn] */
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), sh4r.pr );
                    break;
                case 0x23: /* STC.L   VBR, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), sh4r.vbr );
                    break;
                case 0x24: /* ROTCL   Rn */
                    tmp = RN(ir) >> 31;
                    RN(ir) <<= 1;
                    RN(ir) |= sh4r.t;
                    sh4r.t = tmp;
                    break;
                case 0x25: /* ROTCR   Rn */
                    tmp = RN(ir) & 0x00000001;
                    RN(ir) >>= 1;
                    RN(ir) |= (sh4r.t << 31 );
                    sh4r.t = tmp;
                    break;
                case 0x26: /* LDS.L   [Rn++], PR */
		    CHECKRALIGN32( RN(ir) );
                    sh4r.pr = MEM_READ_LONG( RN(ir) );
                    RN(ir) += 4;
                    break;
                case 0x27: /* LDC.L   [Rn++], VBR */
                    CHECKPRIV();
		    CHECKRALIGN32( RN(ir) );
                    sh4r.vbr = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x28: /* SHLL16  Rn */
                    RN(ir) <<= 16;
                    break;
                case 0x29: /* SHLR16  Rn */
                    RN(ir) >>= 16;
                    break;
                case 0x2A: /* LDS     Rn, PR */
                    sh4r.pr = RN(ir);
                    break;
                case 0x2B: /* JMP     [Rn] */
                    CHECKDEST( RN(ir) );
                    CHECKSLOTILLEGAL();
                    sh4r.in_delay_slot = 1;
                    sh4r.pc = sh4r.new_pc;
                    sh4r.new_pc = RN(ir);
                    return TRUE;
                case 0x2E: /* LDC     Rn, VBR */
                    CHECKPRIV();
                    sh4r.vbr = RN(ir);
                    break;
                case 0x32: /* STC.L   SGR, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), sh4r.sgr );
                    break;
                case 0x33: /* STC.L   SSR, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), sh4r.ssr );
                    break;
                case 0x37: /* LDC.L   [Rn++], SSR */
                    CHECKPRIV();
		    CHECKRALIGN32( RN(ir) );
                    sh4r.ssr = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x3E: /* LDC     Rn, SSR */
                    CHECKPRIV();
                    sh4r.ssr = RN(ir);
                    break;
                case 0x43: /* STC.L   SPC, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), sh4r.spc );
                    break;
                case 0x47: /* LDC.L   [Rn++], SPC */
                    CHECKPRIV();
		    CHECKRALIGN32( RN(ir) );
                    sh4r.spc = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x4E: /* LDC     Rn, SPC */
                    CHECKPRIV();
                    sh4r.spc = RN(ir);
                    break;
                case 0x52: /* STS.L   FPUL, [--Rn] */
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), sh4r.fpul );
                    break;
                case 0x56: /* LDS.L   [Rn++], FPUL */
		    CHECKRALIGN32( RN(ir) );
                    sh4r.fpul = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x5A: /* LDS     Rn, FPUL */
                    sh4r.fpul = RN(ir);
                    break;
                case 0x62: /* STS.L   FPSCR, [--Rn] */
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), sh4r.fpscr );
                    break;
                case 0x66: /* LDS.L   [Rn++], FPSCR */
		    CHECKRALIGN32( RN(ir) );
                    sh4r.fpscr = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x6A: /* LDS     Rn, FPSCR */
                    sh4r.fpscr = RN(ir);
                    break;
                case 0xF2: /* STC.L   DBR, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), sh4r.dbr );
                    break;
                case 0xF6: /* LDC.L   [Rn++], DBR */
                    CHECKPRIV();
		    CHECKRALIGN32( RN(ir) );
                    sh4r.dbr = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0xFA: /* LDC     Rn, DBR */
                    CHECKPRIV();
                    sh4r.dbr = RN(ir);
                    break;
                case 0x83: case 0x93: case 0xA3: case 0xB3: case 0xC3:
                case 0xD3: case 0xE3: case 0xF3: /* STC.L   Rn_BANK, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
		    CHECKWALIGN32( RN(ir) );
                    MEM_WRITE_LONG( RN(ir), RN_BANK(ir) );
                    break;
                case 0x87: case 0x97: case 0xA7: case 0xB7: case 0xC7:
                case 0xD7: case 0xE7: case 0xF7: /* LDC.L   [Rn++], Rn_BANK */
                    CHECKPRIV();
		    CHECKRALIGN32( RN(ir) );
                    RN_BANK(ir) = MEM_READ_LONG( RN(ir) );
                    RN(ir) += 4;
                    break;
                case 0x8E: case 0x9E: case 0xAE: case 0xBE: case 0xCE:
                case 0xDE: case 0xEE: case 0xFE: /* LDC     Rm, Rn_BANK */
                    CHECKPRIV();
                    RN_BANK(ir) = RM(ir);
                    break;
                default:
                    if( (ir&0x000F) == 0x0F ) {
                        /* MAC.W   [Rm++], [Rn++] */
			CHECKRALIGN16( RN(ir) );
			CHECKRALIGN16( RM(ir) );
                        tmp = SIGNEXT16(MEM_READ_WORD(RM(ir))) *
                            SIGNEXT16(MEM_READ_WORD(RN(ir)));
                        if( sh4r.s ) {
                            /* FIXME */
                            UNIMP(ir);
                        } else sh4r.mac += SIGNEXT32(tmp);
                        RM(ir) += 2;
                        RN(ir) += 2;
                    } else if( (ir&0x000F) == 0x0C ) {
                        /* SHAD    Rm, Rn */
                        tmp = RM(ir);
                        if( (tmp & 0x80000000) == 0 ) RN(ir) <<= (tmp&0x1f);
                        else if( (tmp & 0x1F) == 0 )  
			  RN(ir) = ((int32_t)RN(ir)) >> 31;
                        else 
			  RN(ir) = ((int32_t)RN(ir)) >> (((~RM(ir)) & 0x1F)+1);
                    } else if( (ir&0x000F) == 0x0D ) {
                        /* SHLD    Rm, Rn */
                        tmp = RM(ir);
                        if( (tmp & 0x80000000) == 0 ) RN(ir) <<= (tmp&0x1f);
                        else if( (tmp & 0x1F) == 0 ) RN(ir) = 0;
                        else RN(ir) >>= (((~tmp) & 0x1F)+1);
                    } else UNDEF(ir);
            }
            break;
        case 5: /* 0101nnnnmmmmdddd */
            /* MOV.L   [Rm + disp4*4], Rn */
	    tmp = RM(ir) + (DISP4(ir)<<2);
	    CHECKRALIGN32( tmp );
            RN(ir) = MEM_READ_LONG( tmp );
            break;
        case 6: /* 0110xxxxxxxxxxxx */
            switch( ir&0x000f ) {
                case 0: /* MOV.B   [Rm], Rn */
                    RN(ir) = MEM_READ_BYTE( RM(ir) );
                    break;
                case 1: /* MOV.W   [Rm], Rn */
		    CHECKRALIGN16( RM(ir) );
                    RN(ir) = MEM_READ_WORD( RM(ir) );
                    break;
                case 2: /* MOV.L   [Rm], Rn */
		    CHECKRALIGN32( RM(ir) );
                    RN(ir) = MEM_READ_LONG( RM(ir) );
                    break;
                case 3: /* MOV     Rm, Rn */
                    RN(ir) = RM(ir);
                    break;
                case 4: /* MOV.B   [Rm++], Rn */
                    RN(ir) = MEM_READ_BYTE( RM(ir) );
                    RM(ir) ++;
                    break;
                case 5: /* MOV.W   [Rm++], Rn */
		    CHECKRALIGN16( RM(ir) );
                    RN(ir) = MEM_READ_WORD( RM(ir) );
                    RM(ir) += 2;
                    break;
                case 6: /* MOV.L   [Rm++], Rn */
		    CHECKRALIGN32( RM(ir) );
                    RN(ir) = MEM_READ_LONG( RM(ir) );
                    RM(ir) += 4;
                    break;
                case 7: /* NOT     Rm, Rn */
                    RN(ir) = ~RM(ir);
                    break;
                case 8: /* SWAP.B  Rm, Rn */
                    RN(ir) = (RM(ir)&0xFFFF0000) | ((RM(ir)&0x0000FF00)>>8) |
                        ((RM(ir)&0x000000FF)<<8);
                    break;
                case 9: /* SWAP.W  Rm, Rn */
                    RN(ir) = (RM(ir)>>16) | (RM(ir)<<16);
                    break;
                case 10:/* NEGC    Rm, Rn */
                    tmp = 0 - RM(ir);
                    RN(ir) = tmp - sh4r.t;
                    sh4r.t = ( 0<tmp || tmp<RN(ir) ? 1 : 0 );
                    break;
                case 11:/* NEG     Rm, Rn */
                    RN(ir) = 0 - RM(ir);
                    break;
                case 12:/* EXTU.B  Rm, Rn */
                    RN(ir) = RM(ir)&0x000000FF;
                    break;
                case 13:/* EXTU.W  Rm, Rn */
                    RN(ir) = RM(ir)&0x0000FFFF;
                    break;
                case 14:/* EXTS.B  Rm, Rn */
                    RN(ir) = SIGNEXT8( RM(ir)&0x000000FF );
                    break;
                case 15:/* EXTS.W  Rm, Rn */
                    RN(ir) = SIGNEXT16( RM(ir)&0x0000FFFF );
                    break;
            }
            break;
        case 7: /* 0111nnnniiiiiiii */
            /* ADD    imm8, Rn */
            RN(ir) += IMM8(ir);
            break;
        case 8: /* 1000xxxxxxxxxxxx */
            switch( (ir&0x0F00) >> 8 ) {
                case 0: /* MOV.B   R0, [Rm + disp4] */
                    MEM_WRITE_BYTE( RM(ir) + DISP4(ir), R0 );
                    break;
                case 1: /* MOV.W   R0, [Rm + disp4*2] */
		    tmp = RM(ir) + (DISP4(ir)<<1);
		    CHECKWALIGN16( tmp );
                    MEM_WRITE_WORD( tmp, R0 );
                    break;
                case 4: /* MOV.B   [Rm + disp4], R0 */
                    R0 = MEM_READ_BYTE( RM(ir) + DISP4(ir) );
                    break;
                case 5: /* MOV.W   [Rm + disp4*2], R0 */
		    tmp = RM(ir) + (DISP4(ir)<<1);
		    CHECKRALIGN16( tmp );
                    R0 = MEM_READ_WORD( tmp );
                    break;
                case 8: /* CMP/EQ  imm, R0 */
                    sh4r.t = ( R0 == IMM8(ir) ? 1 : 0 );
                    break;
                case 9: /* BT      disp8 */
                    CHECKSLOTILLEGAL();
                    if( sh4r.t ) {
                        CHECKDEST( sh4r.pc + (PCDISP8(ir)<<1) + 4 )
                        sh4r.pc += (PCDISP8(ir)<<1) + 4;
                        sh4r.new_pc = sh4r.pc + 2;
                        return TRUE;
                    }
                    break;
                case 11:/* BF      disp8 */
                    CHECKSLOTILLEGAL();
                    if( !sh4r.t ) {
                        CHECKDEST( sh4r.pc + (PCDISP8(ir)<<1) + 4 )
                        sh4r.pc += (PCDISP8(ir)<<1) + 4;
                        sh4r.new_pc = sh4r.pc + 2;
                        return TRUE;
                    }
                    break;
                case 13:/* BT/S    disp8 */
                    CHECKSLOTILLEGAL();
                    if( sh4r.t ) {
                        CHECKDEST( sh4r.pc + (PCDISP8(ir)<<1) + 4 )
                        sh4r.in_delay_slot = 1;
                        sh4r.pc = sh4r.new_pc;
                        sh4r.new_pc = pc + (PCDISP8(ir)<<1) + 4;
                        sh4r.in_delay_slot = 1;
                        return TRUE;
                    }
                    break;
                case 15:/* BF/S    disp8 */
                    CHECKSLOTILLEGAL();
                    if( !sh4r.t ) {
                        CHECKDEST( sh4r.pc + (PCDISP8(ir)<<1) + 4 )
                        sh4r.in_delay_slot = 1;
                        sh4r.pc = sh4r.new_pc;
                        sh4r.new_pc = pc + (PCDISP8(ir)<<1) + 4;
                        return TRUE;
                    }
                    break;
                default: UNDEF(ir);
            }
            break;
        case 9: /* 1001xxxxxxxxxxxx */
            /* MOV.W   [disp8*2 + pc + 4], Rn */
	    CHECKSLOTILLEGAL();
	    tmp = pc + 4 + (DISP8(ir)<<1);
            RN(ir) = MEM_READ_WORD( tmp );
            break;
        case 10:/* 1010dddddddddddd */
            /* BRA     disp12 */
            CHECKSLOTILLEGAL();
            CHECKDEST( sh4r.pc + (DISP12(ir)<<1) + 4 );
            sh4r.in_delay_slot = 1;
            sh4r.pc = sh4r.new_pc;
            sh4r.new_pc = pc + 4 + (DISP12(ir)<<1);
            return TRUE;
        case 11:/* 1011dddddddddddd */
            /* BSR     disp12 */
            CHECKDEST( sh4r.pc + (DISP12(ir)<<1) + 4 );
	    CHECKSLOTILLEGAL();
            sh4r.in_delay_slot = 1;
            sh4r.pr = pc + 4;
            sh4r.pc = sh4r.new_pc;
            sh4r.new_pc = pc + 4 + (DISP12(ir)<<1);
	    TRACE_CALL( pc, sh4r.new_pc );
            return TRUE;
        case 12:/* 1100xxxxdddddddd */
        switch( (ir&0x0F00)>>8 ) {
                case 0: /* MOV.B  R0, [GBR + disp8] */
                    MEM_WRITE_BYTE( sh4r.gbr + DISP8(ir), R0 );
                    break;
                case 1: /* MOV.W  R0, [GBR + disp8*2] */
		    tmp = sh4r.gbr + (DISP8(ir)<<1);
		    CHECKWALIGN16( tmp );
                    MEM_WRITE_WORD( tmp, R0 );
                    break;
                case  2: /*MOV.L   R0, [GBR + disp8*4] */
		    tmp = sh4r.gbr + (DISP8(ir)<<2);
		    CHECKWALIGN32( tmp );
                    MEM_WRITE_LONG( tmp, R0 );
                    break;
                case 3: /* TRAPA   imm8 */
                    CHECKSLOTILLEGAL();
                    MMIO_WRITE( MMU, TRA, UIMM8(ir)<<2 );
		    sh4r.pc += 2;
                    sh4_raise_exception( EXC_TRAP );
                    break;
                case 4: /* MOV.B   [GBR + disp8], R0 */
                    R0 = MEM_READ_BYTE( sh4r.gbr + DISP8(ir) );
                    break;
                case 5: /* MOV.W   [GBR + disp8*2], R0 */
		    tmp = sh4r.gbr + (DISP8(ir)<<1);
		    CHECKRALIGN16( tmp );
                    R0 = MEM_READ_WORD( tmp );
                    break;
                case 6: /* MOV.L   [GBR + disp8*4], R0 */
		    tmp = sh4r.gbr + (DISP8(ir)<<2);
		    CHECKRALIGN32( tmp );
                    R0 = MEM_READ_LONG( tmp );
                    break;
                case 7: /* MOVA    disp8 + pc&~3 + 4, R0 */
		    CHECKSLOTILLEGAL();
                    R0 = (pc&0xFFFFFFFC) + (DISP8(ir)<<2) + 4;
                    break;
                case 8: /* TST     imm8, R0 */
                    sh4r.t = (R0 & UIMM8(ir) ? 0 : 1);
                    break;
                case 9: /* AND     imm8, R0 */
                    R0 &= UIMM8(ir);
                    break;
                case 10:/* XOR     imm8, R0 */
                    R0 ^= UIMM8(ir);
                    break;
                case 11:/* OR      imm8, R0 */
                    R0 |= UIMM8(ir);
                    break;
                case 12:/* TST.B   imm8, [R0+GBR] */		    
                    sh4r.t = ( MEM_READ_BYTE(R0 + sh4r.gbr) & UIMM8(ir) ? 0 : 1 );
                    break;
                case 13:/* AND.B   imm8, [R0+GBR] */
                    MEM_WRITE_BYTE( R0 + sh4r.gbr,
                                    UIMM8(ir) & MEM_READ_BYTE(R0 + sh4r.gbr) );
                    break;
                case 14:/* XOR.B   imm8, [R0+GBR] */
                    MEM_WRITE_BYTE( R0 + sh4r.gbr,
                                    UIMM8(ir) ^ MEM_READ_BYTE(R0 + sh4r.gbr) );
                    break;
                case 15:/* OR.B    imm8, [R0+GBR] */
                    MEM_WRITE_BYTE( R0 + sh4r.gbr,
                                    UIMM8(ir) | MEM_READ_BYTE(R0 + sh4r.gbr) );
                    break;
            }
            break;
        case 13:/* 1101nnnndddddddd */
            /* MOV.L   [disp8*4 + pc&~3 + 4], Rn */
	    CHECKSLOTILLEGAL();
	    tmp = (pc&0xFFFFFFFC) + (DISP8(ir)<<2) + 4;
            RN(ir) = MEM_READ_LONG( tmp );
            break;
        case 14:/* 1110nnnniiiiiiii */
            /* MOV     imm8, Rn */
            RN(ir) = IMM8(ir);
            break;
        case 15:/* 1111xxxxxxxxxxxx */
            CHECKFPUEN();
	    if( IS_FPU_DOUBLEPREC() ) {
		switch( ir&0x000F ) {
                case 0: /* FADD    FRm, FRn */
                    DRN(ir) += DRM(ir);
                    break;
                case 1: /* FSUB    FRm, FRn */
                    DRN(ir) -= DRM(ir);
                    break;
                case 2: /* FMUL    FRm, FRn */
                    DRN(ir) = DRN(ir) * DRM(ir);
                    break;
                case 3: /* FDIV    FRm, FRn */
                    DRN(ir) = DRN(ir) / DRM(ir);
                    break;
                case 4: /* FCMP/EQ FRm, FRn */
                    sh4r.t = ( DRN(ir) == DRM(ir) ? 1 : 0 );
                    break;
                case 5: /* FCMP/GT FRm, FRn */
                    sh4r.t = ( DRN(ir) > DRM(ir) ? 1 : 0 );
                    break;
                case 6: /* FMOV.S  [Rm+R0], FRn */
                    MEM_FP_READ( RM(ir) + R0, FRNn(ir) );
                    break;
                case 7: /* FMOV.S  FRm, [Rn+R0] */
                    MEM_FP_WRITE( RN(ir) + R0, FRMn(ir) );
                    break;
                case 8: /* FMOV.S  [Rm], FRn */
                    MEM_FP_READ( RM(ir), FRNn(ir) );
                    break;
                case 9: /* FMOV.S  [Rm++], FRn */
                    MEM_FP_READ( RM(ir), FRNn(ir) );
                    RM(ir) += FP_WIDTH;
                    break;
                case 10:/* FMOV.S  FRm, [Rn] */
                    MEM_FP_WRITE( RN(ir), FRMn(ir) );
                    break;
                case 11:/* FMOV.S  FRm, [--Rn] */
                    RN(ir) -= FP_WIDTH;
                    MEM_FP_WRITE( RN(ir), FRMn(ir) );
                    break;
                case 12:/* FMOV    FRm, FRn */
		    if( IS_FPU_DOUBLESIZE() )
			DRN(ir) = DRM(ir);
		    else
			FRN(ir) = FRM(ir);
                    break;
                case 13:
                    switch( (ir&0x00F0) >> 4 ) {
		    case 0: /* FSTS    FPUL, FRn */
			FRN(ir) = FPULf;
			break;
		    case 1: /* FLDS    FRn,FPUL */
			FPULf = FRN(ir);
			break;
		    case 2: /* FLOAT   FPUL, FRn */
			DRN(ir) = (float)FPULi;
			break;
		    case 3: /* FTRC    FRn, FPUL */
			dtmp = DRN(ir);
			if( dtmp >= MAX_INTF )
			    FPULi = MAX_INT;
			else if( dtmp <= MIN_INTF )
			    FPULi = MIN_INT;
			else 
			    FPULi = (int32_t)dtmp;
			break;
		    case 4: /* FNEG    FRn */
			DRN(ir) = -DRN(ir);
			break;
		    case 5: /* FABS    FRn */
			DRN(ir) = fabs(DRN(ir));
			break;
		    case 6: /* FSQRT   FRn */
			DRN(ir) = sqrt(DRN(ir));
			break;
		    case 7: /* FSRRA FRn */
			/* NO-OP when PR=1 */
			break;
		    case 8: /* FLDI0   FRn */
			DRN(ir) = 0.0;
			break;
		    case 9: /* FLDI1   FRn */
			DRN(ir) = 1.0;
			break;
		    case 10: /* FCNVSD FPUL, DRn */
			if( ! IS_FPU_DOUBLESIZE() )
			    DRN(ir) = (double)FPULf;
			break;
		    case 11: /* FCNVDS DRn, FPUL */
			if( ! IS_FPU_DOUBLESIZE() )
			    FPULf = (float)DRN(ir);
			break;
		    case 14:/* FIPR    FVm, FVn */
			/* NO-OP when PR=1 */
			break;
		    case 15:
			if( (ir&0x0300) == 0x0100 ) { /* FTRV    XMTRX,FVn */
			    /* NO-OP when PR=1 */
			    break;
			}
			else if( (ir&0x0100) == 0 ) { /* FSCA    FPUL, DRn */	
			    /* NO-OP when PR=1 */
			    break;
			}
			else if( ir == 0xFBFD ) {
			    /* FRCHG   */
			    sh4r.fpscr ^= FPSCR_FR;
			    break;
			}
			else if( ir == 0xF3FD ) {
			    /* FSCHG   */
			    sh4r.fpscr ^= FPSCR_SZ;
			    break;
			}
		    default: UNDEF(ir);
                    }
                    break;
                case 14:/* FMAC    FR0, FRm, FRn */
                    DRN(ir) += DRM(ir)*DR0;
                    break;
                default: UNDEF(ir);
		}
	    } else { /* Single precision */
		switch( ir&0x000F ) {
                case 0: /* FADD    FRm, FRn */
                    FRN(ir) += FRM(ir);
                    break;
                case 1: /* FSUB    FRm, FRn */
                    FRN(ir) -= FRM(ir);
                    break;
                case 2: /* FMUL    FRm, FRn */
                    FRN(ir) = FRN(ir) * FRM(ir);
                    break;
                case 3: /* FDIV    FRm, FRn */
                    FRN(ir) = FRN(ir) / FRM(ir);
                    break;
                case 4: /* FCMP/EQ FRm, FRn */
                    sh4r.t = ( FRN(ir) == FRM(ir) ? 1 : 0 );
                    break;
                case 5: /* FCMP/GT FRm, FRn */
                    sh4r.t = ( FRN(ir) > FRM(ir) ? 1 : 0 );
                    break;
                case 6: /* FMOV.S  [Rm+R0], FRn */
                    MEM_FP_READ( RM(ir) + R0, FRNn(ir) );
                    break;
                case 7: /* FMOV.S  FRm, [Rn+R0] */
                    MEM_FP_WRITE( RN(ir) + R0, FRMn(ir) );
                    break;
                case 8: /* FMOV.S  [Rm], FRn */
                    MEM_FP_READ( RM(ir), FRNn(ir) );
                    break;
                case 9: /* FMOV.S  [Rm++], FRn */
                    MEM_FP_READ( RM(ir), FRNn(ir) );
                    RM(ir) += FP_WIDTH;
                    break;
                case 10:/* FMOV.S  FRm, [Rn] */
                    MEM_FP_WRITE( RN(ir), FRMn(ir) );
                    break;
                case 11:/* FMOV.S  FRm, [--Rn] */
                    RN(ir) -= FP_WIDTH;
                    MEM_FP_WRITE( RN(ir), FRMn(ir) );
                    break;
                case 12:/* FMOV    FRm, FRn */
		    if( IS_FPU_DOUBLESIZE() )
			DRN(ir) = DRM(ir);
		    else
			FRN(ir) = FRM(ir);
                    break;
                case 13:
                    switch( (ir&0x00F0) >> 4 ) {
		    case 0: /* FSTS    FPUL, FRn */
			FRN(ir) = FPULf;
			break;
		    case 1: /* FLDS    FRn,FPUL */
			FPULf = FRN(ir);
			break;
		    case 2: /* FLOAT   FPUL, FRn */
			FRN(ir) = (float)FPULi;
			break;
		    case 3: /* FTRC    FRn, FPUL */
			ftmp = FRN(ir);
			if( ftmp >= MAX_INTF )
			    FPULi = MAX_INT;
			else if( ftmp <= MIN_INTF )
			    FPULi = MIN_INT;
			else
			    FPULi = (int32_t)ftmp;
			break;
		    case 4: /* FNEG    FRn */
			FRN(ir) = -FRN(ir);
			break;
		    case 5: /* FABS    FRn */
			FRN(ir) = fabsf(FRN(ir));
			break;
		    case 6: /* FSQRT   FRn */
			FRN(ir) = sqrtf(FRN(ir));
			break;
		    case 7: /* FSRRA FRn */
			FRN(ir) = 1.0/sqrtf(FRN(ir));
			break;
		    case 8: /* FLDI0   FRn */
			FRN(ir) = 0.0;
			break;
		    case 9: /* FLDI1   FRn */
			FRN(ir) = 1.0;
			break;
		    case 10: /* FCNVSD FPUL, DRn */
			break;
		    case 11: /* FCNVDS DRn, FPUL */
			break;
		    case 14:/* FIPR    FVm, FVn */
                            /* FIXME: This is not going to be entirely accurate
                             * as the SH4 instruction is less precise. Also
                             * need to check for 0s and infinities.
                             */
                        {
                            int tmp2 = FVN(ir);
                            tmp = FVM(ir);
                            FR(tmp2+3) = FR(tmp)*FR(tmp2) +
                                FR(tmp+1)*FR(tmp2+1) +
                                FR(tmp+2)*FR(tmp2+2) +
                                FR(tmp+3)*FR(tmp2+3);
                            break;
                        }
		    case 15:
			if( (ir&0x0300) == 0x0100 ) { /* FTRV    XMTRX,FVn */
			    tmp = FVN(ir);
			    float fv[4] = { FR(tmp), FR(tmp+1), FR(tmp+2), FR(tmp+3) };
			    FR(tmp) = XF(0) * fv[0] + XF(4)*fv[1] +
				XF(8)*fv[2] + XF(12)*fv[3];
			    FR(tmp+1) = XF(1) * fv[0] + XF(5)*fv[1] +
				XF(9)*fv[2] + XF(13)*fv[3];
			    FR(tmp+2) = XF(2) * fv[0] + XF(6)*fv[1] +
				XF(10)*fv[2] + XF(14)*fv[3];
			    FR(tmp+3) = XF(3) * fv[0] + XF(7)*fv[1] +
				XF(11)*fv[2] + XF(15)*fv[3];
			    break;
			}
			else if( (ir&0x0100) == 0 ) { /* FSCA    FPUL, DRn */
			    float angle = (((float)(short)(FPULi>>16)) +
					   (((float)(FPULi&0xFFFF))/65536.0)) *
				2 * M_PI;
			    int reg = FRNn(ir);
			    FR(reg) = sinf(angle);
			    FR(reg+1) = cosf(angle);
			    break;
			}
			else if( ir == 0xFBFD ) {
			    /* FRCHG   */
			    sh4r.fpscr ^= FPSCR_FR;
			    break;
			}
			else if( ir == 0xF3FD ) {
			    /* FSCHG   */
			    sh4r.fpscr ^= FPSCR_SZ;
			    break;
			}
		    default: UNDEF(ir);
                    }
                    break;
                case 14:/* FMAC    FR0, FRm, FRn */
                    FRN(ir) += FRM(ir)*FR0;
                    break;
                default: UNDEF(ir);
		}
	    }
	    break;
    }
    sh4r.pc = sh4r.new_pc;
    sh4r.new_pc += 2;
    sh4r.in_delay_slot = 0;
    return TRUE;
}
