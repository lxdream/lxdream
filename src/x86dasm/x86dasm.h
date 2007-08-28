/**
 * $Id: x86dasm.h,v 1.1 2007-08-28 08:46:54 nkeynes Exp $
 *
 * Wrapper around i386-dis to supply the same behaviour as the other
 * disassembly functions.
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

#include "cpu.h"
#include "mem.h"
extern const struct cpu_desc_struct x86_cpu_desc;

uint32_t x86_disasm_instruction( uint32_t pc, char *buf, int len, char *opcode );
