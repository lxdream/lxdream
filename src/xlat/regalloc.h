/**
 * $Id: regalloc.h 931 2008-10-31 02:57:59Z nkeynes $
 * 
 * Register allocation based on simple linear scan
 *
 * Copyright (c) 2008 Nathan Keynes.
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

#ifndef lxdream_regalloc_H
#define lxdream_regalloc_H 1

#include "xlat/xir.h"

typedef struct target_machine_reg {
    
} *target_machine_reg_t;

/**
 * Allocate registers from the source machine to the target machine
 */
void xlat_register_alloc( xir_op_t start, xir_op_t end, target_machine_reg_t target_registers );


#endif /* !lxdream_regalloc_H */
