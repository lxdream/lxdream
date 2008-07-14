/**
 * $Id$
 * 
 * SH4 CPU definition and disassembly function declarations
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

#ifndef lxdream_sh4dasm_H
#define lxdream_sh4dasm_H 1

#include "cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

uint32_t sh4_disasm_instruction( uint32_t pc, char *buf, int len, char * );
void sh4_disasm_region( FILE *f, int from, int to );

extern const struct cpu_desc_struct sh4_cpu_desc;

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_sh4dasm_H */
