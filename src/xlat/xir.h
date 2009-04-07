/**
 * $Id: xir.h 931 2008-10-31 02:57:59Z nkeynes $
 * 
 * This file defines the translation IR and associated functions.
 *
 * Copyright (c) 2009 Nathan Keynes.
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

#ifndef lxdream_xir_H
#define lxdream_xir_H 1

#include <stdint.h>

/*****************************************************************************
 *
 * We use a very simple low-level 2-op instruction form, largely intended to
 * closely match the x86 ISA to simplify final code generation. Complex
 * instructions are either broken up into simpler ops, or inserted as 
 * opaque macros. First operand is source, second operand is destination.
 *
 * Data types are encoded in the instruction:
 *    Byte   (B) 8-bit integer
 *    Word   (W) 16-bit integer
 *    Long   (L) 32-bit integer
 *    Quad   (Q) 64-bit integer
 *    Float  (F) 32-bit floating point
 *    Double (D) 64-bit floating point
 *    Vec4   (V) 4x32-bit floating point
 *    Matrix (M) 4x4x32-bit floating point in column-major order
 * This is not an exhaustive list, but it is sufficient to cover all operations
 * required for the SH4.
 * 
 * ALU instructions come in two variants, xxxS which modifies the condition 
 * flags, and the regular xxx version that does not. Implementations are assumed
 * to have at least the standard NZVC flags available (or will have to fake it) 
 * 
 * Variations in flag behaviour between implementations need to be accounted for
 * somehow.
 ****************************************************************************/

/* Registers 0..127 belong to the source machine, all higher numbers are temporaries */
#define MIN_SOURCE_REGISTER 0
#define MAX_SOURCE_REGISTER 1023
#define MIN_TEMP_REGISTER 1024
#define MAX_TEMP_REGISTER 1535

/* Target registers have a separate 'address' space. */
#define MIN_TARGET_REGISTER 0
#define MAX_TARGET_REGISTER 127

/* Convenience defines */
#define REG_TMP0 (MIN_TEMP_REGISTER)
#define REG_TMP1 (MIN_TEMP_REGISTER+1)
#define REG_TMP2 (MIN_TEMP_REGISTER+2)
#define REG_TMP3 (MIN_TEMP_REGISTER+3)
#define REG_TMP4 (MIN_TEMP_REGISTER+4)
#define REG_TMP5 (MIN_TEMP_REGISTER+5)

#define REG_TMPQ0 (MIN_TEMP_REGISTER+128)
#define REG_TMPQ1 (MIN_TEMP_REGISTER+129)

/**
 * Operands are either integer, float, or double, and are either immediate or
 * assigned to a source-machine register, destination-machine register, or a
 * temporary register. (All temporaries have to be resolved to a dest-reg before
 * code generation)
 */
typedef enum {
    NO_OPERAND = 0,
    SOURCE_REGISTER_OPERAND =1, // Source (or temp) register
    TARGET_REGISTER_OPERAND =2,
    INT_IMM_OPERAND = 3,
    QUAD_IMM_OPERAND = 4,
    FLOAT_IMM_OPERAND = 5,
    DOUBLE_IMM_OPERAND = 6,
    POINTER_OPERAND = 7, // Native target pointer, eg direct memory access
} xir_operand_type_t;

typedef struct xir_operand {
    xir_operand_type_t type;
    union {
        uint32_t i;
        uint64_t q;
        float f;
        double d;
        void *p;
    } value;
} *xir_operand_t;

/* Condition codes */
typedef enum {
    CC_TRUE = -1, /* Always */
    CC_OV   = 0,  /* Overflow */
    CC_NO   = 1,  /* !Overflow */
    CC_UGE  = 2,  /* Unsigned greater or equal */
    CC_ULT  = 3,  /* Unsigned less than */
    CC_ULE  = 4,  /* Unsigned less or equal */
    CC_UGT  = 5,  /* Unsigned greater than */
    CC_EQ   = 6,  /* Equal */
    CC_NE   = 7,  /* !Equal */
    CC_NEG  = 8,  /* Negative */
    CC_POS  = 9,  /* Not-negative (positive or zero) */ 
    CC_SGE  = 10,
    CC_SLT  = 11,
    CC_SLE  = 12,
    CC_SGT  = 13
} xir_cc_t;

#define CC_C  CC_ULT
#define CC_NC CC_UGE

typedef enum {
    // No operands
    OP_NOP     = 0,
    OP_BARRIER, // Direction to register allocator - Ensure all state is committed

    // One operand
    OP_DEC,        /* Decrement and set Z if result == 0 */
    OP_LD,        /* Load flags from reg/imm (1 = condition, 0 = !condition) */
    OP_ST,        /* Set reg to 1 on condition, 0 on !condition */
    OP_RESTFLAGS,    /* Restore flags from register */
    OP_SAVEFLAGS,    /* Save flags into register */
    OP_ENTER,     // Block start - immediate operand is a bitmask of target registers used
    OP_BRREL,
    OP_BR,
    OP_CALL0,  // Call function with no arguments or return value
    OP_OCBI,
    OP_OCBP,
    OP_OCBWB,
    OP_PREF,

    // Register moves */
    OP_MOV, 
    OP_MOVQ,
    OP_MOVV,
    OP_MOVM,
    OP_MOVSX8,
    OP_MOVSX16,
    OP_MOVSX32,
    OP_MOVZX8,
    OP_MOVZX16,
    OP_MOVZX32,

    /* ALU */
    OP_ADD,
    OP_ADDS,
    OP_ADDC,
    OP_ADDCS,
    OP_AND,
    OP_ANDS,
    OP_CMP,
    OP_DIV,    /* Unsigned division */
    OP_DIVS,   /* Unsigned divison and update flags */
    OP_MUL,
    OP_MULS,
    OP_MULQ,
    OP_MULQS,
    OP_NEG,
    OP_NEGS,
    OP_NOT,
    OP_NOTS,
    OP_OR,
    OP_ORS,
    OP_RCL,
    OP_RCR,
    OP_ROL,    /* Rotate left w/o updating flags */
    OP_ROLS,   /* Rotate left, and set carry */ 
    OP_ROR,    /* Rotate right */
    OP_RORS,   /* Rotate right and set carry */
    OP_SAR,    /* Shift arithmetic right */
    OP_SARS,   /* Shift arithmetic right and set carry */
    OP_SDIV,   /* Signed division */
    OP_SDIVS,  /* Signed division and update flags */
    OP_SLL,    /* Shift logical left */
    OP_SLLS,   /* Shift logical left and set carry */
    OP_SLR,    /* Shift logical right */
    OP_SLRS,   /* Shift logical right and set carry */
    OP_SUB,    /* Subtract, no flags changed/used */
    OP_SUBS,   /* Subtract, flag set on overflow */
    OP_SUBB,    /* Subtract with borrow */
    OP_SUBBS,   /* Subtract with borrow and set carry */
    OP_SHUFFLE, /* Rearrange bytes according to immediate pattern */ 
    OP_TST,
    OP_XOR,
    OP_XORS,
    OP_XLAT,
    
    /* FPU */
    OP_ABSD,
    OP_ABSF,
    OP_ABSV,
    OP_ADDD,
    OP_ADDF,
    OP_ADDV,
    OP_CMPD,
    OP_CMPF,
    OP_DIVD,
    OP_DIVF,
    OP_DIVV,
    OP_MULD,
    OP_MULF,
    OP_MULV,
    OP_NEGD,
    OP_NEGF,
    OP_NEGV,
    OP_SQRTD,
    OP_SQRTF,
    OP_SQRTV,
    OP_RSQRTD,
    OP_RSQRTF,
    OP_RSQRTV,
    OP_SUBD,
    OP_SUBF,
    OP_SUBV,
    OP_DTOF,
    OP_DTOI,
    OP_FTOD,
    OP_FTOI,
    OP_ITOD,
    OP_ITOF,
    OP_SINCOSF,
    OP_DOTPRODV,
    OP_MATMULV,

    // Memory operations - these all indirect through the memory tables.
    OP_LOADB,
    OP_LOADBFW,
    OP_LOADW,
    OP_LOADL,
    OP_LOADQ,
    OP_STOREB,
    OP_STOREW,
    OP_STOREL,
    OP_STOREQ,
    OP_STORELCA,
    
    OP_BRCOND,
    OP_BRCONDDEL, // Delayed branch - sets newpc rather than pc (and is not a terminator)
    OP_RAISEME, // imm mask in, reg in - branch to exception if (reg & mask) == 0
    OP_RAISEMNE, // imm mask in, reg in - branch to exception if (reg & mask) != 0


    // Native calls (not source machine calls)
    OP_CALLLUT, // Call indirect through base pointer (reg) + displacement
    OP_CALL1,  // Call function with single argument and no return value
    OP_CALLR,  // Call function with no arguments and a single return value

    /********************** SH4-specific macro operations *************************/
    /* TODO: These need to be broken down into smaller operations eventually, 
     * especially as some are likely to be partially optimizable. But in the
     * meantime this at least gets things working
     */
    
    /**
     * ADDQSAT32 Rm, Rn - 64-bit Add Rm to Rn, saturating to 32-bits if S==1 (per SH4 MAC.W) 
     * 
     * if R_S == 0 ->
     *     Rn += Rm
     * else ->
     *     if overflow32( Rn + Rm ) ->
     *         Rn = saturate32( Rn + Rm ) | 0x100000000
     *     else ->
     *         Rn += Rm
     */
    OP_ADDQSAT32, 
    
    /**
     * ADDSAT48 Rm, Rn - 64-bit Add Rm to Rn, saturating to 48-bits if S==1 (per SH4 MAC.L)
     * 
     * if R_S == 0 ->
     *     Rn += Rm
     * else ->
     *     if( Rm + Rn > 0x00007FFFFFFFFFFF ) ->
     *          Rn = 0x00007FFFFFFFFFFF
     *     else if( Rm + Rn < 0x0000800000000000 ) ->
     *          Rn = 0x0000800000000000
     *     else ->
     *          Rn += Rm
     */
    OP_ADDQSAT48,
    
    /**
     * CMP/STR Rm, Rn - Set T if any byte is the same between Rm and Rn
     * 
     * Macro expansion:
     *   MOV Rm, %tmp
     *   XOR Rn, %tmp
     *   TEST   0x000000FF, %tmp     
     *   TESTne 0x0000FF00, %tmp      
     *   TESTne 0x00FF0000, %tmp      
     *   TESTne 0xFF000000, %tmp      
     *   SETe T
     * 
     */
    OP_CMPSTR,
    
    /**
     * DIV1 Rm,Rn performs a single-step division of Rm/Rn, modifying flags
     * as it goes.
     * 
     * sign = Rn >> 31
     * Rn = (Rn << 1) | R_T
     * If R_Q == R_M -> Rn = Rn - Rm
     * Else           -> Rn = Rn + Rm
     * R_Q = sign ^ R_M ^ (Rn>>31)
     * R_T = (R_Q == R_M)    ; or newq == (rn>>31)
     * 
     * Macro expansion:
     *   LDc R_T
     *   RCL 1, Rn
     *   SETc temp
     *   CMP R_Q, R_M
     *   ADDne Rm, Rn
     *   SUBeq Rm, Rn
     *   MOV Rn, R_Q
     *   SHR 31, Rn
     *   XOR temp, R_Q
     *   XOR R_M, R_Q
     *   CMP R_M, R_Q
     *   SETe R_T
     */
    OP_DIV1, 
    
    /**
     * SHAD Rm, Rn performs an arithmetic shift of Rn as follows:
     *   If Rm >= 0 -> Rn = Rn << (Rm&0x1F)
     *   If Rm < 0  -> 
     *      If Rm&0x1F == 0 -> Rn = Rn >> 31
     *      Else            -> Rn = Rn >> 32 - (Rm&0x1F)
     * 
     *   CMP 0, Rm
     *   ANDuge 0x1F, Rm
     *   SLLuge Rm, Rn
     *   ORult 0xFFFFFFE0, Rm
     *   NEGult Rm
     *   SARult Rm, Rn ; unmasked shift
     *   
     */
    OP_SHAD,   // Shift dynamic arithmetic (left or right)
    
    /**
     * SHLD Rm, Rn performs a logical shift of Rn as follows:
     *   If Rm >= 0 -> Rn = Rn << (Rm&0x1F)
     *   If Rm < 0  -> 
     *      If Rm&0x1F == 0 -> Rn = 0
     *      Else            -> Rn = Rn >> 32 - (Rm&0x1F)
     */
    OP_SHLD,   // Shift dynamic logical (left or right)
} xir_opcode_t;

#define MAX_OP0_OPCODE OP_BARRIER
#define MAX_OP1_OPCODE OP_PREF
#define MAX_OP2_OPCODE OP_SHLD
#define NUM_OP0_OPCODES (MAX_OP0_OPCODE+1)
#define NUM_OP1_OPCODES (MAX_OP1_OPCODE-MAX_OP0_OPCODE)
#define NUM_OP2_OPCODES (MAX_OP2_OPCODE-MAX_OP1_OPCODE)
#define MAX_OPCODE (MAX_OP2_OPCODE)
#define NUM_OPCODES (MAX_OP2_OPCODE+1)

typedef struct xir_op {
    xir_opcode_t opcode;
    xir_cc_t cond;
    struct xir_operand operand[2];
    struct xir_op *next; /* Next instruction (normal path) - NULL in the case of the last instruction */
    struct xir_op *prev; /* Previous instruction (normal path) - NULL in the case of the first instruction */
    struct xir_op *exc;  /* Next instruction if the opcode takes an exception - NULL if no exception is possible */
} *xir_op_t;

/* Defined in xlat/xlat.h */
typedef struct xlat_source_machine *xlat_source_machine_t;
typedef struct xlat_target_machine *xlat_target_machine_t;

/**
 * Source data structure. This mainly exists to manage memory for XIR operations
 */
typedef struct xir_basic_block {
    xir_op_t ir_begin; /* Beginning of code block */
    xir_op_t ir_end; /* End of code block (Last instruction in code block) */
    xir_op_t ir_ptr; /* First unallocated instruction in allocation block */
    xir_op_t ir_alloc_begin; /* Beginning of memory allocation */
    xir_op_t ir_alloc_end; /* End of allocation */
    uint32_t pc_begin; /* first instruction */
    uint32_t pc_end;   /* next instruction after end */ 
    xlat_source_machine_t source;
    struct mem_region_fn **address_space; /* source machine memory access table */
} *xir_basic_block_t;

typedef int xir_offset_t;

/**************************** OP Information ******************************/

/* Instruction operand modes */
#define OPM_NO   0x000000  /* No operands */
#define OPM_R    0x000001  /* Single operand, read-only */
#define OPM_W    0x000002  /* Single operand, write-only */
#define OPM_RW   0x000003  /* Single operand, read-write */
#define OPM_R_R  0x000005  /* Two operands, both read-only */
#define OPM_R_W  0x000009  /* Two operands, first read-only, second write-only */
#define OPM_R_RW 0x00000D  /* Two operands, first read-only, second read-write */
#define OPM_I_I  0x000000  /* Both operands i32 */
#define OPM_Q_Q  0x000110  /* Both operands i64 */
#define OPM_I_Q  0x000100  /* i32,i64 operands */
#define OPM_Q_I  0x000010  /* i64,i32 operands */
#define OPM_F_F  0x000220  /* Both operands float */
#define OPM_D_D  0x000330  /* Both operands double */
#define OPM_I_F  0x000200  /* i32,float operands */
#define OPM_I_D  0x000300  /* i32,double operands */
#define OPM_F_I  0x000020  /* float,i32 operands */
#define OPM_D_I  0x000030  /* double,i32 operands */
#define OPM_F_D  0x000320  /* float,double operands */
#define OPM_D_F  0x000230  /* double,float operands */
#define OPM_V_V  0x000440  /* vec4,vec4 operands */
#define OPM_V_M  0x000540  /* vec4,matrix16 operands */
#define OPM_M_M  0x000550  /* mat16,mat16 operands */
#define OPM_TR   0x001000  /* Use T */
#define OPM_TW   0x002000  /* Set T */
#define OPM_TRW  0x003000  /* Use+Set T */
#define OPM_EXC  0x004000  /* May raise an exception, clobbers volatiles */
#define OPM_CLB  0x008000  /* Clobbers volatile registers */
#define OPM_CLBT 0x00C000  /* Clobbers 'temporary regs' but not the full volatile set */
#define OPM_TERM 0x010000 /* Terminates block. (Must be final instruction in block) */ 

#define OPM_R_R_TW   (OPM_R_R|OPM_TW)   /* Read two ops + set flags */
#define OPM_R_RW_TR  (OPM_R_RW|OPM_TR)  /* Read/write + use flags */
#define OPM_R_RW_TW  (OPM_R_RW|OPM_TW)  /* Read/write + set flags */
#define OPM_R_RW_TRW (OPM_R_RW|OPM_TRW) /* Read/write + use/set flags */
#define OPM_R_W_TW   (OPM_R_W|OPM_TW)  /* Read/write + set flags */
#define OPM_RW_TW    (OPM_RW|OPM_TW)    /* Read/write single op + set flags */
#define OPM_RW_TRW   (OPM_RW|OPM_TRW)   /* Read/write single op + use/set flags */
#define OPM_FRW      (OPM_RW|OPM_F_F)   /* Read/write single float op */
#define OPM_FR_FRW   (OPM_R_RW|OPM_F_F) /* Read/write float op pair */
#define OPM_FR_FW    (OPM_R_W|OPM_F_F) /* Read/write float op pair */
#define OPM_FR_FR_TW (OPM_R_R_TW|OPM_F_F) /* Read two float ops + set flags */ 
#define OPM_DRW      (OPM_RW|OPM_D_D)   /* Read/write single double op */
#define OPM_DR_DRW   (OPM_R_RW|OPM_D_D) /* Read/write double op pair */
#define OPM_DR_DW    (OPM_R_W|OPM_D_D) /* Read/write double op pair */
#define OPM_VR_VRW   (OPM_R_RW|OPM_V_V) /* Vector Read/write double op pair */
#define OPM_VR_VW    (OPM_R_W|OPM_V_V) /* Vector Read/write double op pair */
#define OPM_DR_DR_TW (OPM_R_R_TW|OPM_D_D) /* Read two double ops + set flags */

#define OPM_R_W_EXC  (OPM_R_W|OPM_EXC)  /* Read first, write second, possible exc (typical load) */
#define OPM_R_R_EXC  (OPM_R_R|OPM_EXC)  /* Read first, write second, possible exc (typical store) */
#define OPM_R_EXC    (OPM_R|OPM_EXC)    /* Read-only single op, possible exc (eg pref) */

struct xir_opcode_entry {
    char *name;
    int mode;
};

struct xir_symbol_entry {
    const char *name;
    void *ptr;
};

extern const struct xir_opcode_entry XIR_OPCODE_TABLE[]; 
#define XOP_IS_SRCREG(op,n) (op->operand[n].type == SOURCE_REGISTER_OPERAND)
#define XOP_IS_TGTREG(op,n) (op->operand[n].type == TARGET_REGISTER_OPERAND)
#define XOP_IS_INTIMM(op,n) (op->operand[n].type == INT_IMM_OPERAND)
#define XOP_IS_FLOATIMM(op,n) (op->operand[n].type == FLOAT_IMM_OPERAND)
#define XOP_IS_DOUBLEIMM(op,n) (op->operand[n].type == DOUBLE_IMM_OPERAND)
#define XOP_IS_QUADIMM(op,n) (op->operand[n].type == QUAD_IMM_OPERAND)
#define XOP_IS_PTRIMM(op,n) (op->operand[n].type == POINTER_OPERAND)
#define XOP_IS_IMM(op,n) (op->operand[n].type > TARGET_REGISTER_OPERAND)
#define XOP_IS_REG(op,n) (XOP_IS_SRCREG(op,n)||XOP_IS_TGTREG(op,n)
#define XOP_IS_FORM(op,t1,t2) (op->operand[0].type == t1 && op->operand[1].type == t2)

#define XOP_REG(op,n) (op->operand[n].value.i)
#define XOP_REG1(op) XOP_REG(op,0)
#define XOP_REG2(op) XOP_REG(op,1)
#define XOP_INT(op,n) (op->operand[n].value.i)
#define XOP_QUAD(op,n) (op->operand[n].value.q)
#define XOP_FLOAT(op,n) (op->operand[n].value.f)
#define XOP_DOUBLE(op,n) (op->operand[n].value.d)
#define XOP_PTR(op,n) (op->operand[n].value.p)

#define XOP_IS_TERMINATOR(op) (XIR_OPCODE_TABLE[op->opcode].mode & OPM_TERM)
#define XOP_HAS_0_OPERANDS(op) ((XIR_OPCODE_TABLE[op->opcode].mode & 0x0F) == 0)
#define XOP_HAS_1_OPERAND(op)  ((XIR_OPCODE_TABLE[op->opcode].mode & 0x0F) < 4)
#define XOP_HAS_2_OPERANDS(op)  ((XIR_OPCODE_TABLE[op->opcode].mode & 0x0C) != 0)
#define XOP_HAS_EXCEPTION(op) ((XIR_OPCODE_TABLE[op->opcode].mode & 0xC000) == OPM_EXC)

#define XOP_READS_OP1(op) (XIR_OPCODE_TABLE[op->opcode].mode & 0x01)
#define XOP_WRITES_OP1(op) (XIR_OPCODE_TABLE[op->opcode].mode & 0x02)
#define XOP_READS_OP2(op) (XIR_OPCODE_TABLE[op->opcode].mode & 0x04)
#define XOP_WRITES_OP2(op) (XIR_OPCODE_TABLE[op->opcode].mode & 0x08)
#define XOP_READS_FLAGS(op) ((XIR_OPCODE_TABLE[op->opcode].mode & OPM_TR) || (op->cond != CC_TRUE && op->opcode != OP_LD)) 
#define XOP_WRITES_FLAGS(op) (XIR_OPCODE_TABLE[op->opcode].mode & OPM_TW)

#define XOP_READS_REG1(op) (XOP_READS_OP1(op) && XOP_IS_REG(op,0))
#define XOP_WRITES_REG1(op) (XOP_WRITES_OP1(op) && XOP_IS_REG(op,0))
#define XOP_READS_REG2(op) (XOP_READS_OP2(op) && XOP_IS_REG(op,1))
#define XOP_WRITES_REG2(op) (XOP_WRITES_OP2(op) && XOP_IS_REG(op,1))

#define XOP_TYPE1(op) (op->operand[0].type)
#define XOP_TYPE2(op) (op->operand[1].type)
#define XOP_OPERAND(op,i) (&op->operand[i])

/******************************* OP Constructors ******************************/

xir_op_t xir_append_op2( xir_basic_block_t xbb, int op, int arg0type, uint32_t arg0, int arg1type, uint32_t arg1 );
xir_op_t xir_append_op2cc( xir_basic_block_t xbb, int op, int cc, int arg0type, uint32_t arg0, int arg1type, uint32_t arg1 );
xir_op_t xir_append_float_op2( xir_basic_block_t xbb, int op, float imm1, int arg1type, uint32_t arg1 );
xir_op_t xir_append_ptr_op2( xir_basic_block_t xbb, int op, void *arg0, int arg1type, uint32_t arg1 );


#define XOP1( op, arg0 )       xir_append_op2(xbb, op, SOURCE_REGISTER_OPERAND, arg0, NO_OPERAND, 0)
#define XOP1CC( op, cc, arg0 ) xir_append_op2cc(xbb, op, cc, SOURCE_REGISTER_OPERAND, arg0, NO_OPERAND, 0)
#define XOP1I( op, arg0 )      xir_append_op2(xbb, op, INT_IMM_OPERAND, arg0, NO_OPERAND, 0)
#define XOP2I( op, arg0, arg1 ) xir_append_op2(xbb, op, INT_IMM_OPERAND, arg0, SOURCE_REGISTER_OPERAND, arg1)
#define XOP2II( op, arg0, arg1 ) xir_append_op2(xbb, op, INT_IMM_OPERAND, arg0, INT_IMM_OPERAND, arg1)
#define XOP2IICC( op, cc, arg0, arg1 ) xir_append_op2cc(xbb, op, cc, INT_IMM_OPERAND, arg0, INT_IMM_OPERAND, arg1)
#define XOP2( op, arg0, arg1 ) xir_append_op2(xbb, op, SOURCE_REGISTER_OPERAND, arg0, SOURCE_REGISTER_OPERAND, arg1)
#define XOP2CC( op, cc, arg0, arg1 ) xir_append_op2cc(xbb, op, cc, SOURCE_REGISTER_OPERAND, arg0, SOURCE_REGISTER_OPERAND, arg1)
#define XOP2F( op, arg0, arg1 ) xir_append_float_op2(xbb, op, arg0, SOURCE_REGISTER_OPERAND, arg1) 
#define XOP2P( op, arg0, arg1 ) xir_append_ptr_op2(xbb, op, arg0, SOURCE_REGISTER_OPERAND, arg1) 
#define XOP0( op )             xir_append_op2(xbb, op, NO_OPERAND, 0, NO_OPERAND, 0)
#define XOPCALL0( arg0 )   xir_append_ptr_op2(xbb, OP_CALL0, arg0, NO_OPERAND, 0) 
#define XOPCALL1( arg0, arg1 ) xir_append_ptr_op2(xbb, OP_CALL1, arg0, SOURCE_REGISTER_OPERAND, arg1) 
#define XOPCALL1I( arg0, arg1 ) xir_append_ptr_op2(xbb, OP_CALL1, arg0, INT_IMM_OPERAND, arg1)
#define XOPCALLR( arg0, arg1 ) xir_append_ptr_op2(xbb, OP_CALLR, arg0, SOURCE_REGISTER_OPERAND, arg1)

/**************************** IR Modification ******************************/

/**
 * Insert a new instruction immediately before the given existing inst.
 */
void xir_insert_op( xir_op_t op, xir_op_t before );

/**
 * Insert the block start..end immediately before the given instruction
 */
void xir_insert_block( xir_op_t start, xir_op_t end, xir_op_t before );
                      
/**
 * Remove the specified instruction completely from the block in which it appears.
 * Note: removing terminators with this method may break the representation.
 * Op itself is not modified. 
 */
void xir_remove_op( xir_op_t op );

/**
 * Apply a shuffle directly to the given operand, and return the result
 */ 
uint32_t xir_shuffle_imm32( uint32_t shuffle, uint32_t operand );

/**
 * Apply a shuffle transitively to the operation (which must also be a shuffle).
 * For example, given the sequence
 *   op1: shuffle 0x2134, r12
 *   op2: shuffle 0x3412, r12
 * xir_trans_shuffle( 0x2134, op2 ) can be used to replace op2 wih
 *        shuffle 0x3421, r12
 */
void xir_shuffle_op( uint32_t shuffle, xir_op_t it );

/**
 * Return the number of instructions that would be emitted by xir_shuffle_lower
 * for the given instruction (not including the leading nop, if there is one)
 */
int xir_shuffle_lower_size( xir_op_t it );

/**
 * Transform a shuffle instruction into an equivalent sequence of shifts, and
 * logical operations.
 * @return the last instruction in the resultant sequence (which may be the
 * original instruction pointer). 
 */
xir_op_t xir_shuffle_lower( xir_basic_block_t xbb, xir_op_t it, int tmp1, int tmp2 );


/**************************** Debugging ******************************/

/**
 * Verify the integrity of an IR block - abort with assertion failure on any
 * errors.
 */
void xir_verify_block( xir_op_t start, xir_op_t end );

/**
 * Set the register name mappings for source and target registers - only really
 * used for debug output
 */
void xir_set_register_names( const char **source_regs, const char **target_regs );

/**
 * Set the symbol table mappings for target points - also only really for
 * debugging output.
 */
void xir_set_symbol_table( const struct xir_symbol_entry *symtab ); 

/**
 * Dump the specified block of IR to stdout
 */
void xir_dump_block( xir_op_t start, xir_op_t end );


#endif /* !lxdream_xir_H */
