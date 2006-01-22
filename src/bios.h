/**
 * $Id: bios.h,v 1.1 2006-01-22 22:40:53 nkeynes Exp $
 * 
 * "Fake" BIOS support, to allow basic functionality without the BIOS
 * actually being present.
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
#ifndef dream_bios_H
#define dream_bios_H 1

#include <stdint.h>
#include <glib/gtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Execute a BIOS syscall identified by a syscall ID (currently the last
 * byte of the vector).
 */
void bios_syscall( uint32_t syscallid );

/**
 * Install the BIOS emu hack into ram (sets the vectors at 8C0000B0 through 
 * 8C0000C0)
 */
void bios_install( void );


#ifdef __cplusplus
}
#endif
#endif
