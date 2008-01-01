/**
 * $Id$
 * 
 * Routines to add hook functions that are callable from the SH4
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
#include "syscall.h"
#include "sh4/sh4core.h"


struct syscall_hook {
    syscall_hook_func_t hook;
    sh4addr_t vector;
} syscall_hooks[256];

void syscall_add_hook( uint32_t hook_id, syscall_hook_func_t hook ) 
{
    hook_id &= 0xFF;
    if( syscall_hooks[hook_id].hook != NULL )
	WARN( "Overwriting existing hook %02X", hook_id );
    syscall_hooks[hook_id].hook = hook;
    syscall_hooks[hook_id].vector = 0;
}

void syscall_add_hook_vector( uint32_t hook_id, uint32_t vector_addr,
			      syscall_hook_func_t hook )
{
    hook_id &= 0xFF;
    syscall_add_hook( hook_id, hook );
    syscall_hooks[hook_id].vector = vector_addr;
    sh4_write_long( vector_addr, 0xFFFFFF00 + hook_id );
}

void syscall_invoke( uint32_t hook_id )
{
    hook_id &= 0xFF;
    syscall_hook_func_t hook = syscall_hooks[hook_id].hook;
    if( hook == NULL ) {
	WARN( "Invoked non-existent hook %02X", hook_id );
    } else {
	hook(hook_id);
    }
}

void syscall_repatch_vectors( )
{
    int i;
    for( i=0; i<256; i++ ) {
	if( syscall_hooks[i].hook != NULL &&
	    syscall_hooks[i].vector != 0 ) {
	    sh4_write_long( syscall_hooks[i].vector, 0xFFFFFF00 + i );
	}
    }
}
