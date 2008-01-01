/**
 * $Id$
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
#define MAX_INSTRUCTION_SIZE 256
/** Maximum size of the translation epilogue (current real size is 116 bytes, so
 * allows a little room
 */
#define EPILOGUE_SIZE 128
/**

 */
uint32_t sh4_xlat_run_slice( uint32_t nanosecs );

/**
 * Translate the specified block of code starting from the specified start
 * address until the first branch/jump instruction.
 */
void *sh4_translate_basic_block( sh4addr_t start );

extern uint8_t *xlat_output;

/******************************************************************************
 * Code generation - these methods must be provided by the
 * actual code gen (eg sh4x86.c) 
 ******************************************************************************/

#define TARGET_X86 1
#define TARGET_X86_64 2

void sh4_translate_begin_block( sh4addr_t pc );
uint32_t sh4_translate_instruction( sh4addr_t pc );
void sh4_translate_end_block( sh4addr_t pc );
