/**
 * $Id: xlat.h 931 2008-10-31 02:57:59Z nkeynes $
 * 
 * Internal translation data structures and functions.
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

#ifndef lxdream_xlat_H
#define lxdream_xlat_H 1

#include "xlat/xiropt.h"
#include "xlat/xltcache.h"

typedef struct target_data *target_data_t;
    
/**
 * Source machine description. This should be immutable once constructed.
 **/
struct xlat_source_machine {
    const char *name;
    void *state_data;   /* Pointer to source machine state structure */
    uint32_t pc_offset; /* Offset of source PC, relative to state_data */
    uint32_t delayed_pc_offset; /* Offset of source delayed PC offset, relative to state_data */
    uint32_t t_offset; /* Offset of source T reg, relative to state_data */
    uint32_t m_offset;
    uint32_t q_offset;
    uint32_t s_offset;
  
    /**
     * Return the name of the register with the given type,
     * or NULL if no such register exists
     */
    const char * (*get_register_name)( uint32_t reg, xir_type_t type );
    
    /**
     * Decode a basic block of instructions from start, stopping after a
     * control transfer or before the given end instruction.
     * @param sd source data. This method should set the address_space field.
     * @return the pc value after the last decoded instruction.
     */ 
    uint32_t (*decode_basic_block)(xir_basic_block_t xbb);
};


/* Target machine description (no these are not meant to be symmetrical) */
struct xlat_target_machine {
    const char *name;

    /* Required functions */
    
    /**
     * Return the name of the register with the given type,
     * or NULL if no such register exists
     */
    const char * (*get_register_name)( uint32_t reg, xir_type_t type );

    /**
     * Test if the given operands are legal for the opcode. Note that it is assumed that
     * target register operands are always legal. This is used by the register allocator
     * to determine when it can fuse load/stores with another operation.
     */
    gboolean (*is_legal)( xir_opcode_t op, xir_operand_form_t arg0, xir_operand_form_t arg1 );

    /**
     * Lower IR closer to the machine, handling machine-specific issues that can't
     * wait until final code-gen. Can add additional instructions where required.
     */
    void (*lower)( xir_basic_block_t xbb, xir_op_t begin, xir_op_t end );

    /**
     * Determine the memory required to emit code for the specified block excluding
     * exceptions. This can be an overestimate,
     * as long as it is at least large enough for the final code.
     * @param begin start of code block
     * @param end end of code block
     * @return estimated size of emitted code.
     */
    uint32_t (*get_code_size)( xir_op_t begin, xir_op_t end );

    /**
     * Final target code generation.
     * @param td target_data information.
     * @param begin start of code block
     * @param end end of code block
     * @param exception_table Table of pointers to exception code 
     * @return number of bytes actually emitted.
     */
    uint32_t (*codegen)( target_data_t td, xir_op_t begin, xir_op_t end ); 
};


/**
 * Fixup records generated while assembling the code. Records are one of the
 * following types:
 *    Constant (32 or 64-bit)
 *    Exception (from a memory call or RAISEME instruction)
 *    
 * Relocations may be 32/64 bit absolute or 32-bit PC-relative. The value in the
 * relocation cell is taken as the addend if nonzero. For relative relocations, 
 * the relative displacement is calculated from the end of the fixup value - 
 * that is, for a REL32, the result will be
 *   *fixup_loc += &target - (fixup_loc+4)
 *  
 * In principle we could use a global constant table, but that adds complexity
 * at this stage. 
 */

#define TARGET_FIXUP_CONST32  0x00  
#define TARGET_FIXUP_CONST64  0x01
#define TARGET_FIXUP_RAISE    0x02
#define TARGET_FIXUP_RAISEEXT 0x03 /* An exception that can be raised from outside the generated code */
#define TARGET_FIXUP_OFFSET   0x04 /* Offset within the code block */
#define TARGET_FIXUP_POINTER  0x05 /* Absolute pointer */

#define TARGET_FIXUP_ABS32    0x00
#define TARGET_FIXUP_ABS64    0x10
#define TARGET_FIXUP_REL32    0x20
#define TARGET_FIXUP_REL64    0x30

#define TARGET_FIXUP_TARGET(x) ((x)&0x0F)
#define TARGET_FIXUP_MODE(x)   ((x)&0xF0)

typedef struct target_fixup_struct {
    int fixup_type; /* Combination of TARGET_FIXUP flags above */
    uint32_t fixup_offset; /* Location of fixup (to be modified) relative to start of block */
    uint32_t target_offset;
    union {
        uint32_t i;
        uint64_t q;
        float f;
        double d;
        void *p;
        xir_op_t *exc;
    } value;
} *target_fixup_t;

/**
 * Temporary data maintained during code generation
 */
struct target_data {
    struct xlat_target_machine *mach;
    struct xlat_source_macine *src;
    xlat_cache_block_t block;
    uint8_t *xlat_output;
    target_fixup_t fixup_table;
    int fixup_table_posn;
    int fixup_table_size;
};

/** Add fixup to a 32-bit constant memory value, adding the value to the constant table */
void target_add_const32_fixup( target_data_t td, int mode, void *location, uint32_t i );
/** Add fixup to a 64-bit constant memory value, adding the value to the constant table */
void target_add_const64_fixup( target_data_t td, int mode, void *location, uint64_t i );
/** Add fixup to an internal exception handler block */
void target_add_raise_fixup( target_data_t td, int type, void *location, xir_op_t *exc );
/** Add fixup to an externally accessible exception handle block */
void target_add_raiseext_fixup( target_data_t td, int type, void *location, xir_op_t *exc );
/** Add fixup to an arbitrary offset within the code block */
void target_add_offset_fixup( target_data_t td, int type, void *location, uint32_t off );
/** Add fixup to an arbitrary pointer */
void target_add_pointer_fixup( target_data_t td, int type, void *location, void *p );

/**
 * Generate final code for the block.
 * @return entry point of the code block.
 */ 
void *target_codegen( xlat_target_machine_t target, xir_basic_block_t xbb );   

#endif /* lxdream_xlat_H */
