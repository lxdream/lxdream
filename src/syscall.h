/**
 * $Id$
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
#ifndef lxdream_syscall_H
#define lxdream_syscall_H 1

#include <stdint.h>
#include <glib.h>

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

void bios_boot( uint32_t syscallid );

/**
 * Install the DCLoad syscall hack
 */
void dcload_install( void );

/**
 * Set the flag that indicates whether the dcload exit() syscall will be
 * honoured by exiting the VM.
 */
void dcload_set_allow_unsafe( gboolean allow );

#define IS_SYSCALL(pc)  (((uint32_t)pc)>=0xFFFFFF00)

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_syscall_H */
