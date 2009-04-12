/**
 * $Id: xir.h 931 2008-10-31 02:57:59Z nkeynes $
 * 
 * This file provides support functions for the translation IR.
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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "xlat/xir.h"
#include "xlat/xlat.h" 

static const struct xir_symbol_entry *xir_symbol_table = NULL;

static const char *XIR_CC_NAMES[] = {
        "ov", "no", "uge", "ult", "ule", "ugt", "eq", "ne",
        "neg", "pos", "sge", "slt", "sle", "sgt" };
static const char XIR_TYPE_CODES[] = { 'l', 'q', 'f', 'd', 'v', 'm' };

const int XIR_OPERAND_SIZE[] = { 4, 8, 4, 8, 16, 64, (sizeof(void *)), 0 };


const struct xir_opcode_entry XIR_OPCODE_TABLE[] = { 
        { "NOP", OPM_NO },
        { "BARRIER", OPM_NO | OPM_CLB },
        { "DEC", OPM_RW_TW },
        { "LD", OPM_R | OPM_TW },
        { "ST", OPM_W | OPM_TR },
        { "RESTFLAGS", OPM_R | OPM_TW },
        { "SAVEFLAGS", OPM_W | OPM_TR },
        { "ENTER", OPM_R },
        { "BRREL", OPM_R | OPM_TERM },
        { "BR", OPM_R | OPM_TERM },
        { "CALL0", OPM_R | OPM_CLB },
        { "OCBI", OPM_R_EXC },
        { "OCBP", OPM_R_EXC },
        { "OCBWB", OPM_R_EXC },
        { "PREF", OPM_R_EXC },
     
        { "MOV", OPM_R_W },
        { "MOVQ", OPM_R_W|OPM_Q_Q },
        { "MOVV", OPM_R_W|OPM_V_V },
        { "MOVM", OPM_R_W|OPM_M_M },
        { "MOVSX8", OPM_R_W },
        { "MOVSX16", OPM_R_W },
        { "MOVSX32", OPM_R_W|OPM_I_Q },
        { "MOVZX8", OPM_R_W },
        { "MOVZX16", OPM_R_W },
        { "MOVZX32", OPM_R_W|OPM_I_Q },
        
        { "ADD", OPM_R_RW },
        { "ADDS", OPM_R_RW_TW }, 
        { "ADDC", OPM_R_RW_TR },
        { "ADDCS", OPM_R_RW_TRW },
        { "AND", OPM_R_RW },
        { "ANDS", OPM_R_RW_TW },
        { "CMP", OPM_R_R_TW },
        { "DIV", OPM_R_RW },
        { "DIVS", OPM_R_RW_TW },
        { "MUL", OPM_R_RW },
        { "MULS", OPM_R_RW_TW },
        { "MULQ", OPM_R_RW|OPM_Q_Q },
        { "MULQS", OPM_R_RW_TW|OPM_Q_Q },
        { "NEG", OPM_R_W },
        { "NEGS", OPM_R_W_TW },
        { "NOT", OPM_R_W },
        { "NOTS", OPM_R_W_TW },
        { "OR", OPM_R_RW },
        { "ORS", OPM_R_RW_TW },
        { "RCL", OPM_R_RW_TRW },
        { "RCR", OPM_R_RW_TRW },
        { "ROL", OPM_R_RW },
        { "ROLS", OPM_R_RW_TW },
        { "ROR", OPM_R_RW },
        { "RORS", OPM_R_RW_TW },
        { "SAR", OPM_R_RW },
        { "SARS", OPM_R_RW_TW },
        { "SDIV", OPM_R_RW },
        { "SDIVS", OPM_R_RW_TW },
        { "SLL", OPM_R_RW },
        { "SLLS", OPM_R_RW_TW },
        { "SLR", OPM_R_RW },
        { "SLRS", OPM_R_RW_TW },
        { "SUB", OPM_R_RW },
        { "SUBS", OPM_R_RW_TW },
        { "SUBB", OPM_R_RW },
        { "SUBBS", OPM_R_RW_TRW },
        { "SHUFFLE", OPM_R_RW },
        { "TST", OPM_R_R_TW },
        { "XOR", OPM_R_RW },
        { "XORS", OPM_R_RW_TW },        
        
        { "ABSD", OPM_DR_DW },
        { "ABSF", OPM_FR_FW },
        { "ABSV", OPM_VR_VW },
        { "ADDD", OPM_DR_DRW },
        { "ADDF", OPM_FR_FRW },
        { "ADDV", OPM_VR_VRW },
        { "CMPD", OPM_DR_DR_TW },
        { "CMPF", OPM_FR_FR_TW },
        { "DIVD", OPM_DR_DRW },
        { "DIVF", OPM_FR_FRW },
        { "DIVV", OPM_VR_VRW },
        { "MULD", OPM_DR_DRW },
        { "MULF", OPM_FR_FRW },
        { "MULV", OPM_VR_VRW },
        { "NEGD", OPM_DR_DW },
        { "NEGF", OPM_FR_FW },
        { "NEGV", OPM_VR_VW },
        { "SQRTD", OPM_DR_DW },
        { "SQRTF", OPM_FR_FW },
        { "SQRTV", OPM_VR_VW },
        { "RSQRTD", OPM_DR_DW },
        { "RSQRTF", OPM_FR_FW },
        { "RSQRTV", OPM_VR_VW },
        { "SUBD", OPM_DR_DRW },
        { "SUBF", OPM_FR_FRW },
        { "SUBV", OPM_VR_VRW },

        { "DTOF", OPM_R_W|OPM_D_F }, /* Round according to rounding mode */
        { "DTOI", OPM_R_W|OPM_D_I }, /* Truncate + saturate to signed 32-bits */
        { "FTOD", OPM_R_W|OPM_F_D }, /* Exact */
        { "FTOI", OPM_R_W|OPM_F_I }, /* Truncate + saturate to signed 32-bits */
        { "ITOD", OPM_R_W|OPM_I_D }, /* Exact */
        { "ITOF", OPM_R_W|OPM_I_F }, /* Round according to rounding mode */

        { "SINCOSF", OPM_FR_FRW },

        /* Compute the dot product of two vectors - the result is
         * stored in the last element of the target operand (and the
         * other elements are unchanged)
         */
        { "DOTPRODV", OPM_R_RW|OPM_V_V },
        /* Perform the matrix multiplication V * M and place the result
         * in V.
         */
        { "MATMULV", OPM_R_RW|OPM_V_M },
        
        { "LOAD.B", OPM_R_W_EXC },
        { "LOAD.BFW", OPM_R_W_EXC },
        { "LOAD.W", OPM_R_W_EXC },
        { "LOAD.L", OPM_R_W_EXC },
        { "LOAD.Q", OPM_R_W_EXC|OPM_I_Q },
        { "STORE.B", OPM_R_R_EXC },
        { "STORE.W", OPM_R_R_EXC },
        { "STORE.L", OPM_R_R_EXC },
        { "STORE.Q", OPM_R_R_EXC|OPM_I_Q },
        { "STORE.LCA", OPM_R_R_EXC },

        { "BRCOND", OPM_R_R|OPM_TR | OPM_TERM },
        { "BRCONDDEL", OPM_R_R|OPM_TR },
        { "RAISE/ME", OPM_R_R | OPM_EXC },
        { "RAISE/MNE", OPM_R_R | OPM_EXC },
        
        /* Native/pointer operations */
        { "CALL/LUT", OPM_R_R | OPM_EXC },
        { "CALL1", OPM_R_R | OPM_CLB },
        { "CALLR", OPM_R_W | OPM_CLB },
        { "LOADPTRL", OPM_R_W | 0x060 },
        { "LOADPTRQ", OPM_R_W | 0x160 },
        { "XLAT", OPM_R_RW | 0x600 }, /* Xlat Rm, Rn - Read native [Rm+Rn] and store in Rn */ 
        
        { "ADDQSAT32", OPM_R_R | OPM_CLBT|OPM_Q_Q },
        { "ADDQSAT48", OPM_R_R | OPM_CLBT|OPM_Q_Q },
        { "CMP/STR", OPM_R_R_TW | OPM_CLBT },
        { "DIV1", OPM_R_RW_TRW | OPM_CLBT },
        { "SHAD", OPM_R_RW | OPM_CLBT },
        { "SHLD", OPM_R_RW | OPM_CLBT },
               
};

void xir_clear_basic_block( xir_basic_block_t xbb )
{
    xbb->next_temp_reg = 0;
    xir_alloc_temp_reg( xbb, XTY_LONG, -1 );
    xir_alloc_temp_reg( xbb, XTY_LONG, -1 );
    xir_alloc_temp_reg( xbb, XTY_LONG, -1 );
    xir_alloc_temp_reg( xbb, XTY_QUAD, -1 );
    xir_alloc_temp_reg( xbb, XTY_QUAD, -1 );
}

uint32_t xir_alloc_temp_reg( xir_basic_block_t xbb, xir_type_t type, int home )
{
    assert( xbb->next_temp_reg <= MAX_TEMP_REGISTER );
    int reg = xbb->next_temp_reg++;
    xbb->temp_regs[reg].type = type;
    xbb->temp_regs[reg].home_register = home;
    return reg;
}

void xir_set_symbol_table( const struct xir_symbol_entry *symtab )
{
    xir_symbol_table = symtab;
}

const char *xir_lookup_symbol( const void *ptr ) 
{
    if( xir_symbol_table != NULL ) {
        const struct xir_symbol_entry *p;
        for( p = xir_symbol_table; p->name != NULL; p++ ) {
            if( p->ptr == ptr ) {
                return p->name;
            }
        }
    }
    return NULL;
}   

static int xir_snprint_operand( char *buf, int buflen, xir_basic_block_t xbb, xir_operand_t op, xir_type_t ty )
{
    const char *name;
    xir_temp_register_t tmp;
    
    switch( op->form ) {
    case IMMEDIATE_OPERAND:
        switch( ty ) {
        case XTY_LONG:
            return snprintf( buf, buflen, "$0x%x", op->value.i );
        case XTY_QUAD:
            return snprintf( buf, buflen, "$0x%lld", op->value.q );
        case XTY_FLOAT:
            return snprintf( buf, buflen, "%f", op->value.f );
        case XTY_DOUBLE:
            return snprintf( buf, buflen, "%f", op->value.f );
        case XTY_PTR:
            name = xir_lookup_symbol( op->value.p );
            if( name != NULL ) {
                return snprintf( buf, buflen, "*%s", name );
            } else {
                return snprintf( buf, buflen, "*%p", op->value.p );
            }
        default:
            return snprintf( buf, buflen, "ILLOP" );
        }
        break;
    case SOURCE_OPERAND:
        name = xbb->source->get_register_name(op->value.i, ty);
        if( name != NULL ) {
            return snprintf( buf, buflen, "%%%s", name );
        } else {
            return snprintf( buf, buflen, "%%src%d", op->value.i );
        }
        break;
    case TEMP_OPERAND:
        tmp = &xbb->temp_regs[op->value.i];
        if( tmp->home_register != -1 && 
            (name = xbb->source->get_register_name(tmp->home_register,ty)) != NULL ) {
            return snprintf( buf, buflen, "%%%s.%c%d", name,
                    XIR_TYPE_CODES[tmp->type], op->value.i );
        } else {
            return snprintf( buf, buflen, "%%tmp%c%d", 
                    XIR_TYPE_CODES[tmp->type], op->value.i );
        }
    case DEST_OPERAND:
        name = xbb->target->get_register_name(op->value.i, ty);
        if( name != NULL ) {
            return snprintf( buf, buflen, "%%%s", name );
        } else {
            return snprintf( buf, buflen, "%%dst%d", op->value.i );
        }
    default:
        return snprintf( buf, buflen, "ILLOP" );
    }
}

static void xir_print_instruction( FILE *out, xir_basic_block_t xbb, xir_op_t i )
{
    char operands[64] = "";
    
    if( i->operand[0].form != NO_OPERAND ) {
        int pos = xir_snprint_operand( operands, sizeof(operands), xbb, &i->operand[0],
                XOP_OPTYPE(i,0));
        if( i->operand[1].form != NO_OPERAND ) {
            strncat( operands, ", ", sizeof(operands)-pos );
            pos += 2;
            xir_snprint_operand( operands+pos, sizeof(operands)-pos, xbb, &i->operand[1],
                                 XOP_OPTYPE(i,1) );
        }
    }
    if( i->cond == CC_TRUE ) {
        fprintf( out, "%-9s %-30s", XIR_OPCODE_TABLE[i->opcode].name, operands );
    } else {
        char buf[16];
        snprintf( buf, 16, "%s%s", XIR_OPCODE_TABLE[i->opcode].name, XIR_CC_NAMES[i->cond] );
        fprintf( out, "%-9s %-30s", buf, operands );
    }
}

/**
 * Sanity check a block of IR to make sure that
 * operands match up with the expected values etc
 */
void xir_verify_block( xir_basic_block_t xbb, xir_op_t start, xir_op_t end )
{
    xir_op_t it;
    int flags_written = 0;
    for( it = start; it != NULL; it = it->next ) {
        assert( it != NULL && "Unexpected end of block" );
        assert( it->cond >= CC_TRUE && it->cond <= CC_SGT && "Invalid condition code" );
        if( XOP_HAS_0_OPERANDS(it) ) {
            assert( it->operand[0].form == NO_OPERAND && it->operand[1].form == NO_OPERAND );
        } else if( XOP_HAS_1_OPERAND(it) ) {
            assert( it->operand[0].form != NO_OPERAND && it->operand[1].form == NO_OPERAND );
        } else if( XOP_HAS_2_OPERANDS(it) ) {
            assert( it->operand[0].form != NO_OPERAND && it->operand[1].form != NO_OPERAND );
        }
        
        if( it->opcode == OP_ENTER ) {
            assert( it->prev == NULL && "Enter instruction must have no predecessors" );
            assert( it == start && "Enter instruction must occur at the start of the block" );
            assert( it->operand[0].form == IMMEDIATE_OPERAND && "Enter instruction must have an immediate operand" );
        } else if( it->opcode == OP_ST || it->opcode == OP_LD ) {
            assert( it->cond != CC_TRUE && "Opcode not permitted with True condition code" );
        }
        
        if( XOP_WRITES_OP1(it) ) {
            assert( XOP_IS_REG(it,0) &&  "Writable operand 1 requires a register" );
        }
        if( XOP_WRITES_OP2(it) ) {
            assert( XOP_IS_REG(it,1) && "Writable operand 2 requires a register" );
        }

        if( XOP_IS_SRC(it,0) ) {
            assert( XOP_REG(it,0) <= MAX_SOURCE_REGISTER && "Undefined source register" );
        } else if( XOP_IS_TMP(it,0) ) {
            assert( XOP_REG(it,0) < xbb->next_temp_reg && "Undefined temporary register" );
        } else if( XOP_IS_DST(it,0) ) {
            assert( XOP_REG(it,0) <= MAX_DEST_REGISTER && "Undefined target register" );
        }
        if( XOP_IS_SRC(it,1) ) {
            assert( XOP_REG(it,1) <= MAX_SOURCE_REGISTER && "Undefined source register" );
        } else if( XOP_IS_TMP(it,1) ) {
            assert( XOP_REG(it,1) < xbb->next_temp_reg && "Undefined temporary register" );
        } else if( XOP_IS_DST(it,1) ) {
            assert( XOP_REG(it,1) <= MAX_DEST_REGISTER && "Undefined target register" );
        }

        if( XOP_READS_FLAGS(it) ) {
            assert( flags_written && "Flags used without prior definition in block" );
        }
        if( XOP_WRITES_FLAGS(it) ) {
            flags_written = 1;
        }
        
        if( XOP_HAS_EXCEPTION(it) ) {
            assert( it->exc != NULL && "Missing exception block" );
            assert( it->exc->prev == it && "Exception back-link broken" );
            xir_verify_block( xbb, it->exc, NULL ); // Verify exception sub-block
        } else {
            assert( it->exc == NULL && "Unexpected exception block" );
        }
        if( XOP_IS_TERMINATOR(it) ) {
            assert( it->next == NULL && "Unexpected next instruction on terminator" );
        } else {
            assert( it->next != NULL && "Missing terminator instruction at end of block" );
            assert( it->next->prev == it && "Linked-list chain broken" );
        }
        if( it == end )
            break;
    }
}

xir_op_t xir_append_op2( xir_basic_block_t xbb, int op, int arg0form, uint32_t arg0, int arg1form, uint32_t arg1 )
{
    xbb->ir_ptr->opcode = op;
    xbb->ir_ptr->cond = CC_TRUE;
    xbb->ir_ptr->operand[0].form = arg0form;
    xbb->ir_ptr->operand[0].value.i = arg0;
    xbb->ir_ptr->operand[1].form = arg1form;
    xbb->ir_ptr->operand[1].value.i = arg1;
    xbb->ir_ptr->exc = NULL;
    xbb->ir_ptr->next = xbb->ir_ptr+1;
    (xbb->ir_ptr+1)->prev = xbb->ir_ptr;
    return xbb->ir_ptr++;
}

xir_op_t xir_append_op2cc( xir_basic_block_t xbb, int op, xir_cc_t cc, int arg0form, uint32_t arg0, int arg1form, uint32_t arg1 )
{
    xbb->ir_ptr->opcode = op;
    xbb->ir_ptr->cond = cc;
    xbb->ir_ptr->operand[0].form = arg0form;
    xbb->ir_ptr->operand[0].value.i = arg0;
    xbb->ir_ptr->operand[1].form = arg1form;
    xbb->ir_ptr->operand[1].value.i = arg1;
    xbb->ir_ptr->exc = NULL;
    xbb->ir_ptr->next = xbb->ir_ptr+1;
    (xbb->ir_ptr+1)->prev = xbb->ir_ptr;
    return xbb->ir_ptr++;
}

xir_op_t xir_append_float_op2( xir_basic_block_t xbb, int op, float imm1, int arg1form, uint32_t arg1 )
{
    xbb->ir_ptr->opcode = op;
    xbb->ir_ptr->cond = CC_TRUE;
    xbb->ir_ptr->operand[0].form = IMMEDIATE_OPERAND;
    xbb->ir_ptr->operand[0].value.i = imm1;
    xbb->ir_ptr->operand[1].form = arg1form;
    xbb->ir_ptr->operand[1].value.i = arg1;
    xbb->ir_ptr->exc = NULL;
    xbb->ir_ptr->next = xbb->ir_ptr+1;
    (xbb->ir_ptr+1)->prev = xbb->ir_ptr;
    return xbb->ir_ptr++;
}

xir_op_t xir_append_ptr_op2( xir_basic_block_t xbb, int op, void *arg0, int arg1form, uint32_t arg1 )
{
    xbb->ir_ptr->opcode = op;
    xbb->ir_ptr->cond = CC_TRUE;
    xbb->ir_ptr->operand[0].form = IMMEDIATE_OPERAND;
    xbb->ir_ptr->operand[0].value.p = arg0;
    xbb->ir_ptr->operand[1].form = arg1form;
    xbb->ir_ptr->operand[1].value.i = arg1;
    xbb->ir_ptr->exc = NULL;
    xbb->ir_ptr->next = xbb->ir_ptr+1;
    (xbb->ir_ptr+1)->prev = xbb->ir_ptr;
    return xbb->ir_ptr++;
}

void xir_insert_op( xir_op_t op, xir_op_t before )
{
    op->prev = before->prev;
    op->next = before;
    before->prev->next = op;
    before->prev = op;
}

void xir_insert_block( xir_op_t start, xir_op_t end, xir_op_t before )
{
    start->prev = before->prev;
    end->next = before;
    before->prev->next = start;
    before->prev = end;
}

void xir_remove_op( xir_op_t op )
{
    if( op->next != NULL ) {
        op->next->prev = op->prev;
    }
    if( op->prev != NULL ) {
        if( op->prev->next == op ) {
            op->prev->next = op->next;
        } else {
            assert( op->prev->exc == op );
            op->prev->exc = op->next;
        }
    }
}

void xir_print_block( FILE *out, xir_basic_block_t xbb, xir_op_t start, xir_op_t end )
{
    xir_op_t it = start;
    xir_op_t exc = NULL;
    
    while( it != NULL ) {
        if( it->exc ) {
            while( exc ) {
                fprintf( out, "%40c   ", ' ' );
                xir_print_instruction( out, xbb, exc );
                fprintf( out, "\n" );
                exc = exc->next;
            }
            exc = it->exc;
        }
        xir_print_instruction( out, xbb, it );
        if( exc ) {
            if( it->exc ) {
                fprintf( out, "=> " );
            } else {
                fprintf( out, "   " );
            }
            xir_print_instruction( out, xbb, exc );
            exc = exc->next;
        }
        fprintf( out, "\n" );
        if( it == end )
            break;
        it = it->next;
    }
}

void xir_dump_block( xir_basic_block_t xbb )
{
    xir_print_block( stdout, xbb, xbb->ir_begin, xbb->ir_end );
}
