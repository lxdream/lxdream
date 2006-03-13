/**
 * $Id: syscall.h,v 1.1 2006-03-13 12:38:34 nkeynes Exp $
 * 
 * Generic syscall support - ability to add hooks into SH4 code to call out
 * to the emu.
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
#ifndef dream_syscall_H
#define dream_syscall_H 1

#include <stdint.h>
#include <glib/gtypes.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef void (*syscall_hook_func_t)( uint32_t hook_id );


/**
 * Define a new hook without an indirect vector
 */
void syscall_add_hook( uint32_t hook_id, syscall_hook_func_t hook );

/**
 * Define a new hook which indirects through the specified vector address
 * (which must be somewhere in main SH4 ram).
 */
void syscall_add_hook_vector( uint32_t hook_id, uint32_t vector_addr,
			      syscall_hook_func_t hook );

/**
 * Invoke a syscall from the SH4
 */
void syscall_invoke( uint32_t hook_id );

/**
 * Repatch all syscall vectors (eg in case of system reset)
 */
void syscall_repatch_vectors( );

/************************ Standard syscall hacks ************************/

/**
 * Install the BIOS emu hack into ram (sets the vectors at 8C0000B0 through 
 * 8C0000C0)
 */
void bios_install( void );

/**
 * Install the DCLoad syscall hack
 */
void dcload_install( void );

#ifdef __cplusplus
}
#endif
#endif
