/**
 * $Id$
 *
 * Generic CPU utility functions
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

#include "cpu.h"

void cpu_print_registers( FILE *f, cpu_desc_t cpu )
{
    int i;
    int column = 0;

    fprintf( f, "%s registers:\n", cpu->name );
    for( i=0; cpu->regs_info[i].name != NULL; i++ ) {
        void *value = cpu->get_register(i);
        if( value != NULL ) {
            column++;
            switch( cpu->regs_info[i].type ) {
            case REG_TYPE_INT:
                fprintf( f,  "%5s: %08x   ", cpu->regs_info[i].name, *(uint32_t *)value );
                break;
            case REG_TYPE_FLOAT:
                fprintf( f, "%5s: %.8f ", cpu->regs_info[i].name, (double)*(float *)value );
                break;
            case REG_TYPE_DOUBLE:
                fprintf( f, "%5s: %.8f ", cpu->regs_info[i].name, *(double *)value );
                break;
            case REG_TYPE_NONE:
                column = 4;
            }
        }
        if( column == 4 ) {
            fprintf( f, "\n" );
            column = 0;
        }
    }
}
