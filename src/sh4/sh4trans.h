/**
 * $Id: sh4trans.h,v 1.1 2007-08-23 12:33:27 nkeynes Exp $
 * 
 * SH4->x86 translation module
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

#include "dream.h"
#include "mem.h"

/** Maximum size of a translated instruction, in bytes. This includes potentially
 * writing the entire epilogue
 */
#define MAX_INSTRUCTION_SIZE 32

/**

 */
uint32_t sh4_xlat_run_slice( uint32_t nanosecs );

/**
 * Translate the specified block of code starting from the specified start
 * address until the first branch/jump instruction.
 */
void *sh4_translate_basic_block( sh4addr_t start );

extern uint8_t *xlat_output;

/************** Output generation ***************/

void sh4_translate_begin_block();

uint32_t sh4_x86_translate_instruction( sh4addr_t pc );

void sh4_translate_end_block( sh4addr_t pc );
