/**
 * $Id: x86op.h 973 2009-01-13 11:56:28Z nkeynes $
 * 
 * Declarations for the SH4 -> IR decoder.
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

#ifndef lxdream_sh4xir_H
#define lxdream_sh4xir_H 1

#include "xlat/xir.h"
#include "xlat/xlat.h"

/**
 * SH4 source description
 */
extern struct xlat_source_machine sh4_source_machine_desc;

/**
 * Mapping from register number to names
 */
extern struct xlat_source_machine sh4_source_machine_desc;

#endif /* !lxdream_sh4xir_H */
