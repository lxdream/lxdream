/**
 * $Id: ide.c,v 1.14 2006-05-23 13:11:45 nkeynes Exp $
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
    idereg.status = 0x00;
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
    fwrite( data_buffer, data_buffer_len, 1, f );
}

static int ide_load_state( FILE *f )
{
    uint32_t length;
    fread( &idereg, sizeof(idereg), 1, f );
    fread( &length, sizeof(uint32_t), 1, f );
    if( length > data_buffer_len ) {
	if( data_buffer != NULL )
	    free( data_buffer );
	data_buffer = malloc( length );
	assert( data_buffer != NULL );
	data_buffer_len = length;
    }
    fread( data_buffer, length, 1, f );
    return 0;
}

/************************ State transitions *************************/

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
    idereg.status = IDE_STATUS_DRDY | IDE_STATUS_DRQ;
    idereg.count = IDE_COUNT_IO;
    idereg.data_length = length;
    idereg.data_offset = 0;
    if( dma ) {
	idereg.state = IDE_STATE_DMA_READ;
	idereg.status |= IDE_STATUS_DMRD;
    } else {
	idereg.state = IDE_STATE_PIO_READ;
	idereg.lba1 = length & 0xFF;
	idereg.lba2 = (length >> 8) & 0xFF;
	//	idereg.lba1 = blocksize & 0xFF;
	//	idereg.lba2 = blocksize >> 8; 
	idereg.block_length = blocksize;
	idereg.block_left = blocksize;
    }
}

static void ide_start_packet_read( int length, int blocksize )
{
    ide_start_read( length, blocksize, idereg.feature & IDE_FEAT_DMA ? TRUE : FALSE );
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
	    idereg.status &= ~IDE_STATUS_DRQ;
	    ide_raise_interrupt();
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

void ide_set_packet_result( uint16_t result )
{
    idereg.gdrom_sense[0] = 0xf0;
    idereg.gdrom_sense[2] = result & 0xFF;
    idereg.gdrom_sense[8] = (result >> 8) & 0xFF;
    idereg.error = (result & 0x0F) << 4;
    if( result != 0 ) {
	idereg.status = 0x51;
    } else {
	idereg.status = idereg.status & ~(IDE_STATUS_BSY|IDE_STATUS_CHK);
    }
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

    ide_raise_interrupt( );
    /* Okay we have the packet in the command buffer */
    WARN( "ATAPI: Received Packet command: %02X", cmd[0] );
    fwrite_dump( (unsigned char *)cmd, 12, stderr );
    switch( cmd[0] ) {
    case PKT_CMD_TEST_READY:
	if( !gdrom_is_mounted() ) {
	    ide_set_packet_result( PKT_ERR_NODISC );
	    return;
	}
	ide_set_packet_result( 0 );
	idereg.status = 0x50;
	return;
	break;
    case PKT_CMD_IDENTIFY:
	lba = cmd[2];
	if( lba >= sizeof(gdrom_ident) ) {
	    ide_set_error(PKT_ERR_BADFIELD);
	    return;
	}
	length = cmd[4];
	if( lba+length > sizeof(gdrom_ident) )
	    length = sizeof(gdrom_ident) - lba;
	memcpy( data_buffer, gdrom_ident + lba, length );
	ide_start_packet_read( length, blocksize );
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
	    return;
	}
	ide_start_packet_read( length, blocksize );
	break;
    case PKT_CMD_DISC_INFO:
	length = cmd[4];
	if( length > 6 )
	    length = 6;
	status = gdrom_get_info( data_buffer, cmd[2] );
	if( status != PKT_ERR_OK ) {
	    ide_set_packet_result( status );
	    return;
	}
	ide_start_packet_read( length, blocksize );
	break;
    case PKT_CMD_READ_SECTOR:
	lba = cmd[2] << 16 | cmd[3] << 8 | cmd[4];
	length = cmd[8] << 16 | cmd[9] << 8 | cmd[10]; /* blocks */
	if( length > data_buffer_len ) {
	    do {
		data_buffer_len = data_buffer_len << 1;
	    } while( data_buffer_len < length );
	    data_buffer = realloc( data_buffer, data_buffer_len );
	}

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
	datalen = data_buffer_len;
	status = gdrom_read_sectors( lba, length, mode, data_buffer, &datalen );
	if( status != 0 ) {
	    ide_set_packet_result( status );
	    idereg.gdrom_sense[5] = (lba >> 16) & 0xFF;
	    idereg.gdrom_sense[6] = (lba >> 8) & 0xFF;
	    idereg.gdrom_sense[7] = lba & 0xFF;
	    return;
	}
	ide_start_packet_read( datalen, blocksize );
	break;
    case PKT_CMD_SPIN_UP:
	/* do nothing? */
	break;
    default:
	ide_set_packet_result( PKT_ERR_BADCMD ); /* Invalid command */
	return;
    }
    ide_set_packet_result( PKT_ERR_OK );
}
