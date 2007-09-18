/**
 * $Id: sh4x86.c,v 1.10 2007-09-18 08:59:00 nkeynes Exp $
 * 
 * SH4 => x86 translation. This version does no real optimization, it just
 * outputs straight-line x86 code - it mainly exists to provide a baseline
 * to test the optimizing versions against.
 *
 * Copyright (c) 2007 Nathan Keynes.
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
#include <math.h>

#ifndef NDEBUG
#define DEBUG_JUMPS 1
#endif

#include "sh4/sh4core.h"
#include "sh4/sh4trans.h"
#include "sh4/sh4mmio.h"
#include "sh4/x86op.h"
#include "clock.h"

#define DEFAULT_BACKPATCH_SIZE 4096

/** 
 * Struct to manage internal translation state. This state is not saved -
 * it is only valid between calls to sh4_translate_begin_block() and
 * sh4_translate_end_block()
 */
struct sh4_x86_state {
    gboolean in_delay_slot;
    gboolean priv_checked; /* true if we've already checked the cpu mode. */
    gboolean fpuen_checked; /* true if we've already checked fpu enabled. */
    int exit_code;

    /* Allocated memory for the (block-wide) back-patch list */
    uint32_t **backpatch_list;
    uint32_t backpatch_posn;
    uint32_t backpatch_size;
};

#define EXIT_DATA_ADDR_READ 0
#define EXIT_DATA_ADDR_WRITE 7
#define EXIT_ILLEGAL 14
#define EXIT_SLOT_ILLEGAL 21
#define EXIT_FPU_DISABLED 28
#define EXIT_SLOT_FPU_DISABLED 35

static struct sh4_x86_state sh4_x86;

static uint32_t max_int = 0x7FFFFFFF;
static uint32_t min_int = 0x80000000;
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

void sh4_sleep()
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



void sh4_x86_init()
{
    sh4_x86.backpatch_list = malloc(DEFAULT_BACKPATCH_SIZE);
    sh4_x86.backpatch_size = DEFAULT_BACKPATCH_SIZE / sizeof(uint32_t *);
}


static void sh4_x86_add_backpatch( uint8_t *ptr )
{
    if( sh4_x86.backpatch_posn == sh4_x86.backpatch_size ) {
	sh4_x86.backpatch_size <<= 1;
	sh4_x86.backpatch_list = realloc( sh4_x86.backpatch_list, sh4_x86.backpatch_size * sizeof(uint32_t *) );
	assert( sh4_x86.backpatch_list != NULL );
    }
    sh4_x86.backpatch_list[sh4_x86.backpatch_posn++] = (uint32_t *)ptr;
}

static void sh4_x86_do_backpatch( uint8_t *reloc_base )
{
    unsigned int i;
    for( i=0; i<sh4_x86.backpatch_posn; i++ ) {
	*sh4_x86.backpatch_list[i] += (reloc_base - ((uint8_t *)sh4_x86.backpatch_list[i]) - 4);
    }
}

/**
 * Emit an instruction to load an SH4 reg into a real register
 */
static inline void load_reg( int x86reg, int sh4reg ) 
{
    /* mov [bp+n], reg */
    OP(0x8B);
    OP(0x45 + (x86reg<<3));
    OP(REG_OFFSET(r[sh4reg]));
}

static inline void load_reg16s( int x86reg, int sh4reg )
{
    OP(0x0F);
    OP(0xBF);
    MODRM_r32_sh4r(x86reg, REG_OFFSET(r[sh4reg]));
}

static inline void load_reg16u( int x86reg, int sh4reg )
{
    OP(0x0F);
    OP(0xB7);
    MODRM_r32_sh4r(x86reg, REG_OFFSET(r[sh4reg]));

}

#define load_spreg( x86reg, regoff ) MOV_sh4r_r32( regoff, x86reg )
#define store_spreg( x86reg, regoff ) MOV_r32_sh4r( x86reg, regoff )
/**
 * Emit an instruction to load an immediate value into a register
 */
static inline void load_imm32( int x86reg, uint32_t value ) {
    /* mov #value, reg */
    OP(0xB8 + x86reg);
    OP32(value);
}

/**
 * Emit an instruction to store an SH4 reg (RN)
 */
void static inline store_reg( int x86reg, int sh4reg ) {
    /* mov reg, [bp+n] */
    OP(0x89);
    OP(0x45 + (x86reg<<3));
    OP(REG_OFFSET(r[sh4reg]));
}

#define load_fr_bank(bankreg) load_spreg( bankreg, REG_OFFSET(fr_bank))

/**
 * Load an FR register (single-precision floating point) into an integer x86
 * register (eg for register-to-register moves)
 */
void static inline load_fr( int bankreg, int x86reg, int frm )
{
    OP(0x8B); OP(0x40+bankreg+(x86reg<<3)); OP((frm^1)<<2);
}

/**
 * Store an FR register (single-precision floating point) into an integer x86
 * register (eg for register-to-register moves)
 */
void static inline store_fr( int bankreg, int x86reg, int frn )
{
    OP(0x89);  OP(0x40+bankreg+(x86reg<<3)); OP((frn^1)<<2);
}


/**
 * Load a pointer to the back fp back into the specified x86 register. The
 * bankreg must have been previously loaded with FPSCR.
 * NB: 12 bytes
 */
static inline void load_xf_bank( int bankreg )
{
    NOT_r32( bankreg );
    SHR_imm8_r32( (21 - 6), bankreg ); // Extract bit 21 then *64 for bank size
    AND_imm8s_r32( 0x40, bankreg );    // Complete extraction
    OP(0x8D); OP(0x44+(bankreg<<3)); OP(0x28+bankreg); OP(REG_OFFSET(fr)); // LEA [ebp+bankreg+disp], bankreg
}

/**
 * Update the fr_bank pointer based on the current fpscr value.
 */
static inline void update_fr_bank( int fpscrreg )
{
    SHR_imm8_r32( (21 - 6), fpscrreg ); // Extract bit 21 then *64 for bank size
    AND_imm8s_r32( 0x40, fpscrreg );    // Complete extraction
    OP(0x8D); OP(0x44+(fpscrreg<<3)); OP(0x28+fpscrreg); OP(REG_OFFSET(fr)); // LEA [ebp+fpscrreg+disp], fpscrreg
    store_spreg( fpscrreg, REG_OFFSET(fr_bank) );
}
/**
 * Push FPUL (as a 32-bit float) onto the FPU stack
 */
static inline void push_fpul( )
{
    OP(0xD9); OP(0x45); OP(R_FPUL);
}

/**
 * Pop FPUL (as a 32-bit float) from the FPU stack
 */
static inline void pop_fpul( )
{
    OP(0xD9); OP(0x5D); OP(R_FPUL);
}

/**
 * Push a 32-bit float onto the FPU stack, with bankreg previously loaded
 * with the location of the current fp bank.
 */
static inline void push_fr( int bankreg, int frm ) 
{
    OP(0xD9); OP(0x40 + bankreg); OP((frm^1)<<2);  // FLD.S [bankreg + frm^1*4]
}

/**
 * Pop a 32-bit float from the FPU stack and store it back into the fp bank, 
 * with bankreg previously loaded with the location of the current fp bank.
 */
static inline void pop_fr( int bankreg, int frm )
{
    OP(0xD9); OP(0x58 + bankreg); OP((frm^1)<<2); // FST.S [bankreg + frm^1*4]
}

/**
 * Push a 64-bit double onto the FPU stack, with bankreg previously loaded
 * with the location of the current fp bank.
 */
static inline void push_dr( int bankreg, int frm )
{
    OP(0xDD); OP(0x40 + bankreg); OP(frm<<2); // FLD.D [bankreg + frm*4]
}

static inline void pop_dr( int bankreg, int frm )
{
    OP(0xDD); OP(0x58 + bankreg); OP(frm<<2); // FST.D [bankreg + frm*4]
}

/**
 * Note: clobbers EAX to make the indirect call - this isn't usually
 * a problem since the callee will usually clobber it anyway.
 */
static inline void call_func0( void *ptr )
{
    load_imm32(R_EAX, (uint32_t)ptr);
    CALL_r32(R_EAX);
}

static inline void call_func1( void *ptr, int arg1 )
{
    PUSH_r32(arg1);
    call_func0(ptr);
    ADD_imm8s_r32( 4, R_ESP );
}

static inline void call_func2( void *ptr, int arg1, int arg2 )
{
    PUSH_r32(arg2);
    PUSH_r32(arg1);
    call_func0(ptr);
    ADD_imm8s_r32( 8, R_ESP );
}

/**
 * Write a double (64-bit) value into memory, with the first word in arg2a, and
 * the second in arg2b
 * NB: 30 bytes
 */
static inline void MEM_WRITE_DOUBLE( int addr, int arg2a, int arg2b )
{
    ADD_imm8s_r32( 4, addr );
    PUSH_r32(arg2b);
    PUSH_r32(addr);
    ADD_imm8s_r32( -4, addr );
    PUSH_r32(arg2a);
    PUSH_r32(addr);
    call_func0(sh4_write_long);
    ADD_imm8s_r32( 8, R_ESP );
    call_func0(sh4_write_long);
    ADD_imm8s_r32( 8, R_ESP );
}

/**
 * Read a double (64-bit) value from memory, writing the first word into arg2a
 * and the second into arg2b. The addr must not be in EAX
 * NB: 27 bytes
 */
static inline void MEM_READ_DOUBLE( int addr, int arg2a, int arg2b )
{
    PUSH_r32(addr);
    call_func0(sh4_read_long);
    POP_r32(addr);
    PUSH_r32(R_EAX);
    ADD_imm8s_r32( 4, addr );
    PUSH_r32(addr);
    call_func0(sh4_read_long);
    ADD_imm8s_r32( 4, R_ESP );
    MOV_r32_r32( R_EAX, arg2b );
    POP_r32(arg2a);
}

/* Exception checks - Note that all exception checks will clobber EAX */
static void check_priv( )
{
    if( !sh4_x86.priv_checked ) {
	sh4_x86.priv_checked = TRUE;
	load_spreg( R_EAX, R_SR );
	AND_imm32_r32( SR_MD, R_EAX );
	if( sh4_x86.in_delay_slot ) {
	    JE_exit( EXIT_SLOT_ILLEGAL );
	} else {
	    JE_exit( EXIT_ILLEGAL );
	}
    }
}

static void check_fpuen( )
{
    if( !sh4_x86.fpuen_checked ) {
	sh4_x86.fpuen_checked = TRUE;
	load_spreg( R_EAX, R_SR );
	AND_imm32_r32( SR_FD, R_EAX );
	if( sh4_x86.in_delay_slot ) {
	    JNE_exit(EXIT_SLOT_FPU_DISABLED);
	} else {
	    JNE_exit(EXIT_FPU_DISABLED);
	}
    }
}

static void check_ralign16( int x86reg )
{
    TEST_imm32_r32( 0x00000001, x86reg );
    JNE_exit(EXIT_DATA_ADDR_READ);
}

static void check_walign16( int x86reg )
{
    TEST_imm32_r32( 0x00000001, x86reg );
    JNE_exit(EXIT_DATA_ADDR_WRITE);
}

static void check_ralign32( int x86reg )
{
    TEST_imm32_r32( 0x00000003, x86reg );
    JNE_exit(EXIT_DATA_ADDR_READ);
}
static void check_walign32( int x86reg )
{
    TEST_imm32_r32( 0x00000003, x86reg );
    JNE_exit(EXIT_DATA_ADDR_WRITE);
}

#define UNDEF()
#define MEM_RESULT(value_reg) if(value_reg != R_EAX) { MOV_r32_r32(R_EAX,value_reg); }
#define MEM_READ_BYTE( addr_reg, value_reg ) call_func1(sh4_read_byte, addr_reg ); MEM_RESULT(value_reg)
#define MEM_READ_WORD( addr_reg, value_reg ) call_func1(sh4_read_word, addr_reg ); MEM_RESULT(value_reg)
#define MEM_READ_LONG( addr_reg, value_reg ) call_func1(sh4_read_long, addr_reg ); MEM_RESULT(value_reg)
#define MEM_WRITE_BYTE( addr_reg, value_reg ) call_func2(sh4_write_byte, addr_reg, value_reg)
#define MEM_WRITE_WORD( addr_reg, value_reg ) call_func2(sh4_write_word, addr_reg, value_reg)
#define MEM_WRITE_LONG( addr_reg, value_reg ) call_func2(sh4_write_long, addr_reg, value_reg)

#define SLOTILLEGAL() JMP_exit(EXIT_SLOT_ILLEGAL); sh4_x86.in_delay_slot = FALSE; return 1;



/**
 * Emit the 'start of block' assembly. Sets up the stack frame and save
 * SI/DI as required
 */
void sh4_translate_begin_block() 
{
    PUSH_r32(R_EBP);
    /* mov &sh4r, ebp */
    load_imm32( R_EBP, (uint32_t)&sh4r );
    PUSH_r32(R_EDI);
    PUSH_r32(R_ESI);
    XOR_r32_r32(R_ESI, R_ESI);
    
    sh4_x86.in_delay_slot = FALSE;
    sh4_x86.priv_checked = FALSE;
    sh4_x86.fpuen_checked = FALSE;
    sh4_x86.backpatch_posn = 0;
    sh4_x86.exit_code = 1;
}

/**
 * Exit the block early (ie branch out), conditionally or otherwise
 */
void exit_block( )
{
    store_spreg( R_EDI, REG_OFFSET(pc) );
    MOV_moff32_EAX( (uint32_t)&sh4_cpu_period );
    load_spreg( R_ECX, REG_OFFSET(slice_cycle) );
    MUL_r32( R_ESI );
    ADD_r32_r32( R_EAX, R_ECX );
    store_spreg( R_ECX, REG_OFFSET(slice_cycle) );
    load_imm32( R_EAX, sh4_x86.exit_code );
    POP_r32(R_ESI);
    POP_r32(R_EDI);
    POP_r32(R_EBP);
    RET();
}

/**
 * Flush any open regs back to memory, restore SI/DI/, update PC, etc
 */
void sh4_translate_end_block( sh4addr_t pc ) {
    assert( !sh4_x86.in_delay_slot ); // should never stop here
    // Normal termination - save PC, cycle count
    exit_block( );

    if( sh4_x86.backpatch_posn != 0 ) {
	uint8_t *end_ptr = xlat_output;
	// Exception termination. Jump block for various exception codes:
	PUSH_imm32( EXC_DATA_ADDR_READ );
	JMP_rel8( 33, target1 );
	PUSH_imm32( EXC_DATA_ADDR_WRITE );
	JMP_rel8( 26, target2 );
	PUSH_imm32( EXC_ILLEGAL );
	JMP_rel8( 19, target3 );
	PUSH_imm32( EXC_SLOT_ILLEGAL ); 
	JMP_rel8( 12, target4 );
	PUSH_imm32( EXC_FPU_DISABLED ); 
	JMP_rel8( 5, target5 );
	PUSH_imm32( EXC_SLOT_FPU_DISABLED );
	// target
	JMP_TARGET(target1);
	JMP_TARGET(target2);
	JMP_TARGET(target3);
	JMP_TARGET(target4);
	JMP_TARGET(target5);
	load_spreg( R_ECX, REG_OFFSET(pc) );
	ADD_r32_r32( R_ESI, R_ECX );
	ADD_r32_r32( R_ESI, R_ECX );
	store_spreg( R_ECX, REG_OFFSET(pc) );
	MOV_moff32_EAX( (uint32_t)&sh4_cpu_period );
	load_spreg( R_ECX, REG_OFFSET(slice_cycle) );
	MUL_r32( R_ESI );
	ADD_r32_r32( R_EAX, R_ECX );
	store_spreg( R_ECX, REG_OFFSET(slice_cycle) );
	
	load_imm32( R_EAX, (uint32_t)sh4_raise_exception ); // 6
	CALL_r32( R_EAX ); // 2
	ADD_imm8s_r32( 4, R_ESP );
	POP_r32(R_ESI);
	POP_r32(R_EDI);
	POP_r32(R_EBP);
	RET();

	sh4_x86_do_backpatch( end_ptr );
    }

}


extern uint16_t *sh4_icache;
extern uint32_t sh4_icache_addr;

/**
 * Translate a single instruction. Delayed branches are handled specially
 * by translating both branch and delayed instruction as a single unit (as
 * 
 *
 * @return true if the instruction marks the end of a basic block
 * (eg a branch or 
 */
uint32_t sh4_x86_translate_instruction( uint32_t pc )
{
    uint32_t ir;
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
	    ir = sh4_read_word(pc);
	} else {
	    sh4_icache_addr = pageaddr;
	    ir = sh4_icache[(pc&0xFFF)>>1];
	}
    }

        switch( (ir&0xF000) >> 12 ) {
            case 0x0:
                switch( ir&0xF ) {
                    case 0x2:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* STC SR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        check_priv();
                                        call_func0(sh4_read_sr);
                                        store_reg( R_EAX, Rn );
                                        }
                                        break;
                                    case 0x1:
                                        { /* STC GBR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        load_spreg( R_EAX, R_GBR );
                                        store_reg( R_EAX, Rn );
                                        }
                                        break;
                                    case 0x2:
                                        { /* STC VBR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_spreg( R_EAX, R_VBR );
                                        store_reg( R_EAX, Rn );
                                        }
                                        break;
                                    case 0x3:
                                        { /* STC SSR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_spreg( R_EAX, R_SSR );
                                        store_reg( R_EAX, Rn );
                                        }
                                        break;
                                    case 0x4:
                                        { /* STC SPC, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_spreg( R_EAX, R_SPC );
                                        store_reg( R_EAX, Rn );
                                        }
                                        break;
                                    default:
                                        UNDEF();
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* STC Rm_BANK, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm_BANK = ((ir>>4)&0x7); 
                                check_priv();
                                load_spreg( R_EAX, REG_OFFSET(r_bank[Rm_BANK]) );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                        }
                        break;
                    case 0x3:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* BSRF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                if( sh4_x86.in_delay_slot ) {
                            	SLOTILLEGAL();
                                } else {
                            	load_imm32( R_EAX, pc + 4 );
                            	store_spreg( R_EAX, R_PR );
                            	load_reg( R_EDI, Rn );
                            	ADD_r32_r32( R_EAX, R_EDI );
                            	sh4_x86.in_delay_slot = TRUE;
                            	return 0;
                                }
                                }
                                break;
                            case 0x2:
                                { /* BRAF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                if( sh4_x86.in_delay_slot ) {
                            	SLOTILLEGAL();
                                } else {
                            	load_reg( R_EDI, Rn );
                            	ADD_imm32_r32( pc + 4, R_EDI );
                            	sh4_x86.in_delay_slot = TRUE;
                            	return 0;
                                }
                                }
                                break;
                            case 0x8:
                                { /* PREF @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                PUSH_r32( R_EAX );
                                AND_imm32_r32( 0xFC000000, R_EAX );
                                CMP_imm32_r32( 0xE0000000, R_EAX );
                                JNE_rel8(7, end);
                                call_func0( sh4_flush_store_queue );
                                JMP_TARGET(end);
                                ADD_imm8s_r32( 4, R_ESP );
                                }
                                break;
                            case 0x9:
                                { /* OCBI @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xA:
                                { /* OCBP @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xB:
                                { /* OCBWB @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xC:
                                { /* MOVCA.L R0, @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, 0 );
                                load_reg( R_ECX, Rn );
                                check_walign32( R_ECX );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x4:
                        { /* MOV.B Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, 0 );
                        load_reg( R_ECX, Rn );
                        ADD_r32_r32( R_EAX, R_ECX );
                        load_reg( R_EAX, Rm );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, 0 );
                        load_reg( R_ECX, Rn );
                        ADD_r32_r32( R_EAX, R_ECX );
                        check_walign16( R_ECX );
                        load_reg( R_EAX, Rm );
                        MEM_WRITE_WORD( R_ECX, R_EAX );
                        }
                        break;
                    case 0x6:
                        { /* MOV.L Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, 0 );
                        load_reg( R_ECX, Rn );
                        ADD_r32_r32( R_EAX, R_ECX );
                        check_walign32( R_ECX );
                        load_reg( R_EAX, Rm );
                        MEM_WRITE_LONG( R_ECX, R_EAX );
                        }
                        break;
                    case 0x7:
                        { /* MUL.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        MUL_r32( R_ECX );
                        store_spreg( R_EAX, R_MACL );
                        }
                        break;
                    case 0x8:
                        switch( (ir&0xFF0) >> 4 ) {
                            case 0x0:
                                { /* CLRT */
                                CLC();
                                SETC_t();
                                }
                                break;
                            case 0x1:
                                { /* SETT */
                                STC();
                                SETC_t();
                                }
                                break;
                            case 0x2:
                                { /* CLRMAC */
                                XOR_r32_r32(R_EAX, R_EAX);
                                store_spreg( R_EAX, R_MACL );
                                store_spreg( R_EAX, R_MACH );
                                }
                                break;
                            case 0x3:
                                { /* LDTLB */
                                }
                                break;
                            case 0x4:
                                { /* CLRS */
                                CLC();
                                SETC_sh4r(R_S);
                                }
                                break;
                            case 0x5:
                                { /* SETS */
                                STC();
                                SETC_sh4r(R_S);
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x9:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* NOP */
                                /* Do nothing. Well, we could emit an 0x90, but what would really be the point? */
                                }
                                break;
                            case 0x1:
                                { /* DIV0U */
                                XOR_r32_r32( R_EAX, R_EAX );
                                store_spreg( R_EAX, R_Q );
                                store_spreg( R_EAX, R_M );
                                store_spreg( R_EAX, R_T );
                                }
                                break;
                            case 0x2:
                                { /* MOVT Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_T );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xA:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* STS MACH, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_MACH );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x1:
                                { /* STS MACL, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_MACL );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x2:
                                { /* STS PR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_PR );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x3:
                                { /* STC SGR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                check_priv();
                                load_spreg( R_EAX, R_SGR );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x5:
                                { /* STS FPUL, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_FPUL );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x6:
                                { /* STS FPSCR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_FPSCR );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0xF:
                                { /* STC DBR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                check_priv();
                                load_spreg( R_EAX, R_DBR );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xB:
                        switch( (ir&0xFF0) >> 4 ) {
                            case 0x0:
                                { /* RTS */
                                if( sh4_x86.in_delay_slot ) {
                            	SLOTILLEGAL();
                                } else {
                            	load_spreg( R_EDI, R_PR );
                            	sh4_x86.in_delay_slot = TRUE;
                            	return 0;
                                }
                                }
                                break;
                            case 0x1:
                                { /* SLEEP */
                                check_priv();
                                call_func0( sh4_sleep );
                                sh4_x86.exit_code = 0;
                                sh4_x86.in_delay_slot = FALSE;
                                return 1;
                                }
                                break;
                            case 0x2:
                                { /* RTE */
                                check_priv();
                                if( sh4_x86.in_delay_slot ) {
                            	SLOTILLEGAL();
                                } else {
                            	load_spreg( R_EDI, R_SPC );
                            	load_spreg( R_EAX, R_SSR );
                            	call_func1( sh4_write_sr, R_EAX );
                            	sh4_x86.in_delay_slot = TRUE;
                            	sh4_x86.priv_checked = FALSE;
                            	sh4_x86.fpuen_checked = FALSE;
                            	return 0;
                                }
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xC:
                        { /* MOV.B @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, 0 );
                        load_reg( R_ECX, Rm );
                        ADD_r32_r32( R_EAX, R_ECX );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xD:
                        { /* MOV.W @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, 0 );
                        load_reg( R_ECX, Rm );
                        ADD_r32_r32( R_EAX, R_ECX );
                        check_ralign16( R_ECX );
                        MEM_READ_WORD( R_ECX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xE:
                        { /* MOV.L @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, 0 );
                        load_reg( R_ECX, Rm );
                        ADD_r32_r32( R_EAX, R_ECX );
                        check_ralign32( R_ECX );
                        MEM_READ_LONG( R_ECX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xF:
                        { /* MAC.L @Rm+, @Rn+ */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_ECX, Rm );
                        check_ralign32( R_ECX );
                        load_reg( R_ECX, Rn );
                        check_ralign32( R_ECX );
                        ADD_imm8s_sh4r( 4, REG_OFFSET(r[Rn]) );
                        MEM_READ_LONG( R_ECX, R_EAX );
                        PUSH_r32( R_EAX );
                        load_reg( R_ECX, Rm );
                        ADD_imm8s_sh4r( 4, REG_OFFSET(r[Rm]) );
                        MEM_READ_LONG( R_ECX, R_EAX );
                        POP_r32( R_ECX );
                        IMUL_r32( R_ECX );
                        ADD_r32_sh4r( R_EAX, R_MACL );
                        ADC_r32_sh4r( R_EDX, R_MACH );
                    
                        load_spreg( R_ECX, R_S );
                        TEST_r32_r32(R_ECX, R_ECX);
                        JE_rel8( 7, nosat );
                        call_func0( signsat48 );
                        JMP_TARGET( nosat );
                        }
                        break;
                    default:
                        UNDEF();
                        break;
                }
                break;
            case 0x1:
                { /* MOV.L Rm, @(disp, Rn) */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<2; 
                load_reg( R_ECX, Rn );
                load_reg( R_EAX, Rm );
                ADD_imm32_r32( disp, R_ECX );
                check_walign32( R_ECX );
                MEM_WRITE_LONG( R_ECX, R_EAX );
                }
                break;
            case 0x2:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* MOV.B Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_ECX, Rn );
                        check_walign16( R_ECX );
                        load_reg( R_EAX, Rm );
                        MEM_WRITE_WORD( R_ECX, R_EAX );
                        }
                        break;
                    case 0x2:
                        { /* MOV.L Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        check_walign32(R_ECX);
                        MEM_WRITE_LONG( R_ECX, R_EAX );
                        }
                        break;
                    case 0x4:
                        { /* MOV.B Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        ADD_imm8s_r32( -1, R_ECX );
                        store_reg( R_ECX, Rn );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_ECX, Rn );
                        check_walign16( R_ECX );
                        load_reg( R_EAX, Rm );
                        ADD_imm8s_r32( -2, R_ECX );
                        store_reg( R_ECX, Rn );
                        MEM_WRITE_WORD( R_ECX, R_EAX );
                        }
                        break;
                    case 0x6:
                        { /* MOV.L Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        check_walign32( R_ECX );
                        ADD_imm8s_r32( -4, R_ECX );
                        store_reg( R_ECX, Rn );
                        MEM_WRITE_LONG( R_ECX, R_EAX );
                        }
                        break;
                    case 0x7:
                        { /* DIV0S Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        SHR_imm8_r32( 31, R_EAX );
                        SHR_imm8_r32( 31, R_ECX );
                        store_spreg( R_EAX, R_M );
                        store_spreg( R_ECX, R_Q );
                        CMP_r32_r32( R_EAX, R_ECX );
                        SETNE_t();
                        }
                        break;
                    case 0x8:
                        { /* TST Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        TEST_r32_r32( R_EAX, R_ECX );
                        SETE_t();
                        }
                        break;
                    case 0x9:
                        { /* AND Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        AND_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xA:
                        { /* XOR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        XOR_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xB:
                        { /* OR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        OR_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xC:
                        { /* CMP/STR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        XOR_r32_r32( R_ECX, R_EAX );
                        TEST_r8_r8( R_AL, R_AL );
                        JE_rel8(13, target1);
                        TEST_r8_r8( R_AH, R_AH ); // 2
                        JE_rel8(9, target2);
                        SHR_imm8_r32( 16, R_EAX ); // 3
                        TEST_r8_r8( R_AL, R_AL ); // 2
                        JE_rel8(2, target3);
                        TEST_r8_r8( R_AH, R_AH ); // 2
                        JMP_TARGET(target1);
                        JMP_TARGET(target2);
                        JMP_TARGET(target3);
                        SETE_t();
                        }
                        break;
                    case 0xD:
                        { /* XTRCT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        MOV_r32_r32( R_EAX, R_ECX );
                        SHR_imm8_r32( 16, R_EAX );
                        SHL_imm8_r32( 16, R_ECX );
                        OR_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xE:
                        { /* MULU.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg16u( R_EAX, Rm );
                        load_reg16u( R_ECX, Rn );
                        MUL_r32( R_ECX );
                        store_spreg( R_EAX, R_MACL );
                        }
                        break;
                    case 0xF:
                        { /* MULS.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg16s( R_EAX, Rm );
                        load_reg16s( R_ECX, Rn );
                        MUL_r32( R_ECX );
                        store_spreg( R_EAX, R_MACL );
                        }
                        break;
                    default:
                        UNDEF();
                        break;
                }
                break;
            case 0x3:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* CMP/EQ Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        CMP_r32_r32( R_EAX, R_ECX );
                        SETE_t();
                        }
                        break;
                    case 0x2:
                        { /* CMP/HS Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        CMP_r32_r32( R_EAX, R_ECX );
                        SETAE_t();
                        }
                        break;
                    case 0x3:
                        { /* CMP/GE Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        CMP_r32_r32( R_EAX, R_ECX );
                        SETGE_t();
                        }
                        break;
                    case 0x4:
                        { /* DIV1 Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_spreg( R_ECX, R_M );
                        load_reg( R_EAX, Rn );
                        LDC_t();
                        RCL1_r32( R_EAX );
                        SETC_r8( R_DL ); // Q'
                        CMP_sh4r_r32( R_Q, R_ECX );
                        JE_rel8(5, mqequal);
                        ADD_sh4r_r32( REG_OFFSET(r[Rm]), R_EAX );
                        JMP_rel8(3, end);
                        JMP_TARGET(mqequal);
                        SUB_sh4r_r32( REG_OFFSET(r[Rm]), R_EAX );
                        JMP_TARGET(end);
                        store_reg( R_EAX, Rn ); // Done with Rn now
                        SETC_r8(R_AL); // tmp1
                        XOR_r8_r8( R_DL, R_AL ); // Q' = Q ^ tmp1
                        XOR_r8_r8( R_AL, R_CL ); // Q'' = Q' ^ M
                        store_spreg( R_ECX, R_Q );
                        XOR_imm8s_r32( 1, R_AL );   // T = !Q'
                        MOVZX_r8_r32( R_AL, R_EAX );
                        store_spreg( R_EAX, R_T );
                        }
                        break;
                    case 0x5:
                        { /* DMULU.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        MUL_r32(R_ECX);
                        store_spreg( R_EDX, R_MACH );
                        store_spreg( R_EAX, R_MACL );
                        }
                        break;
                    case 0x6:
                        { /* CMP/HI Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        CMP_r32_r32( R_EAX, R_ECX );
                        SETA_t();
                        }
                        break;
                    case 0x7:
                        { /* CMP/GT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        CMP_r32_r32( R_EAX, R_ECX );
                        SETG_t();
                        }
                        break;
                    case 0x8:
                        { /* SUB Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        SUB_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xA:
                        { /* SUBC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        LDC_t();
                        SBB_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xB:
                        { /* SUBV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        SUB_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        SETO_t();
                        }
                        break;
                    case 0xC:
                        { /* ADD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        ADD_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xD:
                        { /* DMULS.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        IMUL_r32(R_ECX);
                        store_spreg( R_EDX, R_MACH );
                        store_spreg( R_EAX, R_MACL );
                        }
                        break;
                    case 0xE:
                        { /* ADDC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        LDC_t();
                        ADC_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        SETC_t();
                        }
                        break;
                    case 0xF:
                        { /* ADDV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        ADD_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        SETO_t();
                        }
                        break;
                    default:
                        UNDEF();
                        break;
                }
                break;
            case 0x4:
                switch( ir&0xF ) {
                    case 0x0:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHL1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x1:
                                { /* DT Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                ADD_imm8s_r32( -1, R_EAX );
                                store_reg( R_EAX, Rn );
                                SETE_t();
                                }
                                break;
                            case 0x2:
                                { /* SHAL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHL1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x1:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHR1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x1:
                                { /* CMP/PZ Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                CMP_imm8s_r32( 0, R_EAX );
                                SETGE_t();
                                }
                                break;
                            case 0x2:
                                { /* SHAR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SAR1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x2:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* STS.L MACH, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, R_ECX );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_MACH );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0x1:
                                { /* STS.L MACL, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, R_ECX );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_MACL );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0x2:
                                { /* STS.L PR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, R_ECX );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_PR );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0x3:
                                { /* STC.L SGR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                check_priv();
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, R_ECX );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_SGR );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0x5:
                                { /* STS.L FPUL, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, R_ECX );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_FPUL );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0x6:
                                { /* STS.L FPSCR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, R_ECX );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_FPSCR );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0xF:
                                { /* STC.L DBR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                check_priv();
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, R_ECX );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_DBR );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x3:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* STC.L SR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_reg( R_ECX, Rn );
                                        ADD_imm8s_r32( -4, R_ECX );
                                        store_reg( R_ECX, Rn );
                                        call_func0( sh4_read_sr );
                                        MEM_WRITE_LONG( R_ECX, R_EAX );
                                        }
                                        break;
                                    case 0x1:
                                        { /* STC.L GBR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        load_reg( R_ECX, Rn );
                                        ADD_imm8s_r32( -4, R_ECX );
                                        store_reg( R_ECX, Rn );
                                        load_spreg( R_EAX, R_GBR );
                                        MEM_WRITE_LONG( R_ECX, R_EAX );
                                        }
                                        break;
                                    case 0x2:
                                        { /* STC.L VBR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_reg( R_ECX, Rn );
                                        ADD_imm8s_r32( -4, R_ECX );
                                        store_reg( R_ECX, Rn );
                                        load_spreg( R_EAX, R_VBR );
                                        MEM_WRITE_LONG( R_ECX, R_EAX );
                                        }
                                        break;
                                    case 0x3:
                                        { /* STC.L SSR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_reg( R_ECX, Rn );
                                        ADD_imm8s_r32( -4, R_ECX );
                                        store_reg( R_ECX, Rn );
                                        load_spreg( R_EAX, R_SSR );
                                        MEM_WRITE_LONG( R_ECX, R_EAX );
                                        }
                                        break;
                                    case 0x4:
                                        { /* STC.L SPC, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_reg( R_ECX, Rn );
                                        ADD_imm8s_r32( -4, R_ECX );
                                        store_reg( R_ECX, Rn );
                                        load_spreg( R_EAX, R_SPC );
                                        MEM_WRITE_LONG( R_ECX, R_EAX );
                                        }
                                        break;
                                    default:
                                        UNDEF();
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* STC.L Rm_BANK, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm_BANK = ((ir>>4)&0x7); 
                                check_priv();
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, R_ECX );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, REG_OFFSET(r_bank[Rm_BANK]) );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                        }
                        break;
                    case 0x4:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* ROTL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                ROL1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                SETC_t();
                                }
                                break;
                            case 0x2:
                                { /* ROTCL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                LDC_t();
                                RCL1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                SETC_t();
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x5:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* ROTR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                ROR1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                SETC_t();
                                }
                                break;
                            case 0x1:
                                { /* CMP/PL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                CMP_imm8s_r32( 0, R_EAX );
                                SETG_t();
                                }
                                break;
                            case 0x2:
                                { /* ROTCR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                LDC_t();
                                RCR1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                SETC_t();
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x6:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* LDS.L @Rm+, MACH */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_MACH );
                                }
                                break;
                            case 0x1:
                                { /* LDS.L @Rm+, MACL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_MACL );
                                }
                                break;
                            case 0x2:
                                { /* LDS.L @Rm+, PR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_PR );
                                }
                                break;
                            case 0x3:
                                { /* LDC.L @Rm+, SGR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                check_priv();
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_SGR );
                                }
                                break;
                            case 0x5:
                                { /* LDS.L @Rm+, FPUL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_FPUL );
                                }
                                break;
                            case 0x6:
                                { /* LDS.L @Rm+, FPSCR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_FPSCR );
                                update_fr_bank( R_EAX );
                                }
                                break;
                            case 0xF:
                                { /* LDC.L @Rm+, DBR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                check_priv();
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_DBR );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x7:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* LDC.L @Rm+, SR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        if( sh4_x86.in_delay_slot ) {
                                    	SLOTILLEGAL();
                                        } else {
                                    	check_priv();
                                    	load_reg( R_EAX, Rm );
                                    	MOV_r32_r32( R_EAX, R_ECX );
                                    	ADD_imm8s_r32( 4, R_EAX );
                                    	store_reg( R_EAX, Rm );
                                    	MEM_READ_LONG( R_ECX, R_EAX );
                                    	call_func1( sh4_write_sr, R_EAX );
                                    	sh4_x86.priv_checked = FALSE;
                                    	sh4_x86.fpuen_checked = FALSE;
                                        }
                                        }
                                        break;
                                    case 0x1:
                                        { /* LDC.L @Rm+, GBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        load_reg( R_EAX, Rm );
                                        MOV_r32_r32( R_EAX, R_ECX );
                                        ADD_imm8s_r32( 4, R_EAX );
                                        store_reg( R_EAX, Rm );
                                        MEM_READ_LONG( R_ECX, R_EAX );
                                        store_spreg( R_EAX, R_GBR );
                                        }
                                        break;
                                    case 0x2:
                                        { /* LDC.L @Rm+, VBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_reg( R_EAX, Rm );
                                        MOV_r32_r32( R_EAX, R_ECX );
                                        ADD_imm8s_r32( 4, R_EAX );
                                        store_reg( R_EAX, Rm );
                                        MEM_READ_LONG( R_ECX, R_EAX );
                                        store_spreg( R_EAX, R_VBR );
                                        }
                                        break;
                                    case 0x3:
                                        { /* LDC.L @Rm+, SSR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_reg( R_EAX, Rm );
                                        MOV_r32_r32( R_EAX, R_ECX );
                                        ADD_imm8s_r32( 4, R_EAX );
                                        store_reg( R_EAX, Rm );
                                        MEM_READ_LONG( R_ECX, R_EAX );
                                        store_spreg( R_EAX, R_SSR );
                                        }
                                        break;
                                    case 0x4:
                                        { /* LDC.L @Rm+, SPC */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_reg( R_EAX, Rm );
                                        MOV_r32_r32( R_EAX, R_ECX );
                                        ADD_imm8s_r32( 4, R_EAX );
                                        store_reg( R_EAX, Rm );
                                        MEM_READ_LONG( R_ECX, R_EAX );
                                        store_spreg( R_EAX, R_SPC );
                                        }
                                        break;
                                    default:
                                        UNDEF();
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* LDC.L @Rm+, Rn_BANK */
                                uint32_t Rm = ((ir>>8)&0xF); uint32_t Rn_BANK = ((ir>>4)&0x7); 
                                check_priv();
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, REG_OFFSET(r_bank[Rn_BANK]) );
                                }
                                break;
                        }
                        break;
                    case 0x8:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLL2 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHL_imm8_r32( 2, R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x1:
                                { /* SHLL8 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHL_imm8_r32( 8, R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x2:
                                { /* SHLL16 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHL_imm8_r32( 16, R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x9:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLR2 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHR_imm8_r32( 2, R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x1:
                                { /* SHLR8 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHR_imm8_r32( 8, R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x2:
                                { /* SHLR16 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHR_imm8_r32( 16, R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xA:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* LDS Rm, MACH */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_MACH );
                                }
                                break;
                            case 0x1:
                                { /* LDS Rm, MACL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_MACL );
                                }
                                break;
                            case 0x2:
                                { /* LDS Rm, PR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_PR );
                                }
                                break;
                            case 0x3:
                                { /* LDC Rm, SGR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                check_priv();
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_SGR );
                                }
                                break;
                            case 0x5:
                                { /* LDS Rm, FPUL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_FPUL );
                                }
                                break;
                            case 0x6:
                                { /* LDS Rm, FPSCR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_FPSCR );
                                update_fr_bank( R_EAX );
                                }
                                break;
                            case 0xF:
                                { /* LDC Rm, DBR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                check_priv();
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_DBR );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xB:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* JSR @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                if( sh4_x86.in_delay_slot ) {
                            	SLOTILLEGAL();
                                } else {
                            	load_imm32( R_EAX, pc + 4 );
                            	store_spreg( R_EAX, R_PR );
                            	load_reg( R_EDI, Rn );
                            	sh4_x86.in_delay_slot = TRUE;
                            	return 0;
                                }
                                }
                                break;
                            case 0x1:
                                { /* TAS.B @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                MEM_READ_BYTE( R_ECX, R_EAX );
                                TEST_r8_r8( R_AL, R_AL );
                                SETE_t();
                                OR_imm8_r8( 0x80, R_AL );
                                load_reg( R_ECX, Rn );
                                MEM_WRITE_BYTE( R_ECX, R_EAX );
                                }
                                break;
                            case 0x2:
                                { /* JMP @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                if( sh4_x86.in_delay_slot ) {
                            	SLOTILLEGAL();
                                } else {
                            	load_reg( R_EDI, Rn );
                            	sh4_x86.in_delay_slot = TRUE;
                            	return 0;
                                }
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xC:
                        { /* SHAD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        /* Annoyingly enough, not directly convertible */
                        load_reg( R_EAX, Rn );
                        load_reg( R_ECX, Rm );
                        CMP_imm32_r32( 0, R_ECX );
                        JGE_rel8(16, doshl);
                                        
                        NEG_r32( R_ECX );      // 2
                        AND_imm8_r8( 0x1F, R_CL ); // 3
                        JE_rel8( 4, emptysar);     // 2
                        SAR_r32_CL( R_EAX );       // 2
                        JMP_rel8(10, end);          // 2
                    
                        JMP_TARGET(emptysar);
                        SAR_imm8_r32(31, R_EAX );  // 3
                        JMP_rel8(5, end2);
                    
                        JMP_TARGET(doshl);
                        AND_imm8_r8( 0x1F, R_CL ); // 3
                        SHL_r32_CL( R_EAX );       // 2
                        JMP_TARGET(end);
                        JMP_TARGET(end2);
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xD:
                        { /* SHLD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rn );
                        load_reg( R_ECX, Rm );
                        CMP_imm32_r32( 0, R_ECX );
                        JGE_rel8(15, doshl);
                    
                        NEG_r32( R_ECX );      // 2
                        AND_imm8_r8( 0x1F, R_CL ); // 3
                        JE_rel8( 4, emptyshr );
                        SHR_r32_CL( R_EAX );       // 2
                        JMP_rel8(9, end);          // 2
                    
                        JMP_TARGET(emptyshr);
                        XOR_r32_r32( R_EAX, R_EAX );
                        JMP_rel8(5, end2);
                    
                        JMP_TARGET(doshl);
                        AND_imm8_r8( 0x1F, R_CL ); // 3
                        SHL_r32_CL( R_EAX );       // 2
                        JMP_TARGET(end);
                        JMP_TARGET(end2);
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xE:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* LDC Rm, SR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        if( sh4_x86.in_delay_slot ) {
                                    	SLOTILLEGAL();
                                        } else {
                                    	check_priv();
                                    	load_reg( R_EAX, Rm );
                                    	call_func1( sh4_write_sr, R_EAX );
                                    	sh4_x86.priv_checked = FALSE;
                                    	sh4_x86.fpuen_checked = FALSE;
                                        }
                                        }
                                        break;
                                    case 0x1:
                                        { /* LDC Rm, GBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        load_reg( R_EAX, Rm );
                                        store_spreg( R_EAX, R_GBR );
                                        }
                                        break;
                                    case 0x2:
                                        { /* LDC Rm, VBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_reg( R_EAX, Rm );
                                        store_spreg( R_EAX, R_VBR );
                                        }
                                        break;
                                    case 0x3:
                                        { /* LDC Rm, SSR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_reg( R_EAX, Rm );
                                        store_spreg( R_EAX, R_SSR );
                                        }
                                        break;
                                    case 0x4:
                                        { /* LDC Rm, SPC */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        check_priv();
                                        load_reg( R_EAX, Rm );
                                        store_spreg( R_EAX, R_SPC );
                                        }
                                        break;
                                    default:
                                        UNDEF();
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* LDC Rm, Rn_BANK */
                                uint32_t Rm = ((ir>>8)&0xF); uint32_t Rn_BANK = ((ir>>4)&0x7); 
                                check_priv();
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, REG_OFFSET(r_bank[Rn_BANK]) );
                                }
                                break;
                        }
                        break;
                    case 0xF:
                        { /* MAC.W @Rm+, @Rn+ */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_ECX, Rm );
                        check_ralign16( R_ECX );
                        load_reg( R_ECX, Rn );
                        check_ralign16( R_ECX );
                        ADD_imm8s_sh4r( 2, REG_OFFSET(r[Rn]) );
                        MEM_READ_WORD( R_ECX, R_EAX );
                        PUSH_r32( R_EAX );
                        load_reg( R_ECX, Rm );
                        ADD_imm8s_sh4r( 2, REG_OFFSET(r[Rm]) );
                        MEM_READ_WORD( R_ECX, R_EAX );
                        POP_r32( R_ECX );
                        IMUL_r32( R_ECX );
                    
                        load_spreg( R_ECX, R_S );
                        TEST_r32_r32( R_ECX, R_ECX );
                        JE_rel8( 47, nosat );
                    
                        ADD_r32_sh4r( R_EAX, R_MACL );  // 6
                        JNO_rel8( 51, end );            // 2
                        load_imm32( R_EDX, 1 );         // 5
                        store_spreg( R_EDX, R_MACH );   // 6
                        JS_rel8( 13, positive );        // 2
                        load_imm32( R_EAX, 0x80000000 );// 5
                        store_spreg( R_EAX, R_MACL );   // 6
                        JMP_rel8( 25, end2 );           // 2
                    
                        JMP_TARGET(positive);
                        load_imm32( R_EAX, 0x7FFFFFFF );// 5
                        store_spreg( R_EAX, R_MACL );   // 6
                        JMP_rel8( 12, end3);            // 2
                    
                        JMP_TARGET(nosat);
                        ADD_r32_sh4r( R_EAX, R_MACL );  // 6
                        ADC_r32_sh4r( R_EDX, R_MACH );  // 6
                        JMP_TARGET(end);
                        JMP_TARGET(end2);
                        JMP_TARGET(end3);
                        }
                        break;
                }
                break;
            case 0x5:
                { /* MOV.L @(disp, Rm), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<2; 
                load_reg( R_ECX, Rm );
                ADD_imm8s_r32( disp, R_ECX );
                check_ralign32( R_ECX );
                MEM_READ_LONG( R_ECX, R_EAX );
                store_reg( R_EAX, Rn );
                }
                break;
            case 0x6:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* MOV.B @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_ECX, Rm );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_ECX, Rm );
                        check_ralign16( R_ECX );
                        MEM_READ_WORD( R_ECX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x2:
                        { /* MOV.L @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_ECX, Rm );
                        check_ralign32( R_ECX );
                        MEM_READ_LONG( R_ECX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x3:
                        { /* MOV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_ECX, Rm );
                        MOV_r32_r32( R_ECX, R_EAX );
                        ADD_imm8s_r32( 1, R_EAX );
                        store_reg( R_EAX, Rm );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        check_ralign16( R_EAX );
                        MOV_r32_r32( R_EAX, R_ECX );
                        ADD_imm8s_r32( 2, R_EAX );
                        store_reg( R_EAX, Rm );
                        MEM_READ_WORD( R_ECX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x6:
                        { /* MOV.L @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        check_ralign32( R_EAX );
                        MOV_r32_r32( R_EAX, R_ECX );
                        ADD_imm8s_r32( 4, R_EAX );
                        store_reg( R_EAX, Rm );
                        MEM_READ_LONG( R_ECX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x7:
                        { /* NOT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        NOT_r32( R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x8:
                        { /* SWAP.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        XCHG_r8_r8( R_AL, R_AH );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x9:
                        { /* SWAP.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        MOV_r32_r32( R_EAX, R_ECX );
                        SHL_imm8_r32( 16, R_ECX );
                        SHR_imm8_r32( 16, R_EAX );
                        OR_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xA:
                        { /* NEGC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        XOR_r32_r32( R_ECX, R_ECX );
                        LDC_t();
                        SBB_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        SETC_t();
                        }
                        break;
                    case 0xB:
                        { /* NEG Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        NEG_r32( R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xC:
                        { /* EXTU.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        MOVZX_r8_r32( R_EAX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xD:
                        { /* EXTU.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        MOVZX_r16_r32( R_EAX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xE:
                        { /* EXTS.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        MOVSX_r8_r32( R_EAX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xF:
                        { /* EXTS.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        MOVSX_r16_r32( R_EAX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                }
                break;
            case 0x7:
                { /* ADD #imm, Rn */
                uint32_t Rn = ((ir>>8)&0xF); int32_t imm = SIGNEXT8(ir&0xFF); 
                load_reg( R_EAX, Rn );
                ADD_imm8s_r32( imm, R_EAX );
                store_reg( R_EAX, Rn );
                }
                break;
            case 0x8:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        { /* MOV.B R0, @(disp, Rn) */
                        uint32_t Rn = ((ir>>4)&0xF); uint32_t disp = (ir&0xF); 
                        load_reg( R_EAX, 0 );
                        load_reg( R_ECX, Rn );
                        ADD_imm32_r32( disp, R_ECX );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W R0, @(disp, Rn) */
                        uint32_t Rn = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<1; 
                        load_reg( R_ECX, Rn );
                        load_reg( R_EAX, 0 );
                        ADD_imm32_r32( disp, R_ECX );
                        check_walign16( R_ECX );
                        MEM_WRITE_WORD( R_ECX, R_EAX );
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @(disp, Rm), R0 */
                        uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF); 
                        load_reg( R_ECX, Rm );
                        ADD_imm32_r32( disp, R_ECX );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @(disp, Rm), R0 */
                        uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<1; 
                        load_reg( R_ECX, Rm );
                        ADD_imm32_r32( disp, R_ECX );
                        check_ralign16( R_ECX );
                        MEM_READ_WORD( R_ECX, R_EAX );
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0x8:
                        { /* CMP/EQ #imm, R0 */
                        int32_t imm = SIGNEXT8(ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        CMP_imm8s_r32(imm, R_EAX);
                        SETE_t();
                        }
                        break;
                    case 0x9:
                        { /* BT disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
                        if( sh4_x86.in_delay_slot ) {
                    	SLOTILLEGAL();
                        } else {
                    	load_imm32( R_EDI, pc + 2 );
                    	CMP_imm8s_sh4r( 0, R_T );
                    	JE_rel8( 5, nottaken );
                    	load_imm32( R_EDI, disp + pc + 4 );
                    	JMP_TARGET(nottaken);
                    	INC_r32(R_ESI);
                    	return 1;
                        }
                        }
                        break;
                    case 0xB:
                        { /* BF disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
                        if( sh4_x86.in_delay_slot ) {
                    	SLOTILLEGAL();
                        } else {
                    	load_imm32( R_EDI, pc + 2 );
                    	CMP_imm8s_sh4r( 0, R_T );
                    	JNE_rel8( 5, nottaken );
                    	load_imm32( R_EDI, disp + pc + 4 );
                    	JMP_TARGET(nottaken);
                    	INC_r32(R_ESI);
                    	return 1;
                        }
                        }
                        break;
                    case 0xD:
                        { /* BT/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
                        if( sh4_x86.in_delay_slot ) {
                    	SLOTILLEGAL();
                        } else {
                    	load_imm32( R_EDI, pc + 4 );
                    	CMP_imm8s_sh4r( 0, R_T );
                    	JE_rel8( 5, nottaken );
                    	load_imm32( R_EDI, disp + pc + 4 );
                    	JMP_TARGET(nottaken);
                    	sh4_x86.in_delay_slot = TRUE;
                    	return 0;
                        }
                        }
                        break;
                    case 0xF:
                        { /* BF/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
                        if( sh4_x86.in_delay_slot ) {
                    	SLOTILLEGAL();
                        } else {
                    	load_imm32( R_EDI, pc + 4 );
                    	CMP_imm8s_sh4r( 0, R_T );
                    	JNE_rel8( 5, nottaken );
                    	load_imm32( R_EDI, disp + pc + 4 );
                    	JMP_TARGET(nottaken);
                    	sh4_x86.in_delay_slot = TRUE;
                    	return 0;
                        }
                        }
                        break;
                    default:
                        UNDEF();
                        break;
                }
                break;
            case 0x9:
                { /* MOV.W @(disp, PC), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t disp = (ir&0xFF)<<1; 
                if( sh4_x86.in_delay_slot ) {
            	SLOTILLEGAL();
                } else {
            	load_imm32( R_ECX, pc + disp + 4 );
            	MEM_READ_WORD( R_ECX, R_EAX );
            	store_reg( R_EAX, Rn );
                }
                }
                break;
            case 0xA:
                { /* BRA disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
                if( sh4_x86.in_delay_slot ) {
            	SLOTILLEGAL();
                } else {
            	load_imm32( R_EDI, disp + pc + 4 );
            	sh4_x86.in_delay_slot = TRUE;
            	return 0;
                }
                }
                break;
            case 0xB:
                { /* BSR disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
                if( sh4_x86.in_delay_slot ) {
            	SLOTILLEGAL();
                } else {
            	load_imm32( R_EAX, pc + 4 );
            	store_spreg( R_EAX, R_PR );
            	load_imm32( R_EDI, disp + pc + 4 );
            	sh4_x86.in_delay_slot = TRUE;
            	return 0;
                }
                }
                break;
            case 0xC:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        { /* MOV.B R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        load_spreg( R_ECX, R_GBR );
                        ADD_imm32_r32( disp, R_ECX );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF)<<1; 
                        load_spreg( R_ECX, R_GBR );
                        load_reg( R_EAX, 0 );
                        ADD_imm32_r32( disp, R_ECX );
                        check_walign16( R_ECX );
                        MEM_WRITE_WORD( R_ECX, R_EAX );
                        }
                        break;
                    case 0x2:
                        { /* MOV.L R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF)<<2; 
                        load_spreg( R_ECX, R_GBR );
                        load_reg( R_EAX, 0 );
                        ADD_imm32_r32( disp, R_ECX );
                        check_walign32( R_ECX );
                        MEM_WRITE_LONG( R_ECX, R_EAX );
                        }
                        break;
                    case 0x3:
                        { /* TRAPA #imm */
                        uint32_t imm = (ir&0xFF); 
                        if( sh4_x86.in_delay_slot ) {
                    	SLOTILLEGAL();
                        } else {
                    	PUSH_imm32( imm );
                    	call_func0( sh4_raise_trap );
                    	ADD_imm8s_r32( 4, R_ESP );
                        }
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF); 
                        load_spreg( R_ECX, R_GBR );
                        ADD_imm32_r32( disp, R_ECX );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF)<<1; 
                        load_spreg( R_ECX, R_GBR );
                        ADD_imm32_r32( disp, R_ECX );
                        check_ralign16( R_ECX );
                        MEM_READ_WORD( R_ECX, R_EAX );
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0x6:
                        { /* MOV.L @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF)<<2; 
                        load_spreg( R_ECX, R_GBR );
                        ADD_imm32_r32( disp, R_ECX );
                        check_ralign32( R_ECX );
                        MEM_READ_LONG( R_ECX, R_EAX );
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0x7:
                        { /* MOVA @(disp, PC), R0 */
                        uint32_t disp = (ir&0xFF)<<2; 
                        if( sh4_x86.in_delay_slot ) {
                    	SLOTILLEGAL();
                        } else {
                    	load_imm32( R_ECX, (pc & 0xFFFFFFFC) + disp + 4 );
                    	store_reg( R_ECX, 0 );
                        }
                        }
                        break;
                    case 0x8:
                        { /* TST #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        TEST_imm32_r32( imm, R_EAX );
                        SETE_t();
                        }
                        break;
                    case 0x9:
                        { /* AND #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        AND_imm32_r32(imm, R_EAX); 
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0xA:
                        { /* XOR #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        XOR_imm32_r32( imm, R_EAX );
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0xB:
                        { /* OR #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        OR_imm32_r32(imm, R_EAX);
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0xC:
                        { /* TST.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0);
                        load_reg( R_ECX, R_GBR);
                        ADD_r32_r32( R_EAX, R_ECX );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        TEST_imm8_r8( imm, R_EAX );
                        SETE_t();
                        }
                        break;
                    case 0xD:
                        { /* AND.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        load_spreg( R_ECX, R_GBR );
                        ADD_r32_r32( R_EAX, R_ECX );
                        PUSH_r32(R_ECX);
                        call_func0(sh4_read_byte);
                        POP_r32(R_ECX);
                        AND_imm32_r32(imm, R_EAX );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0xE:
                        { /* XOR.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        load_spreg( R_ECX, R_GBR );
                        ADD_r32_r32( R_EAX, R_ECX );
                        PUSH_r32(R_ECX);
                        call_func0(sh4_read_byte);
                        POP_r32(R_ECX);
                        XOR_imm32_r32( imm, R_EAX );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0xF:
                        { /* OR.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        load_spreg( R_ECX, R_GBR );
                        ADD_r32_r32( R_EAX, R_ECX );
                        PUSH_r32(R_ECX);
                        call_func0(sh4_read_byte);
                        POP_r32(R_ECX);
                        OR_imm32_r32(imm, R_EAX );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                }
                break;
            case 0xD:
                { /* MOV.L @(disp, PC), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t disp = (ir&0xFF)<<2; 
                if( sh4_x86.in_delay_slot ) {
            	SLOTILLEGAL();
                } else {
            	uint32_t target = (pc & 0xFFFFFFFC) + disp + 4;
            	char *ptr = mem_get_region(target);
            	if( ptr != NULL ) {
            	    MOV_moff32_EAX( (uint32_t)ptr );
            	} else {
            	    load_imm32( R_ECX, target );
            	    MEM_READ_LONG( R_ECX, R_EAX );
            	}
            	store_reg( R_EAX, Rn );
                }
                }
                break;
            case 0xE:
                { /* MOV #imm, Rn */
                uint32_t Rn = ((ir>>8)&0xF); int32_t imm = SIGNEXT8(ir&0xFF); 
                load_imm32( R_EAX, imm );
                store_reg( R_EAX, Rn );
                }
                break;
            case 0xF:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* FADD FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_PR, R_ECX );
                        load_fr_bank( R_EDX );
                        JNE_rel8(13,doubleprec);
                        push_fr(R_EDX, FRm);
                        push_fr(R_EDX, FRn);
                        FADDP_st(1);
                        pop_fr(R_EDX, FRn);
                        JMP_rel8(11,end);
                        JMP_TARGET(doubleprec);
                        push_dr(R_EDX, FRm);
                        push_dr(R_EDX, FRn);
                        FADDP_st(1);
                        pop_dr(R_EDX, FRn);
                        JMP_TARGET(end);
                        }
                        break;
                    case 0x1:
                        { /* FSUB FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_PR, R_ECX );
                        load_fr_bank( R_EDX );
                        JNE_rel8(13, doubleprec);
                        push_fr(R_EDX, FRn);
                        push_fr(R_EDX, FRm);
                        FSUBP_st(1);
                        pop_fr(R_EDX, FRn);
                        JMP_rel8(11, end);
                        JMP_TARGET(doubleprec);
                        push_dr(R_EDX, FRn);
                        push_dr(R_EDX, FRm);
                        FSUBP_st(1);
                        pop_dr(R_EDX, FRn);
                        JMP_TARGET(end);
                        }
                        break;
                    case 0x2:
                        { /* FMUL FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_PR, R_ECX );
                        load_fr_bank( R_EDX );
                        JNE_rel8(13, doubleprec);
                        push_fr(R_EDX, FRm);
                        push_fr(R_EDX, FRn);
                        FMULP_st(1);
                        pop_fr(R_EDX, FRn);
                        JMP_rel8(11, end);
                        JMP_TARGET(doubleprec);
                        push_dr(R_EDX, FRm);
                        push_dr(R_EDX, FRn);
                        FMULP_st(1);
                        pop_dr(R_EDX, FRn);
                        JMP_TARGET(end);
                        }
                        break;
                    case 0x3:
                        { /* FDIV FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_PR, R_ECX );
                        load_fr_bank( R_EDX );
                        JNE_rel8(13, doubleprec);
                        push_fr(R_EDX, FRn);
                        push_fr(R_EDX, FRm);
                        FDIVP_st(1);
                        pop_fr(R_EDX, FRn);
                        JMP_rel8(11, end);
                        JMP_TARGET(doubleprec);
                        push_dr(R_EDX, FRn);
                        push_dr(R_EDX, FRm);
                        FDIVP_st(1);
                        pop_dr(R_EDX, FRn);
                        JMP_TARGET(end);
                        }
                        break;
                    case 0x4:
                        { /* FCMP/EQ FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_PR, R_ECX );
                        load_fr_bank( R_EDX );
                        JNE_rel8(8, doubleprec);
                        push_fr(R_EDX, FRm);
                        push_fr(R_EDX, FRn);
                        JMP_rel8(6, end);
                        JMP_TARGET(doubleprec);
                        push_dr(R_EDX, FRm);
                        push_dr(R_EDX, FRn);
                        JMP_TARGET(end);
                        FCOMIP_st(1);
                        SETE_t();
                        FPOP_st();
                        }
                        break;
                    case 0x5:
                        { /* FCMP/GT FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_PR, R_ECX );
                        load_fr_bank( R_EDX );
                        JNE_rel8(8, doubleprec);
                        push_fr(R_EDX, FRm);
                        push_fr(R_EDX, FRn);
                        JMP_rel8(6, end);
                        JMP_TARGET(doubleprec);
                        push_dr(R_EDX, FRm);
                        push_dr(R_EDX, FRn);
                        JMP_TARGET(end);
                        FCOMIP_st(1);
                        SETA_t();
                        FPOP_st();
                        }
                        break;
                    case 0x6:
                        { /* FMOV @(R0, Rm), FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_reg( R_EDX, Rm );
                        ADD_sh4r_r32( REG_OFFSET(r[0]), R_EDX );
                        check_ralign32( R_EDX );
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_SZ, R_ECX );
                        JNE_rel8(19, doublesize);
                        MEM_READ_LONG( R_EDX, R_EAX );
                        load_fr_bank( R_ECX );
                        store_fr( R_ECX, R_EAX, FRn );
                        if( FRn&1 ) {
                    	JMP_rel8(48, end);
                    	JMP_TARGET(doublesize);
                    	MEM_READ_DOUBLE( R_EDX, R_EAX, R_EDX );
                    	load_spreg( R_ECX, R_FPSCR ); // assume read_long clobbered it
                    	load_xf_bank( R_ECX );
                    	store_fr( R_ECX, R_EAX, FRn&0x0E );
                    	store_fr( R_ECX, R_EDX, FRn|0x01 );
                    	JMP_TARGET(end);
                        } else {
                    	JMP_rel8(36, end);
                    	JMP_TARGET(doublesize);
                    	MEM_READ_DOUBLE( R_EDX, R_EAX, R_EDX );
                    	load_fr_bank( R_ECX );
                    	store_fr( R_ECX, R_EAX, FRn&0x0E );
                    	store_fr( R_ECX, R_EDX, FRn|0x01 );
                    	JMP_TARGET(end);
                        }
                        }
                        break;
                    case 0x7:
                        { /* FMOV FRm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_reg( R_EDX, Rn );
                        ADD_sh4r_r32( REG_OFFSET(r[0]), R_EDX );
                        check_walign32( R_EDX );
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_SZ, R_ECX );
                        JNE_rel8(20, doublesize);
                        load_fr_bank( R_ECX );
                        load_fr( R_ECX, R_EAX, FRm );
                        MEM_WRITE_LONG( R_EDX, R_EAX ); // 12
                        if( FRm&1 ) {
                    	JMP_rel8( 48, end );
                    	JMP_TARGET(doublesize);
                    	load_xf_bank( R_ECX );
                    	load_fr( R_ECX, R_EAX, FRm&0x0E );
                    	load_fr( R_ECX, R_ECX, FRm|0x01 );
                    	MEM_WRITE_DOUBLE( R_EDX, R_EAX, R_ECX );
                    	JMP_TARGET(end);
                        } else {
                    	JMP_rel8( 39, end );
                    	JMP_TARGET(doublesize);
                    	load_fr_bank( R_ECX );
                    	load_fr( R_ECX, R_EAX, FRm&0x0E );
                    	load_fr( R_ECX, R_ECX, FRm|0x01 );
                    	MEM_WRITE_DOUBLE( R_EDX, R_EAX, R_ECX );
                    	JMP_TARGET(end);
                        }
                        }
                        break;
                    case 0x8:
                        { /* FMOV @Rm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_reg( R_EDX, Rm );
                        check_ralign32( R_EDX );
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_SZ, R_ECX );
                        JNE_rel8(19, doublesize);
                        MEM_READ_LONG( R_EDX, R_EAX );
                        load_fr_bank( R_ECX );
                        store_fr( R_ECX, R_EAX, FRn );
                        if( FRn&1 ) {
                    	JMP_rel8(48, end);
                    	JMP_TARGET(doublesize);
                    	MEM_READ_DOUBLE( R_EDX, R_EAX, R_EDX );
                    	load_spreg( R_ECX, R_FPSCR ); // assume read_long clobbered it
                    	load_xf_bank( R_ECX );
                    	store_fr( R_ECX, R_EAX, FRn&0x0E );
                    	store_fr( R_ECX, R_EDX, FRn|0x01 );
                    	JMP_TARGET(end);
                        } else {
                    	JMP_rel8(36, end);
                    	JMP_TARGET(doublesize);
                    	MEM_READ_DOUBLE( R_EDX, R_EAX, R_EDX );
                    	load_fr_bank( R_ECX );
                    	store_fr( R_ECX, R_EAX, FRn&0x0E );
                    	store_fr( R_ECX, R_EDX, FRn|0x01 );
                    	JMP_TARGET(end);
                        }
                        }
                        break;
                    case 0x9:
                        { /* FMOV @Rm+, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_reg( R_EDX, Rm );
                        check_ralign32( R_EDX );
                        MOV_r32_r32( R_EDX, R_EAX );
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_SZ, R_ECX );
                        JNE_rel8(25, doublesize);
                        ADD_imm8s_r32( 4, R_EAX );
                        store_reg( R_EAX, Rm );
                        MEM_READ_LONG( R_EDX, R_EAX );
                        load_fr_bank( R_ECX );
                        store_fr( R_ECX, R_EAX, FRn );
                        if( FRn&1 ) {
                    	JMP_rel8(54, end);
                    	JMP_TARGET(doublesize);
                    	ADD_imm8s_r32( 8, R_EAX );
                    	store_reg(R_EAX, Rm);
                    	MEM_READ_DOUBLE( R_EDX, R_EAX, R_EDX );
                    	load_spreg( R_ECX, R_FPSCR ); // assume read_long clobbered it
                    	load_xf_bank( R_ECX );
                    	store_fr( R_ECX, R_EAX, FRn&0x0E );
                    	store_fr( R_ECX, R_EDX, FRn|0x01 );
                    	JMP_TARGET(end);
                        } else {
                    	JMP_rel8(42, end);
                    	ADD_imm8s_r32( 8, R_EAX );
                    	store_reg(R_EAX, Rm);
                    	MEM_READ_DOUBLE( R_EDX, R_EAX, R_EDX );
                    	load_fr_bank( R_ECX );
                    	store_fr( R_ECX, R_EAX, FRn&0x0E );
                    	store_fr( R_ECX, R_EDX, FRn|0x01 );
                    	JMP_TARGET(end);
                        }
                        }
                        break;
                    case 0xA:
                        { /* FMOV FRm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_reg( R_EDX, Rn );
                        check_walign32( R_EDX );
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_SZ, R_ECX );
                        JNE_rel8(20, doublesize);
                        load_fr_bank( R_ECX );
                        load_fr( R_ECX, R_EAX, FRm );
                        MEM_WRITE_LONG( R_EDX, R_EAX ); // 12
                        if( FRm&1 ) {
                    	JMP_rel8( 48, end );
                    	JMP_TARGET(doublesize);
                    	load_xf_bank( R_ECX );
                    	load_fr( R_ECX, R_EAX, FRm&0x0E );
                    	load_fr( R_ECX, R_ECX, FRm|0x01 );
                    	MEM_WRITE_DOUBLE( R_EDX, R_EAX, R_ECX );
                    	JMP_TARGET(end);
                        } else {
                    	JMP_rel8( 39, end );
                    	JMP_TARGET(doublesize);
                    	load_fr_bank( R_ECX );
                    	load_fr( R_ECX, R_EAX, FRm&0x0E );
                    	load_fr( R_ECX, R_ECX, FRm|0x01 );
                    	MEM_WRITE_DOUBLE( R_EDX, R_EAX, R_ECX );
                    	JMP_TARGET(end);
                        }
                        }
                        break;
                    case 0xB:
                        { /* FMOV FRm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_reg( R_EDX, Rn );
                        check_walign32( R_EDX );
                        load_spreg( R_ECX, R_FPSCR );
                        TEST_imm32_r32( FPSCR_SZ, R_ECX );
                        JNE_rel8(26, doublesize);
                        load_fr_bank( R_ECX );
                        load_fr( R_ECX, R_EAX, FRm );
                        ADD_imm8s_r32(-4,R_EDX);
                        store_reg( R_EDX, Rn );
                        MEM_WRITE_LONG( R_EDX, R_EAX ); // 12
                        if( FRm&1 ) {
                    	JMP_rel8( 54, end );
                    	JMP_TARGET(doublesize);
                    	load_xf_bank( R_ECX );
                    	load_fr( R_ECX, R_EAX, FRm&0x0E );
                    	load_fr( R_ECX, R_ECX, FRm|0x01 );
                    	ADD_imm8s_r32(-8,R_EDX);
                    	store_reg( R_EDX, Rn );
                    	MEM_WRITE_DOUBLE( R_EDX, R_EAX, R_ECX );
                    	JMP_TARGET(end);
                        } else {
                    	JMP_rel8( 45, end );
                    	JMP_TARGET(doublesize);
                    	load_fr_bank( R_ECX );
                    	load_fr( R_ECX, R_EAX, FRm&0x0E );
                    	load_fr( R_ECX, R_ECX, FRm|0x01 );
                    	ADD_imm8s_r32(-8,R_EDX);
                    	store_reg( R_EDX, Rn );
                    	MEM_WRITE_DOUBLE( R_EDX, R_EAX, R_ECX );
                    	JMP_TARGET(end);
                        }
                        }
                        break;
                    case 0xC:
                        { /* FMOV FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        /* As horrible as this looks, it's actually covering 5 separate cases:
                         * 1. 32-bit fr-to-fr (PR=0)
                         * 2. 64-bit dr-to-dr (PR=1, FRm&1 == 0, FRn&1 == 0 )
                         * 3. 64-bit dr-to-xd (PR=1, FRm&1 == 0, FRn&1 == 1 )
                         * 4. 64-bit xd-to-dr (PR=1, FRm&1 == 1, FRn&1 == 0 )
                         * 5. 64-bit xd-to-xd (PR=1, FRm&1 == 1, FRn&1 == 1 )
                         */
                        check_fpuen();
                        load_spreg( R_ECX, R_FPSCR );
                        load_fr_bank( R_EDX );
                        TEST_imm32_r32( FPSCR_SZ, R_ECX );
                        JNE_rel8(8, doublesize);
                        load_fr( R_EDX, R_EAX, FRm ); // PR=0 branch
                        store_fr( R_EDX, R_EAX, FRn );
                        if( FRm&1 ) {
                    	JMP_rel8(24, end);
                    	JMP_TARGET(doublesize);
                    	load_xf_bank( R_ECX ); 
                    	load_fr( R_ECX, R_EAX, FRm-1 );
                    	if( FRn&1 ) {
                    	    load_fr( R_ECX, R_EDX, FRm );
                    	    store_fr( R_ECX, R_EAX, FRn-1 );
                    	    store_fr( R_ECX, R_EDX, FRn );
                    	} else /* FRn&1 == 0 */ {
                    	    load_fr( R_ECX, R_ECX, FRm );
                    	    store_fr( R_EDX, R_EAX, FRn );
                    	    store_fr( R_EDX, R_ECX, FRn+1 );
                    	}
                    	JMP_TARGET(end);
                        } else /* FRm&1 == 0 */ {
                    	if( FRn&1 ) {
                    	    JMP_rel8(24, end);
                    	    load_xf_bank( R_ECX );
                    	    load_fr( R_EDX, R_EAX, FRm );
                    	    load_fr( R_EDX, R_EDX, FRm+1 );
                    	    store_fr( R_ECX, R_EAX, FRn-1 );
                    	    store_fr( R_ECX, R_EDX, FRn );
                    	    JMP_TARGET(end);
                    	} else /* FRn&1 == 0 */ {
                    	    JMP_rel8(12, end);
                    	    load_fr( R_EDX, R_EAX, FRm );
                    	    load_fr( R_EDX, R_ECX, FRm+1 );
                    	    store_fr( R_EDX, R_EAX, FRn );
                    	    store_fr( R_EDX, R_ECX, FRn+1 );
                    	    JMP_TARGET(end);
                    	}
                        }
                        }
                        break;
                    case 0xD:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* FSTS FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                check_fpuen();
                                load_fr_bank( R_ECX );
                                load_spreg( R_EAX, R_FPUL );
                                store_fr( R_ECX, R_EAX, FRn );
                                }
                                break;
                            case 0x1:
                                { /* FLDS FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
                                check_fpuen();
                                load_fr_bank( R_ECX );
                                load_fr( R_ECX, R_EAX, FRm );
                                store_spreg( R_EAX, R_FPUL );
                                }
                                break;
                            case 0x2:
                                { /* FLOAT FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                check_fpuen();
                                load_spreg( R_ECX, R_FPSCR );
                                load_spreg(R_EDX, REG_OFFSET(fr_bank));
                                FILD_sh4r(R_FPUL);
                                TEST_imm32_r32( FPSCR_PR, R_ECX );
                                JNE_rel8(5, doubleprec);
                                pop_fr( R_EDX, FRn );
                                JMP_rel8(3, end);
                                JMP_TARGET(doubleprec);
                                pop_dr( R_EDX, FRn );
                                JMP_TARGET(end);
                                }
                                break;
                            case 0x3:
                                { /* FTRC FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
                                check_fpuen();
                                load_spreg( R_ECX, R_FPSCR );
                                load_fr_bank( R_EDX );
                                TEST_imm32_r32( FPSCR_PR, R_ECX );
                                JNE_rel8(5, doubleprec);
                                push_fr( R_EDX, FRm );
                                JMP_rel8(3, doop);
                                JMP_TARGET(doubleprec);
                                push_dr( R_EDX, FRm );
                                JMP_TARGET( doop );
                                load_imm32( R_ECX, (uint32_t)&max_int );
                                FILD_r32ind( R_ECX );
                                FCOMIP_st(1);
                                JNA_rel8( 16, sat );
                                load_imm32( R_ECX, (uint32_t)&min_int );  // 5
                                FILD_r32ind( R_ECX );           // 2
                                FCOMIP_st(1);                   // 2
                                JAE_rel8( 5, sat2 );            // 2
                                FISTP_sh4r(R_FPUL);             // 3
                                JMP_rel8( 9, end );             // 2
                            
                                JMP_TARGET(sat);
                                JMP_TARGET(sat2);
                                MOV_r32ind_r32( R_ECX, R_ECX ); // 2
                                store_spreg( R_ECX, R_FPUL );
                                FPOP_st();
                                JMP_TARGET(end);
                                }
                                break;
                            case 0x4:
                                { /* FNEG FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                check_fpuen();
                                load_spreg( R_ECX, R_FPSCR );
                                TEST_imm32_r32( FPSCR_PR, R_ECX );
                                load_fr_bank( R_EDX );
                                JNE_rel8(10, doubleprec);
                                push_fr(R_EDX, FRn);
                                FCHS_st0();
                                pop_fr(R_EDX, FRn);
                                JMP_rel8(8, end);
                                JMP_TARGET(doubleprec);
                                push_dr(R_EDX, FRn);
                                FCHS_st0();
                                pop_dr(R_EDX, FRn);
                                JMP_TARGET(end);
                                }
                                break;
                            case 0x5:
                                { /* FABS FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                check_fpuen();
                                load_spreg( R_ECX, R_FPSCR );
                                load_fr_bank( R_EDX );
                                TEST_imm32_r32( FPSCR_PR, R_ECX );
                                JNE_rel8(10, doubleprec);
                                push_fr(R_EDX, FRn); // 3
                                FABS_st0(); // 2
                                pop_fr( R_EDX, FRn); //3
                                JMP_rel8(8,end); // 2
                                JMP_TARGET(doubleprec);
                                push_dr(R_EDX, FRn);
                                FABS_st0();
                                pop_dr(R_EDX, FRn);
                                JMP_TARGET(end);
                                }
                                break;
                            case 0x6:
                                { /* FSQRT FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                check_fpuen();
                                load_spreg( R_ECX, R_FPSCR );
                                TEST_imm32_r32( FPSCR_PR, R_ECX );
                                load_fr_bank( R_EDX );
                                JNE_rel8(10, doubleprec);
                                push_fr(R_EDX, FRn);
                                FSQRT_st0();
                                pop_fr(R_EDX, FRn);
                                JMP_rel8(8, end);
                                JMP_TARGET(doubleprec);
                                push_dr(R_EDX, FRn);
                                FSQRT_st0();
                                pop_dr(R_EDX, FRn);
                                JMP_TARGET(end);
                                }
                                break;
                            case 0x7:
                                { /* FSRRA FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                check_fpuen();
                                load_spreg( R_ECX, R_FPSCR );
                                TEST_imm32_r32( FPSCR_PR, R_ECX );
                                load_fr_bank( R_EDX );
                                JNE_rel8(12, end); // PR=0 only
                                FLD1_st0();
                                push_fr(R_EDX, FRn);
                                FSQRT_st0();
                                FDIVP_st(1);
                                pop_fr(R_EDX, FRn);
                                JMP_TARGET(end);
                                }
                                break;
                            case 0x8:
                                { /* FLDI0 FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                /* IFF PR=0 */
                                  check_fpuen();
                                  load_spreg( R_ECX, R_FPSCR );
                                  TEST_imm32_r32( FPSCR_PR, R_ECX );
                                  JNE_rel8(8, end);
                                  XOR_r32_r32( R_EAX, R_EAX );
                                  load_spreg( R_ECX, REG_OFFSET(fr_bank) );
                                  store_fr( R_ECX, R_EAX, FRn );
                                  JMP_TARGET(end);
                                }
                                break;
                            case 0x9:
                                { /* FLDI1 FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                /* IFF PR=0 */
                                  check_fpuen();
                                  load_spreg( R_ECX, R_FPSCR );
                                  TEST_imm32_r32( FPSCR_PR, R_ECX );
                                  JNE_rel8(11, end);
                                  load_imm32(R_EAX, 0x3F800000);
                                  load_spreg( R_ECX, REG_OFFSET(fr_bank) );
                                  store_fr( R_ECX, R_EAX, FRn );
                                  JMP_TARGET(end);
                                }
                                break;
                            case 0xA:
                                { /* FCNVSD FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                check_fpuen();
                                load_spreg( R_ECX, R_FPSCR );
                                TEST_imm32_r32( FPSCR_PR, R_ECX );
                                JE_rel8(9, end); // only when PR=1
                                load_fr_bank( R_ECX );
                                push_fpul();
                                pop_dr( R_ECX, FRn );
                                JMP_TARGET(end);
                                }
                                break;
                            case 0xB:
                                { /* FCNVDS FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
                                check_fpuen();
                                load_spreg( R_ECX, R_FPSCR );
                                TEST_imm32_r32( FPSCR_PR, R_ECX );
                                JE_rel8(9, end); // only when PR=1
                                load_fr_bank( R_ECX );
                                push_dr( R_ECX, FRm );
                                pop_fpul();
                                JMP_TARGET(end);
                                }
                                break;
                            case 0xE:
                                { /* FIPR FVm, FVn */
                                uint32_t FVn = ((ir>>10)&0x3); uint32_t FVm = ((ir>>8)&0x3); 
                                check_fpuen();
                                load_spreg( R_ECX, R_FPSCR );
                                TEST_imm32_r32( FPSCR_PR, R_ECX );
                                JNE_rel8(44, doubleprec);
                                
                                load_fr_bank( R_ECX );
                                push_fr( R_ECX, FVm<<2 );
                                push_fr( R_ECX, FVn<<2 );
                                FMULP_st(1);
                                push_fr( R_ECX, (FVm<<2)+1);
                                push_fr( R_ECX, (FVn<<2)+1);
                                FMULP_st(1);
                                FADDP_st(1);
                                push_fr( R_ECX, (FVm<<2)+2);
                                push_fr( R_ECX, (FVn<<2)+2);
                                FMULP_st(1);
                                FADDP_st(1);
                                push_fr( R_ECX, (FVm<<2)+3);
                                push_fr( R_ECX, (FVn<<2)+3);
                                FMULP_st(1);
                                FADDP_st(1);
                                pop_fr( R_ECX, (FVn<<2)+3);
                                JMP_TARGET(doubleprec);
                                }
                                break;
                            case 0xF:
                                switch( (ir&0x100) >> 8 ) {
                                    case 0x0:
                                        { /* FSCA FPUL, FRn */
                                        uint32_t FRn = ((ir>>9)&0x7)<<1; 
                                        check_fpuen();
                                        load_spreg( R_ECX, R_FPSCR );
                                        TEST_imm32_r32( FPSCR_PR, R_ECX );
                                        JNE_rel8( 21, doubleprec );
                                        load_fr_bank( R_ECX );
                                        ADD_imm8s_r32( (FRn&0x0E)<<2, R_ECX );
                                        load_spreg( R_EDX, R_FPUL );
                                        call_func2( sh4_fsca, R_EDX, R_ECX );
                                        JMP_TARGET(doubleprec);
                                        }
                                        break;
                                    case 0x1:
                                        switch( (ir&0x200) >> 9 ) {
                                            case 0x0:
                                                { /* FTRV XMTRX, FVn */
                                                uint32_t FVn = ((ir>>10)&0x3); 
                                                check_fpuen();
                                                load_spreg( R_ECX, R_FPSCR );
                                                TEST_imm32_r32( FPSCR_PR, R_ECX );
                                                JNE_rel8( 30, doubleprec );
                                                load_fr_bank( R_EDX );                 // 3
                                                ADD_imm8s_r32( FVn<<4, R_EDX );        // 3
                                                load_xf_bank( R_ECX );                 // 12
                                                call_func2( sh4_ftrv, R_EDX, R_ECX );  // 12
                                                JMP_TARGET(doubleprec);
                                                }
                                                break;
                                            case 0x1:
                                                switch( (ir&0xC00) >> 10 ) {
                                                    case 0x0:
                                                        { /* FSCHG */
                                                        check_fpuen();
                                                        load_spreg( R_ECX, R_FPSCR );
                                                        XOR_imm32_r32( FPSCR_SZ, R_ECX );
                                                        store_spreg( R_ECX, R_FPSCR );
                                                        }
                                                        break;
                                                    case 0x2:
                                                        { /* FRCHG */
                                                        check_fpuen();
                                                        load_spreg( R_ECX, R_FPSCR );
                                                        XOR_imm32_r32( FPSCR_FR, R_ECX );
                                                        store_spreg( R_ECX, R_FPSCR );
                                                        update_fr_bank( R_ECX );
                                                        }
                                                        break;
                                                    case 0x3:
                                                        { /* UNDEF */
                                                        if( sh4_x86.in_delay_slot ) {
                                                    	SLOTILLEGAL();
                                                        } else {
                                                    	JMP_exit(EXIT_ILLEGAL);
                                                    	return 1;
                                                        }
                                                        }
                                                        break;
                                                    default:
                                                        UNDEF();
                                                        break;
                                                }
                                                break;
                                        }
                                        break;
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xE:
                        { /* FMAC FR0, FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        check_fpuen();
                        load_spreg( R_ECX, R_FPSCR );
                        load_spreg( R_EDX, REG_OFFSET(fr_bank));
                        TEST_imm32_r32( FPSCR_PR, R_ECX );
                        JNE_rel8(18, doubleprec);
                        push_fr( R_EDX, 0 );
                        push_fr( R_EDX, FRm );
                        FMULP_st(1);
                        push_fr( R_EDX, FRn );
                        FADDP_st(1);
                        pop_fr( R_EDX, FRn );
                        JMP_rel8(16, end);
                        JMP_TARGET(doubleprec);
                        push_dr( R_EDX, 0 );
                        push_dr( R_EDX, FRm );
                        FMULP_st(1);
                        push_dr( R_EDX, FRn );
                        FADDP_st(1);
                        pop_dr( R_EDX, FRn );
                        JMP_TARGET(end);
                        }
                        break;
                    default:
                        UNDEF();
                        break;
                }
                break;
        }

    if( sh4_x86.in_delay_slot ) {
	ADD_imm8s_r32(2,R_ESI);
	sh4_x86.in_delay_slot = FALSE;
	return 1;
    } else {
	INC_r32(R_ESI);
    }
    return 0;
}
