/**
 * $Id: armmem.c,v 1.6 2005-12-26 10:48:20 nkeynes Exp $
 *
 * Implements the ARM's memory map.
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

#include <stdlib.h>
#include "dream.h"
#include "mem.h"

char *arm_mem = NULL;
char *arm_mem_scratch = NULL;

void arm_mem_init() {
    arm_mem = mem_get_region_by_name( MEM_REGION_AUDIO );
    arm_mem_scratch = mem_get_region_by_name( MEM_REGION_AUDIO_SCRATCH );
}

int arm_has_page( uint32_t addr ) {
    return ( addr < 0x00200000 ||
	     (addr >= 0x00800000 && addr <= 0x00805000 ) );
}

uint32_t arm_read_long( uint32_t addr ) {
    if( addr < 0x00200000 ) {
	return *(int32_t *)(arm_mem + addr);
	/* Main sound ram */
    } else {
	switch( addr & 0xFFFFF000 ) {
	case 0x00800000:
	    return mmio_region_AICA0_read(addr);
	case 0x00801000:
	    return mmio_region_AICA1_read(addr);
	case 0x00802000:
	    return mmio_region_AICA2_read(addr);
	case 0x00803000:
	case 0x00804000:
	    return *(int32_t *)(arm_mem_scratch + addr - 0x00803000);
	}
    }
    ERROR( "Attempted long read to undefined page: %08X",
	   addr );
    /* Undefined memory */
    return 0;
}

uint32_t arm_read_word( uint32_t addr ) {
    return (uint32_t)(uint16_t)arm_read_long( addr );
}

uint32_t arm_read_byte( uint32_t addr ) {
    return (uint32_t)(uint8_t)arm_read_long( addr );
}

void arm_write_long( uint32_t addr, uint32_t value )
{
    if( addr < 0x00200000 ) {
	/* Main sound ram */
	*(uint32_t *)(arm_mem + addr) = value;
    } else {
	switch( addr & 0xFFFFF000 ) {
	case 0x00800000:
	    mmio_region_AICA0_write(addr, value);
	    break;
	case 0x00801000:
	    mmio_region_AICA1_write(addr, value);
	    break;
	case 0x00802000:
	    mmio_region_AICA2_write(addr, value);
	    break;
	case 0x00803000:
	case 0x00804000:
	    *(uint32_t *)(arm_mem_scratch + addr - 0x00803000) = value;
	    break;
	default:
	    ERROR( "Attempted long write to undefined address: %08X",
		   addr );
	    /* Undefined memory */
	} 
    }
    return 0;
}

void arm_write_byte( uint32_t addr, uint32_t value )
{
    if( addr < 0x00200000 ) {
	/* Main sound ram */
	*(uint8_t *)(arm_mem + addr) = (uint8_t)value;
    } else {
	switch( addr & 0xFFFFF000 ) {
	case 0x00800000:
	    mmio_region_AICA0_write(addr, value);
	    break;
	case 0x00801000:
	    mmio_region_AICA1_write(addr, value);
	    break;
	case 0x00802000:
	    mmio_region_AICA2_write(addr, value);
	    break;
	case 0x00803000:
	case 0x00804000:
	    *(uint8_t *)(arm_mem_scratch + addr - 0x00803000) = (uint8_t)value;
	    break;
	default:
	    ERROR( "Attempted byte write to undefined address: %08X",
		   addr );
	    /* Undefined memory */
	} 
    }
    return 0;
}

/* User translations - TODO */

uint32_t arm_read_long_user( uint32_t addr ) {
    return arm_read_long( addr );
}

uint32_t arm_read_byte_user( uint32_t addr ) {
    return arm_read_byte( addr );
}

void arm_write_long_user( uint32_t addr, uint32_t val ) {
    arm_write_long( addr, val );
}

void arm_write_byte_user( uint32_t addr, uint32_t val )
{
    arm_write_byte( addr, val );
}
