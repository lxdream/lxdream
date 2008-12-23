/**
 * $Id$
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
#include "aica.h"
#include "armcore.h"

unsigned char *arm_mem = NULL;
unsigned char *arm_mem_scratch = NULL;

void arm_mem_init() {
    arm_mem = mem_get_region_by_name( MEM_REGION_AUDIO );
    arm_mem_scratch = mem_get_region_by_name( MEM_REGION_AUDIO_SCRATCH );
}

/*************** ARM memory access function blocks **************/

static int32_t FASTCALL ext_audioram_read_long( sh4addr_t addr )
{
    return *((int32_t *)(arm_mem + (addr&0x001FFFFF)));
}
static int32_t FASTCALL ext_audioram_read_word( sh4addr_t addr )
{
    return SIGNEXT16(*((int16_t *)(arm_mem + (addr&0x001FFFFF))));
}
static int32_t FASTCALL ext_audioram_read_byte( sh4addr_t addr )
{
    return SIGNEXT8(*((int16_t *)(arm_mem + (addr&0x001FFFFF))));
}
static void FASTCALL ext_audioram_write_long( sh4addr_t addr, uint32_t val )
{
    *(uint32_t *)(arm_mem + (addr&0x001FFFFF)) = val;
    asic_g2_write_word();
}
static void FASTCALL ext_audioram_write_word( sh4addr_t addr, uint32_t val )
{
    *(uint16_t *)(arm_mem + (addr&0x001FFFFF)) = (uint16_t)val;
    asic_g2_write_word();
}
static void FASTCALL ext_audioram_write_byte( sh4addr_t addr, uint32_t val )
{
    *(uint8_t *)(arm_mem + (addr&0x001FFFFF)) = (uint8_t)val;
    asic_g2_write_word();
}
static void FASTCALL ext_audioram_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memcpy( dest, arm_mem+(addr&0x001FFFFF), 32 );
}
static void FASTCALL ext_audioram_write_burst( sh4addr_t addr, unsigned char *src )
{
    memcpy( arm_mem+(addr&0x001FFFFF), src, 32 );
}

struct mem_region_fn mem_region_audioram = { ext_audioram_read_long, ext_audioram_write_long, 
        ext_audioram_read_word, ext_audioram_write_word, 
        ext_audioram_read_byte, ext_audioram_write_byte, 
        ext_audioram_read_burst, ext_audioram_write_burst }; 


static int32_t FASTCALL ext_audioscratch_read_long( sh4addr_t addr )
{
    return *((int32_t *)(arm_mem_scratch + (addr&0x00001FFF)));
}
static int32_t FASTCALL ext_audioscratch_read_word( sh4addr_t addr )
{
    return SIGNEXT16(*((int16_t *)(arm_mem_scratch + (addr&0x00001FFF))));
}
static int32_t FASTCALL ext_audioscratch_read_byte( sh4addr_t addr )
{
    return SIGNEXT8(*((int16_t *)(arm_mem_scratch + (addr&0x00001FFF))));
}
static void FASTCALL ext_audioscratch_write_long( sh4addr_t addr, uint32_t val )
{
    *(uint32_t *)(arm_mem_scratch + (addr&0x00001FFF)) = val;
    asic_g2_write_word();
}
static void FASTCALL ext_audioscratch_write_word( sh4addr_t addr, uint32_t val )
{
    *(uint16_t *)(arm_mem_scratch + (addr&0x00001FFF)) = (uint16_t)val;
    asic_g2_write_word();
}
static void FASTCALL ext_audioscratch_write_byte( sh4addr_t addr, uint32_t val )
{
    *(uint8_t *)(arm_mem_scratch + (addr&0x00001FFF)) = (uint8_t)val;
    asic_g2_write_word();
}
static void FASTCALL ext_audioscratch_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memcpy( dest, arm_mem_scratch+(addr&0x00001FFF), 32 );
}
static void FASTCALL ext_audioscratch_write_burst( sh4addr_t addr, unsigned char *src )
{
    memcpy( arm_mem_scratch+(addr&0x00001FFF), src, 32 );
}

struct mem_region_fn mem_region_audioscratch = { ext_audioscratch_read_long, ext_audioscratch_write_long, 
        ext_audioscratch_read_word, ext_audioscratch_write_word, 
        ext_audioscratch_read_byte, ext_audioscratch_write_byte, 
        ext_audioscratch_read_burst, ext_audioscratch_write_burst }; 

/************************** Local ARM support **************************/
int arm_has_page( uint32_t addr ) {
    return ( addr < 0x00200000 ||
            (addr >= 0x00800000 && addr <= 0x00805000 ) );
}

uint32_t arm_read_long( uint32_t addr ) {
    if( addr < 0x00200000 ) {
        return *(int32_t *)(arm_mem + addr);
        /* Main sound ram */
    } else {
        uint32_t val;
        switch( addr & 0xFFFFF000 ) {
        case 0x00800000:
            val = mmio_region_AICA0_read(addr&0x0FFF);
            //	    DEBUG( "ARM long read from %08X => %08X", addr, val );
            return val;
        case 0x00801000:
            val = mmio_region_AICA1_read(addr&0x0FFF);
            //	    DEBUG( "ARM long read from %08X => %08X", addr, val );
            return val;
        case 0x00802000:
            val = mmio_region_AICA2_read(addr&0x0FFF);
            // DEBUG( "ARM long read from %08X => %08X", addr, val );
            return val;
        case 0x00803000:
        case 0x00804000:
            return *(int32_t *)(arm_mem_scratch + addr - 0x00803000);
        }
    }
    ERROR( "Attempted long read to undefined page: %08X at %08X",
           addr, armr.r[15] );
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
            // DEBUG( "ARM long write to %08X <= %08X", addr, value );
            mmio_region_AICA0_write(addr&0x0FFF, value);
            break;
        case 0x00801000:
            // DEBUG( "ARM long write to %08X <= %08X", addr, value );
            mmio_region_AICA1_write(addr&0x0FFF, value);
            break;
        case 0x00802000:
            // DEBUG( "ARM long write to %08X <= %08X", addr, value );
            mmio_region_AICA2_write(addr&0x0FFF, value);
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
    return;
}

uint32_t arm_combine_byte( uint32_t addr, uint32_t val, uint8_t byte )
{
    switch( addr & 0x03 ) {
    case 0:
        return (val & 0xFFFFFF00) | byte;
    case 1:
        return (val & 0xFFFF00FF) | (byte<<8);
    case 2:
        return (val & 0xFF00FFFF) | (byte<<16);
    case 3:
        return (val & 0x00FFFFFF) | (byte<<24);
    default:
        return val; // Can't happen, but make gcc happy
    }
}
void arm_write_word( uint32_t addr, uint32_t value )
{
	if( addr < 0x00200000 ) {
        *(uint16_t *)(arm_mem + addr) = (uint16_t)value;
	} else {
		
	}
}
void arm_write_byte( uint32_t addr, uint32_t value )
{
    if( addr < 0x00200000 ) {
        /* Main sound ram */
        *(uint8_t *)(arm_mem + addr) = (uint8_t)value;
    } else {
        uint32_t tmp;
        switch( addr & 0xFFFFF000 ) {
        case 0x00800000:
            tmp = MMIO_READ( AICA0, addr & 0x0FFC );
            value = arm_combine_byte( addr, tmp, value );
            mmio_region_AICA0_write(addr&0x0FFC, value);
            break;
        case 0x00801000:
            tmp = MMIO_READ( AICA1, addr & 0x0FFC );
            value = arm_combine_byte( addr, tmp, value );
            mmio_region_AICA1_write(addr&0x0FFC, value);
            break;
        case 0x00802000:
            tmp = MMIO_READ( AICA2, addr & 0x0FFC );
            value = arm_combine_byte( addr, tmp, value );
            mmio_region_AICA2_write(addr&0x0FFC, value);
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
    return;
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
