/**
 * $Id: target.c 931 2008-10-31 02:57:59Z nkeynes $
 * 
 * Target code-generation support - provides a generic harness around the raw
 * (machine-specific) code emitter.
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

#include <stdlib.h>

#include "lxdream.h"
#include "xlat/xir.h"
#include "xlat/machine.h"

#define DEFAULT_FIXUP_TABLE_SIZE 4096
#define ALIGN32(p) p += ((-(uintptr_t)p)&0x03)
#define ALIGN64(p) p += ((-(uintptr_t)p)&0x07)

/**
 * Currently we use a single static target_data so that we can reuse the 
 * allocated memory (and we only do one codegen at a time anyway). However
 * we keep this private so that other modules can't assume there's only one TD.
 */
static struct target_data TD; 

/**
 * Add a new fixup without setting a target value
 */
static target_fixup_t target_add_fixup( target_data_t td, int type, void *location ) 
{
    if( td->fixup_table_posn == td->fixup_table_size ) {
        td->fixup_table_size <<= 1;
        td->fixup_table = realloc(td->fixup_table, td->fixup_table_size * sizeof(struct target_fixup_struct));
        assert( td->fixup_table != NULL );
    }
    target_fixup_t fixup = &td->fixup_table[td->fixup_table_posn++];
    fixup->fixup_type = type | TARGET_FIXUP_CONST32;
    fixup->fixup_offset = ((uint8_t *)location) - (uint8_t *)&td->block->code[0];
    return fixup;
}    

void target_add_const32_fixup( target_data_t td, int mode, void *location, uint32_t i )
{
    target_add_fixup(td, mode|TARGET_FIXUP_CONST32, location)->value.i = i;
}

void target_add_const64_fixup( target_data_t td, int mode, void *location, uint64_t q )
{
    target_add_fixup(td, mode|TARGET_FIXUP_CONST64, location)->value.q = q;
}

void target_add_raise_fixup( target_data_t td, int type, void *location, xir_op_t *exc )
{
    target_add_fixup(td, mode|TARGET_FIXUP_RAISE, location)->value.exc = exc;
}

void target_add_raiseext_fixup( target_data_t td, int type, void *location, xir_op_t *exc )
{
    target_add_fixup(td, mode|TARGET_FIXUP_RAISEEXT, location)->value.exc = exc;
}

void target_add_offset_fixup( target_data_t td, int type, void *location, uint32_t off )
{
    target_add_fixup(td, mode|TARGET_FIXUP_OFFSET, location)->target_offset = off;
}

void target_add_pointer_fixup( target_data_t td, int type, void *location, void *p )
{
    target_add_fixup(td, mode|TARGET_FIXUP_POINTER, location)->value.p = p;
}



void target_ensure_space( target_data_t td, int space_required )
{
    uint8_t *oldstart = td->block->code;
    uint32_t new_size = (td->xlat_output - oldstart) + space_required;
    if( new_size < td->block->size ) {
        xlat_current_block = xlat_extend_block( xlat_output - oldstart + MAX_INSTRUCTION_SIZE );
        eob = xlat_current_block->code + xlat_current_block->size;
        td->block = xlat_extend_block( new_size );
        xlat_output = td->block->code + (xlat_output - oldstart);        
    }
}

/**
 * Generate the exception table and exception bodies from the fixup data
 * Note that this may add additional constants to the fixup table.
 */
static void target_gen_exception_table( )
{
    int exc_size = 0, num_raiseext = 0;
    
    for( target_fixup_t fixup = &TD.fixup_table[0]; fixup != &TD.fixup_table[TD.fixup_table_posn]; fixup++ ) {
        int type = 
        switch(TARGET_FIXUP_TARGET(fixup->type)) {
        case TARGET_FIXUP_RAISEEXT:
            num_raiseext++;
            /* fallthrough */
        case TARGET_FIXUP_RAISE:
            exc_size += TD.get_code_size(fixup->value.exc, NULL);
        }
    }

    ALIGN64(TD.xlat_output);
    target_ensure_space( td, exc_size + num_raiseext*sizeof(struct xlat_exception_record) );
    uint8_t *blockstart = &TD.block->code[0];
    struct xlat_exception_record *exc_record = (struct xlat_exception_record *)TD.xlat_output;
    TD.block->exc_table_offset = TD.xlat_output - blockstart;
    TD.block->exc_table_size = num_raiseext;
    TD.xlat_output += (num_raiseext*sizeof(struct xlat_exception_record));
    
    for( target_fixup_t fixup = &TD.fixup_table[0]; fixup != &td_fixup_table[TD.fixup_table_posn]; fixup++ ) {
        switch( TARGET_FIXUP_TARGET(fixup->type) ) {
        case TARGET_FIXUP_RAISEEXT:
            exc_record->xlat_pc_offset = fixup->fixup_offset + 4;
            exc_record->xlat_exc_offset = TD.xlat_output - blockstart;
            /* fallthrough */
        case TARGET_FIXUP_RAISE:
            fixup->target_offset = TD.xlat_output - blockstart;
            TD.codegen( td, fixup->value.exc, NULL );
        }
    }
}

/** 
 * Generate constant table from the fixup data.
 */
static void target_gen_constant_table( )
{
    int numconst32=0, numconst64=0;

    /* Determine table size */
    for( target_fixup_t fixup = &TD.fixup_table[0]; fixup != &td_fixup_table[TD.fixup_table_posn]; fixup++ ) {
        int type = TARGET_FIXUP_TARGET(fixup->type);
        if( type == TARGET_FIXUP_CONST32 ) {
            numconst32++;
        } else if( type == TARGET_FIXUP_CONST64 ) {
            numconst64++;
        }
    }
    
    if( numconst64 != 0 ) {
        ALIGN64(TD.xlat_output);
    } else if( numconst32 != 0 ) {
        ALIGN32(TD.xlat_output);
    } else {
        return; /* no constants */
    }
    target_ensure_space( td, numconst64*8 + numconst32*4 );
    uint8_t *blockstart = &TD.block->code[0];
    
    /* TODO: Merge reused constant values */
    uint64_t *const64p = (uint64_t *)TD.xlat_output;
    uint32_t *const32p = (uint32_t *)(TD.xlat_output + numconst64*8);
    TD.xlat_output += (numconst64*8 + numconst32*4);
    
    for( target_fixup_t fixup = &TD.fixup_table[0]; fixup != &td_fixup_table[TD.fixup_table_posn]; fixup++ ) {
        switch(TARGET_FIXUP_TARGET(fixup->type)) {
        case TARGET_FIXUP_CONST32:
            fixup->target_offset =  ((uint8_t *)const32p) - blockstart;  
            *const32p++ = fixup->value.i;
            break;
        case TARGET_FIXUP_CONST64:
            fixup->target_offset =  ((uint8_t *)const64p) - blockstart;  
            *const64p++ = fixup->value.q;
            break;
        }
    }
}

/**
 * Apply all target fixups - assumes exceptions + constants have already been
 * generated.
 */
static void target_apply_fixups( )
{
    for( target_fixup_t fixup = &TD.fixup_table[0]; fixup != &TD.fixup_table[TD.fixup_table_posn]; fixup++ ) {
        void *target;
        if( TARGET_FIXUP_TARGET(fixup->type) == TARGET_FIXUP_POINTER ) {
            target = fixup->value.p;
        } else {
            target = &TD.block->code[fixup->target_offset];
        }
        
        uint32_t *loc = (uint32_t *)TD.block->code[fixup->fixup_offset];
        uint64_t *loc64 = (uint64_t *)TD.block->code[fixup->fixup_offset];
        switch(TARGET_FIXUP_MODE(fixup->fixup_type)) {
        case TARGET_FIXUP_REL32:
            *loc += (uint8_t *)target - (uint8_t *)(loc+1);
            break;
        case TARGET_FIXUP_REL64:
            *loc64 += (uint8_t *)target - (uint8_t *)(loc64+1);
            break;
        case TARGET_FIXUP_ABS32:
            *loc += (uint32_t)target;
            break;
        case TARGET_FIXUP_ABS64:
            *loc64 += (uint64_t)target;
            break;
        }
    }
}

void target_codegen( xlat_target_machine_t machine, source_data_t sd )
{
    /* Setup the target data struct */
    TD.mach = machine;
    TD.src = sd->machine;
    TD.block = xlat_start_block( sd->pc_start );
    TD.xlat_output = &TD.block->code[0];
    if( TD.fixup_table == NULL ) {
        if( TD.fixup_table == NULL ) {
            TD.fixup_table_size = DEFAULT_FIXUP_TABLE_SIZE;
            TD.fixup_table = malloc( td->fixup_table_size * sizeof(struct target_fixup_struct) );
            assert( TD.fixup_table != NULL );
        }
    }
    TD.fixup_table_posn = 0;
    
    uint32_t code_size = machine->get_code_size(sd->ir_begin,sd->ir_end);
    target_ensure_space(&TD, code_size);
    
    machine->codegen(&TD, sd->begin, sd->end);
    
    target_gen_exception_table();
    target_gen_constant_table();
    target_apply_fixups();
    
    xlat_commit_block( TD.xlat_output - &TD.block->code[0], sd->pc_end-sd->pc_start );
    return &TD.block->code[0];
}
