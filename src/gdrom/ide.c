/**
 * $Id: ide.c,v 1.17 2006-12-15 10:18:39 nkeynes Exp $
 *
 * IDE interface implementation
 *
 *
 * Note: All references to read/write are from the host's point of view.
 *
 * See: INF-8020 
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

#define MODULE ide_module

#include <assert.h>
#include <stdlib.h>
#include "dream.h"
#include "asic.h"
#include "gdrom/ide.h"
#include "gdrom/gdrom.h"
#include "gdrom/packet.h"

#define MAX_WRITE_BUF 4096
#define MAX_SECTOR_SIZE 2352 /* Audio sector */
#define DEFAULT_DATA_SECTORS 8

static void ide_init( void );
static void ide_reset( void );
static void ide_save_state( FILE *f );
static int ide_load_state( FILE *f );
static void ide_raise_interrupt( void );
static void ide_clear_interrupt( void );
static void ide_packet_command( unsigned char *data );

struct dreamcast_module ide_module = { "IDE", ide_init, ide_reset, NULL, NULL,
				       NULL, ide_save_state, ide_load_state };

struct ide_registers idereg;
unsigned char *data_buffer = NULL;
uint32_t data_buffer_len = 0;

#define WRITE_BUFFER(x16) *((uint16_t *)(data_buffer + idereg.data_offset)) = x16
#define READ_BUFFER() *((uint16_t *)(data_buffer + idereg.data_offset))

/* "\0\0\0\0\xb4\x19\0\0\x08SE      REV 6.42990316" */
unsigned char gdrom_ident[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0xb4, 0x19, 0x00,
                       0x00, 0x08, 0x53, 0x45, 0x20, 0x20, 0x20, 0x20,
                       0x20, 0x20, 0x52, 0x65, 0x76, 0x20, 0x36, 0x2e,
                       0x34, 0x32, 0x39, 0x39, 0x30, 0x33, 0x31, 0x36 };

unsigned char gdrom_71[] = { 0x3E, 0x0F, 0x90, 0xBE,  0x1D, 0xD9, 0x89, 0x04,  0x28, 0x3A, 0x8E, 0x26,  0x5C, 0x95, 0x10, 0x5A,
				 0x0A, 0x99, 0xEE, 0xFB,  0x69, 0xCE, 0xD9, 0x63,  0x00, 0xF5, 0x0A, 0xBC,  0x2C, 0x0D, 0xF8, 0xE2,
				 0x05, 0x02, 0x00, 0x7C,  0x03, 0x00, 0x3D, 0x08,  0xD8, 0x8D, 0x08, 0x7A,  0x6D, 0x00, 0x35, 0x06,
				 0xBA, 0x66, 0x10, 0x00,  0x91, 0x08, 0x10, 0x29,  0xD0, 0x45, 0xDA, 0x00,  0x2D, 0x05, 0x69, 0x09,
				 0x00, 0x5E, 0x0F, 0x70,  0x86, 0x12, 0x6C, 0x77,  0x5A, 0xFB, 0xCD, 0x56,  0xFB, 0xF7, 0xB7, 0x00,
				 0x5D, 0x07, 0x19, 0x99,  0xF2, 0xAF, 0x00, 0x63,  0x03, 0x00, 0xF0, 0x10,  0xBE, 0xD7, 0xA0, 0x63,
				 0xFA, 0x84, 0xA7, 0x74,  0x94, 0xEF, 0xAD, 0xC2,  0xAC, 0x00, 0x78, 0x07,  0x9F, 0x57, 0x0B, 0x62,
				 0x00, 0xFE, 0x08, 0x08,  0x5D, 0x5A, 0x6A, 0x54,  0x00, 0xE2, 0x09, 0x93,  0x7E, 0x62, 0x2A, 0x5E,
				 0xDA, 0x00, 0x7E, 0x0F,  0xF0, 0x07, 0x01, 0x6D,  0x50, 0x86, 0xDD, 0x4A,  0x15, 0x54, 0xC7, 0xEC,
				 0x00, 0xF2, 0x0B, 0x07,  0xF8, 0x1A, 0xB0, 0x99,  0x3B, 0xF1, 0x36, 0x00,  0x94, 0x07, 0x34, 0xE3,
				 0xBC, 0x6E, 0x00, 0x34,  0x0D, 0x6F, 0xDA, 0xBD,  0xEE, 0xF7, 0xCC, 0xCE,  0x39, 0x7E, 0xE3, 0x00,
				 0x14, 0x08, 0xDC, 0xD2,  0xB9, 0xF9, 0x31, 0x00,  0xB0, 0x0C, 0x10, 0xA3,  0x45, 0x12, 0xC7, 0xCD,
				 0xBF, 0x05, 0x37, 0x00,  0xC4, 0x0D, 0x5F, 0xE0,  0x59, 0xBB, 0x01, 0x59,  0x03, 0xD6, 0x29, 0x9C,
				 0x00, 0x01, 0x0A, 0x09,  0xAA, 0xA8, 0xA8, 0x24,  0x0B, 0x66, 0x00, 0x5C,  0x05, 0xA5, 0xCE, 0x00,
				 0xC1, 0x0B, 0xB7, 0xA0,  0x6F, 0xE9, 0x2B, 0xCC,  0xB5, 0xFC, 0x00, 0x8D,  0x05, 0xF4, 0xAC, 0x00,
				 0x57, 0x04, 0xB6, 0x00,  0xFC, 0x03, 0x00, 0xC3,  0x10, 0x43, 0x3B, 0xBE,  0xA2, 0x96, 0xC3, 0x65,
				 0x9F, 0x9A, 0x88, 0xD5,  0x49, 0x68, 0x00, 0xDC,  0x11, 0x56, 0x23, 0x2D,  0xF9, 0xFC, 0xF5, 0x8B,
				 0x1B, 0xB1, 0xB7, 0x10,  0x21, 0x1C, 0x12, 0x00,  0x0D, 0x0D, 0xEB, 0x86,  0xA2, 0x49, 0x8D, 0x8D,
				 0xBE, 0xA1, 0x6D, 0x53,  0x00, 0xE1, 0x0A, 0x8E,  0x67, 0xAA, 0x16, 0x79,  0x39, 0x59, 0x00, 0x36,
				 0x0B, 0x2A, 0x4E, 0xAE,  0x51, 0x4B, 0xD0, 0x66,  0x33, 0x00, 0x8A, 0x07,  0xCD, 0x6F, 0xBA, 0x92,
				 0x00, 0x1A, 0x0E, 0xDF,  0x4A, 0xB3, 0x77, 0x1F,  0xA5, 0x90, 0x19, 0xFA,  0x59, 0xD7, 0x00, 0x04,
				 0x0F, 0xAC, 0xCA, 0x9F,  0xA4, 0xFC, 0x6D, 0x90,  0x86, 0x9E, 0x1F, 0x44,  0x40, 0x00, 0x9F, 0x04,
				 0x56, 0x00, 0x22, 0x03,  0x00, 0xB8, 0x10, 0x2C,  0x7A, 0x53, 0xA4, 0xBF,  0xA3, 0x90, 0x90, 0x14,
				 0x9D, 0x46, 0x6C, 0x96,  0x00, 0xC6, 0x0B, 0x9B,  0xBB, 0xB0, 0xAE, 0x60,  0x92, 0x8E, 0x0C, 0x00,
				 0x14, 0x06, 0x4B, 0xAE,  0x7F, 0x00, 0x5C, 0x0B,  0x23, 0xFA, 0xE7, 0x51,  0xDA, 0x61, 0x49, 0x5E,
				 0x00, 0xD7, 0x0B, 0x01,  0xFC, 0x55, 0x31, 0x84,  0xC5, 0x0C, 0x98, 0x00,  0x97, 0x50, 0x6E, 0xF9,
				 0xEE, 0x75, 0x92, 0x53,  0xD3, 0x66, 0xA4, 0xAF,  0x3B, 0xFE, 0x7B, 0x27,  0x30, 0xBB, 0xB6, 0xF2,
				 0x76, 0x22, 0x45, 0x42,  0xCA, 0xF9, 0xF0, 0xDE,  0x9F, 0x45, 0x16, 0x68,  0x22, 0xB9, 0x84, 0x28,
				 0x8F, 0x2B, 0xB5, 0x5C,  0xD2, 0xF5, 0x45, 0x36,  0x3E, 0x76, 0xC6, 0xBF,  0x32, 0x5C, 0x41, 0xA6,
				 0x26, 0xC7, 0x82, 0x2F,  0x2E, 0xB5, 0x75, 0xC6,  0xE6, 0x67, 0x9E, 0x77,  0x94, 0xAF, 0x6A, 0x05,
				 0xC0, 0x05, 0x61, 0x71,  0x89, 0x5A, 0xB1, 0xD0,  0xFC, 0x7E, 0xC0, 0x9B,  0xCB, 0x3B, 0x69, 0xD9,
				 0x5F, 0xAF, 0xCA, 0xAB,  0x25, 0xD5, 0xBE, 0x8A,  0x6B, 0xB0, 0xFB, 0x61,  0x6C, 0xEB, 0x85, 0x6E,
				 0x7A, 0x48, 0xFF, 0x97,  0x91, 0x06, 0x3D, 0x4D,  0x68, 0xD3, 0x65, 0x83,  0x90, 0xA0, 0x08, 0x5C,
				 0xFC, 0xEE, 0x7C, 0x33,  0x43, 0x7F, 0x80, 0x52,  0x8B, 0x19, 0x72, 0xF2,  0xC9, 0xAB, 0x93, 0xAF,
				 0x16, 0xED, 0x36, 0x48,  0xAB, 0xC9, 0xD1, 0x03,  0xB3, 0xDC, 0x2F, 0xF2,  0x92, 0x3F, 0x0A, 0x19,
				 0x25, 0xE2, 0xEF, 0x7A,  0x22, 0xDA, 0xDB, 0xCB,  0x32, 0x12, 0x61, 0x49,  0x5B, 0x74, 0x7C, 0x65,
				 0x20, 0x89, 0x54, 0x9E,  0x0E, 0xC9, 0x52, 0xE3,  0xC9, 0x9A, 0x44, 0xC9,  0x5D, 0xA6, 0x77, 0xC0,
				 0xE7, 0x60, 0x91, 0x80,  0x50, 0x1F, 0x33, 0xB1,  0xCD, 0xAD, 0xF4, 0x0D,  0xBB, 0x08, 0xB1, 0xD0,
				 0x13, 0x95, 0xAE, 0xC9,  0xE2, 0x64, 0xA2, 0x65,  0xFB, 0x8F, 0xE9, 0xA2,  0x8A, 0xBC, 0x98, 0x81,
				 0x45, 0xB4, 0x55, 0x4E,  0xB9, 0x74, 0xB4, 0x50,  0x76, 0xBF, 0xF0, 0x45,  0xE7, 0xEE, 0x41, 0x64,
				 0x9F, 0xB5, 0xE0, 0xBB,  0x1C, 0xBB, 0x28, 0x66,  0x1B, 0xDD, 0x2B, 0x02,  0x66, 0xBF, 0xFD, 0x7D,
				 0x37, 0x35, 0x1D, 0x76,  0x21, 0xC3, 0x8F, 0xAF,  0xF6, 0xF9, 0xE9, 0x27,  0x48, 0xE7, 0x3D, 0x95,
				 0x74, 0x0C, 0x77, 0x88,  0x56, 0xD9, 0x84, 0xC8,  0x7D, 0x20, 0x31, 0x43,  0x53, 0xF1, 0xC1, 0xC7,
				 0xC9, 0xF7, 0x5C, 0xC0,  0xA6, 0x5A, 0x27, 0x0A,  0x41, 0xD4, 0x44, 0x94,  0x65, 0x4F, 0xE2, 0x53,
				 0x60, 0x0B, 0xD1, 0x23,  0x6C, 0x0C, 0xBC, 0x70,  0x6C, 0x26, 0x1A, 0x61,  0x1D, 0x35, 0x88, 0xEC,
				 0xB8, 0x15, 0xE3, 0xB4,  0x82, 0xEE, 0xB3, 0x21,  0xAC, 0x6C, 0xB7, 0x33,  0x6D, 0x78, 0x0C, 0x0D,
				 0xB4, 0x0B, 0x29, 0xF2,  0xD4, 0x8C, 0x3F, 0xDD,  0x3F, 0x47, 0xDD, 0xF2,  0xD8, 0x39, 0x57, 0x20,
				 0x28, 0xD8, 0xDD, 0x32,  0xE2, 0x6A, 0x47, 0x53,  0x57, 0xC6, 0xFA, 0x7A,  0x38, 0x30, 0x31, 0x8F,
				 0xE7, 0xD3, 0x84, 0x2B,  0x5D, 0x4F, 0x95, 0x98,  0xED, 0x0B, 0xD7, 0x50,  0x0C, 0x49, 0xDA, 0x59,
				 0x15, 0xF1, 0x39, 0xF3,  0x40, 0xDC, 0xDC, 0x25,  0x24, 0x56, 0x6E, 0xA9,  0x2F, 0xF0, 0x00, 0x00 };


static void ide_init( void )
{
    ide_reset();
    data_buffer_len = DEFAULT_DATA_SECTORS; 
    data_buffer = malloc( MAX_SECTOR_SIZE * data_buffer_len ); 
    assert( data_buffer != NULL );
}

static void ide_reset( void )
{
    ide_clear_interrupt();
    idereg.error = 0x01;
    idereg.count = 0x01;
    idereg.lba0 = /* 0x21; */ 0x81;
    idereg.lba1 = 0x14;
    idereg.lba2 = 0xeb;
    idereg.feature = 0; /* Indeterminate really */
    idereg.status = 0x50;
    idereg.device = 0x00;
    idereg.disc = gdrom_is_mounted() ? (IDE_DISC_CDROM|IDE_DISC_READY) : IDE_DISC_NONE;
    idereg.state = IDE_STATE_IDLE;
    memset( idereg.gdrom_sense, '\0', 10 );
    idereg.data_offset = -1;
    idereg.data_length = -1;
}

static void ide_save_state( FILE *f )
{
    fwrite( &idereg, sizeof(idereg), 1, f );
    fwrite( &data_buffer_len, sizeof(data_buffer_len), 1, f );
    fwrite( data_buffer, MAX_SECTOR_SIZE, data_buffer_len, f );
}

static int ide_load_state( FILE *f )
{
    uint32_t length;
    fread( &idereg, sizeof(idereg), 1, f );
    fread( &length, sizeof(uint32_t), 1, f );
    if( length > data_buffer_len ) {
	if( data_buffer != NULL )
	    free( data_buffer );
	data_buffer = malloc( MAX_SECTOR_SIZE * length  );
	assert( data_buffer != NULL );
	data_buffer_len = length;
    }
    fread( data_buffer, MAX_SECTOR_SIZE, length, f );
    return 0;
}

/************************ State transitions *************************/

void ide_set_packet_result( uint16_t result )
{
    idereg.gdrom_sense[0] = 0xf0;
    idereg.gdrom_sense[2] = result & 0xFF;
    idereg.gdrom_sense[8] = (result >> 8) & 0xFF;
    idereg.error = (result & 0x0F) << 4;
    if( result != 0 ) {
	idereg.status = 0x51;
	ide_raise_interrupt();
    } else {
	idereg.status = idereg.status & ~(IDE_STATUS_BSY|IDE_STATUS_CHK);
    }
}

/**
 * Begin command packet write to the device. This is always 12 bytes of PIO data
 */
static void ide_start_command_packet_write( )
{
    idereg.state = IDE_STATE_CMD_WRITE;
    idereg.status = IDE_STATUS_DRDY | IDE_STATUS_DRQ;
    idereg.error = idereg.feature & 0x03; /* Copy values of OVL/DMA */
    idereg.count = IDE_COUNT_CD;
    idereg.data_offset = 0;
    idereg.data_length = 12;
}

/**
 * Begin PIO read from the device. The data is assumed to already be
 * in the buffer at this point.
 */
static void ide_start_read( int length, int blocksize, gboolean dma ) 
{
    idereg.count = IDE_COUNT_IO;
    idereg.data_length = length;
    idereg.data_offset = 0;
    if( dma ) {
	idereg.state = IDE_STATE_DMA_READ;
	idereg.status = 0xD0;
    } else {
	idereg.state = IDE_STATE_PIO_READ;
	idereg.status = IDE_STATUS_DRDY | IDE_STATUS_DRQ;
	idereg.lba1 = length & 0xFF;
	idereg.lba2 = (length >> 8) & 0xFF;
	//	idereg.lba1 = blocksize & 0xFF;
	//	idereg.lba2 = blocksize >> 8; 
	idereg.block_length = blocksize;
	idereg.block_left = blocksize;
	ide_raise_interrupt( );
    }
}

static void ide_start_packet_read( int length, int blocksize )
{
    ide_set_packet_result( PKT_ERR_OK );
    ide_start_read( length, blocksize, (idereg.feature & IDE_FEAT_DMA) ? TRUE : FALSE );
}

static void ide_raise_interrupt( void )
{
    if( idereg.intrq_pending == 0 ) {
	idereg.intrq_pending = 1;
	if( IS_IDE_IRQ_ENABLED() )
	    asic_event( EVENT_IDE );
    }
}

static void ide_clear_interrupt( void ) 
{
    if( idereg.intrq_pending != 0 ) {
	idereg.intrq_pending = 0;
	if( IS_IDE_IRQ_ENABLED() )
	    asic_clear_event( EVENT_IDE );
    }
}

static void ide_set_error( int error_code )
{
    idereg.status = 0x51;
    idereg.error = error_code;
}

uint8_t ide_read_status( void ) 
{
    if( (idereg.status & IDE_STATUS_BSY) == 0 )
	ide_clear_interrupt();
    return idereg.status;
}

uint16_t ide_read_data_pio( void ) {
    if( idereg.state == IDE_STATE_PIO_READ ) {
	uint16_t rv = READ_BUFFER();
	idereg.data_offset += 2;
	idereg.block_left -= 2;
	if( idereg.data_offset >= idereg.data_length ) {
	    idereg.state = IDE_STATE_IDLE;
	    idereg.status &= ~IDE_STATUS_DRQ;
	    idereg.data_offset = -1;
	    ide_raise_interrupt();
	} else if( idereg.block_left <= 0 ) {
	    idereg.block_left = idereg.block_length;
	    ide_raise_interrupt();
	}
	return rv;
    } else {
        return 0xFFFF;
    }
}


/**
 * DMA read request
 *
 * This method is called from the ASIC side when a DMA read request is
 * initiated. If there is a pending DMA transfer already, we copy the
 * data immediately, otherwise we record the DMA buffer for use when we
 * get to actually doing the transfer.
 * @return number of bytes transfered
 */
uint32_t ide_read_data_dma( uint32_t addr, uint32_t length )
{
    if( idereg.state == IDE_STATE_DMA_READ ) {
	int xferlen = length;
	int remaining = idereg.data_length - idereg.data_offset;
	if( xferlen > remaining )
	    xferlen = remaining;
	mem_copy_to_sh4( addr, data_buffer + idereg.data_offset, xferlen );
	idereg.data_offset += xferlen;
	if( idereg.data_offset >= idereg.data_length ) {
	    idereg.data_offset = -1;
	    idereg.state = IDE_STATE_IDLE;
	    idereg.status = 0x50;
	    ide_raise_interrupt();
	    asic_event( EVENT_IDE_DMA );
	}
	return xferlen;
    }
    return 0;
}

void ide_write_data_pio( uint16_t val ) {
    if( idereg.state == IDE_STATE_CMD_WRITE ) {
	WRITE_BUFFER(val);
	idereg.data_offset+=2;
	if( idereg.data_offset >= idereg.data_length ) {
	    idereg.state = IDE_STATE_BUSY;
	    idereg.status = (idereg.status & ~IDE_STATUS_DRQ) | IDE_STATUS_BSY;
	    idereg.data_offset = -1;
	    ide_packet_command(data_buffer);
	}
    } else if( idereg.state == IDE_STATE_PIO_WRITE ) {
	WRITE_BUFFER(val);
	if( idereg.data_offset >= idereg.data_length ) {
	    idereg.state = IDE_STATE_BUSY;
	    idereg.data_offset = -1;
	    idereg.status = (idereg.status & ~IDE_STATUS_DRQ) | IDE_STATUS_BSY;
	    /* ??? - no data writes yet anyway */
	}
    }
}

void ide_write_control( uint8_t val ) {
    if( IS_IDE_IRQ_ENABLED() ) {
	if( (val & 0x02) != 0 && idereg.intrq_pending != 0 )
	    asic_clear_event( EVENT_IDE );
    } else {
	if( (val & 0x02) == 0 && idereg.intrq_pending != 0 )
	    asic_event( EVENT_IDE );
    }
    idereg.control = val;
}

void ide_write_command( uint8_t val ) {
    ide_clear_interrupt();
    idereg.command = val;
    switch( val ) {
    case IDE_CMD_NOP: /* Effectively an "abort" */
	idereg.state = IDE_STATE_IDLE;
	idereg.status = 0x51;
	idereg.error = 0x04;
	idereg.data_offset = -1;
	ide_raise_interrupt();
	return;
    case IDE_CMD_RESET_DEVICE:
	ide_reset();
	break;
    case IDE_CMD_PACKET:
	ide_start_command_packet_write();
	break;
    case IDE_CMD_SET_FEATURE:
	switch( idereg.feature ) {
	case IDE_FEAT_SET_TRANSFER_MODE:
	    switch( idereg.count & 0xF8 ) {
	    case IDE_XFER_PIO:
		INFO( "Set PIO default mode: %d", idereg.count&0x07 );
		break;
	    case IDE_XFER_PIO_FLOW:
		INFO( "Set PIO Flow-control mode: %d", idereg.count&0x07 );
		break;
	    case IDE_XFER_MULTI_DMA:
		INFO( "Set Multiword DMA mode: %d", idereg.count&0x07 );
		break;
	    case IDE_XFER_ULTRA_DMA:
		INFO( "Set Ultra DMA mode: %d", idereg.count&0x07 );
		break;
	    default:
		INFO( "Setting unknown transfer mode: %02X", idereg.count );
		break;
	    }
	    break;
	default:
	    WARN( "IDE: unimplemented feature: %02X", idereg.feature );
	}
	ide_raise_interrupt( );
	break;
    default:
	WARN( "IDE: Unimplemented command: %02X", val );
    }
    idereg.status = (idereg.status | IDE_STATUS_DRDY | IDE_STATUS_SERV) & (~IDE_STATUS_CHK);
}

/**
 * Execute a packet command. This particular method is responsible for parsing
 * the command buffers (12 bytes), and generating the appropriate responses, 
 * although the actual implementation is mostly delegated to gdrom.c
 */
void ide_packet_command( unsigned char *cmd )
{
    uint32_t length, datalen;
    uint32_t lba, status;
    int mode;
    int blocksize = idereg.lba1 + (idereg.lba2<<8);

    /* Okay we have the packet in the command buffer */
    INFO( "ATAPI packet: %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X", 
	  cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7],
	  cmd[8], cmd[9], cmd[10], cmd[11] );
    switch( cmd[0] ) {
    case PKT_CMD_TEST_READY:
	if( !gdrom_is_mounted() ) {
	    ide_set_packet_result( PKT_ERR_NODISC );
	} else {
	    ide_set_packet_result( 0 );
	    ide_raise_interrupt();
	    idereg.status = 0x50;
	}
	break;
    case PKT_CMD_IDENTIFY:
	lba = cmd[2];
	if( lba >= sizeof(gdrom_ident) ) {
	    ide_set_error(PKT_ERR_BADFIELD);
	} else {
	    length = cmd[4];
	    if( lba+length > sizeof(gdrom_ident) )
		length = sizeof(gdrom_ident) - lba;
	    memcpy( data_buffer, gdrom_ident + lba, length );
	    ide_start_packet_read( length, blocksize );
	}
	break;
    case PKT_CMD_SENSE:
	length = cmd[4];
	if( length > 10 )
	    length = 10;
	memcpy( data_buffer, idereg.gdrom_sense, length );
	ide_start_packet_read( length, blocksize );
	break;
    case PKT_CMD_READ_TOC:
	length = (cmd[3]<<8) | cmd[4];
	if( length > sizeof(struct gdrom_toc) )
	    length = sizeof(struct gdrom_toc);

	status = gdrom_get_toc( data_buffer );
	if( status != PKT_ERR_OK ) {
	    ide_set_packet_result( status );
	} else {
	    ide_start_packet_read( length, blocksize );
	}
	break;
    case PKT_CMD_SESSION_INFO:
	length = cmd[4];
	if( length > 6 )
	    length = 6;
	status = gdrom_get_info( data_buffer, cmd[2] );
	if( status != PKT_ERR_OK ) {
	    ide_set_packet_result( status );
	} else {
	    ide_start_packet_read( length, blocksize );
	}
	break;
    case PKT_CMD_READ_SECTOR:
	lba = cmd[2] << 16 | cmd[3] << 8 | cmd[4];
	length = cmd[8] << 16 | cmd[9] << 8 | cmd[10]; /* blocks */
	switch( cmd[1] ) {
	case 0x20: mode = GDROM_MODE1; break;
	case 0x24: mode = GDROM_GD; break;
	case 0x28: mode = GDROM_MODE1; break; /* ??? */
	case 0x30: mode = GDROM_RAW; break;
	default:
	    ERROR( "Unrecognized read mode '%02X' in GD-Rom read request", cmd[1] );
	    ide_set_packet_result( PKT_ERR_BADFIELD );
	    return;
	}

	if( length > data_buffer_len ) {
	    do {
		data_buffer_len = data_buffer_len << 1;
	    } while( data_buffer_len < length );
	    data_buffer = realloc( data_buffer, MAX_SECTOR_SIZE * data_buffer_len );
	}

	datalen = data_buffer_len;
	status = gdrom_read_sectors( lba, length, mode, data_buffer, &datalen );
	if( status != 0 ) {
	    ide_set_packet_result( status );
	    idereg.gdrom_sense[5] = (lba >> 16) & 0xFF;
	    idereg.gdrom_sense[6] = (lba >> 8) & 0xFF;
	    idereg.gdrom_sense[7] = lba & 0xFF;
	} else {
	    ide_start_packet_read( datalen, blocksize );
	}
	break;
    case PKT_CMD_SPIN_UP:
	/* do nothing? */
	ide_set_packet_result( PKT_ERR_OK );
	ide_raise_interrupt();
	break;
    case PKT_CMD_71:
	/* This is a weird one. As far as I can tell it returns random garbage
	 * (and not even the same length each time, never mind the same data).
	 * For sake of something to do, it returns the results from a test dump
	 */
	memcpy( data_buffer, gdrom_71, sizeof(gdrom_71) );
	ide_start_packet_read( sizeof(gdrom_71), blocksize );
	break;
    default:
	ide_set_packet_result( PKT_ERR_BADCMD ); /* Invalid command */
	return;
    }
}
