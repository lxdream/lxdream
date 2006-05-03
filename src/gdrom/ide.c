/**
 * $Id: ide.c,v 1.12 2006-05-03 12:52:38 nkeynes Exp $
 *
 * IDE interface implementation
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

struct dreamcast_module ide_module = { "IDE", ide_init, ide_reset, NULL, NULL,
				       NULL, ide_save_state, ide_load_state };

struct ide_registers idereg;

static unsigned char command_buffer[12];
unsigned char *data_buffer = NULL;
uint32_t data_buffer_len = 0;

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
}

static void ide_save_state( FILE *f )
{
    

}

static int ide_load_state( FILE *f )
{
    return 0;
}

static void ide_set_write_buffer( unsigned char *buf, int len )
{
    idereg.status |= IDE_ST_DATA;
    idereg.data = buf;
    idereg.datalen = len;
    idereg.writeptr = (uint16_t *)buf;
    idereg.readptr = NULL;
}

static void ide_set_read_buffer( unsigned char *buf, int len, int blocksize )
{
    idereg.status |= IDE_ST_DATA;
    idereg.data = buf;
    idereg.datalen = len;
    idereg.readptr = (uint16_t *)buf;
    idereg.writeptr = NULL;
    idereg.lba1 = len&0xFF;
    idereg.lba2 = len>>8;
    idereg.blocksize = idereg.blockleft = blocksize;
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
    if( (idereg.status & IDE_ST_BUSY) == 0 )
	ide_clear_interrupt();
    return idereg.status;
}

uint16_t ide_read_data_pio( void ) {
    if( idereg.readptr == NULL )
        return 0xFFFF;
    uint16_t rv = *idereg.readptr++;
    idereg.datalen-=2;
    idereg.blockleft-=2;
    if( idereg.datalen <=0 ) {
        idereg.readptr = NULL;
        idereg.status &= ~IDE_ST_DATA;
	ide_raise_interrupt();
    } else if( idereg.blockleft <= 0 ) {
	ide_raise_interrupt();
	idereg.blockleft = idereg.blocksize;
    }
    return rv;
}

void ide_write_data_pio( uint16_t val ) {
    if( idereg.writeptr == NULL )
        return;
    *idereg.writeptr++ = val;
    idereg.datalen-=2;
    if( idereg.datalen <= 0 ) {
	int len = ((unsigned char *)idereg.writeptr) - idereg.data;
        idereg.writeptr = NULL;
        idereg.status &= ~IDE_ST_DATA;
        ide_write_buffer( idereg.data, len );
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
	ide_set_write_buffer(command_buffer,12);
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
    idereg.status = (idereg.status | IDE_ST_READY | IDE_ST_SERV) & (~IDE_ST_ERROR);
}

void ide_set_packet_error( uint16_t error )
{
    idereg.gdrom_error = error;
    idereg.error = (error & 0x0F) << 4;
    if( error != 0 ) {
	idereg.status = 0x51;
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
	    ide_set_packet_error( PKT_ERR_NODISC );
	    return;
	}
	ide_set_packet_error( 0 );
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
	ide_set_read_buffer(gdrom_ident + lba, length, blocksize);
	break;
    case PKT_CMD_SENSE:
	memset( data_buffer, 0, 10 );
	length = cmd[4];
	if( length > 10 )
	    length = 10;
	data_buffer[0] = 0xf0;
	data_buffer[2] = idereg.gdrom_error & 0xFF;
	data_buffer[8] = (idereg.gdrom_error >> 8) & 0xFF;
	ide_set_read_buffer( data_buffer, length , blocksize );
	break;
    case PKT_CMD_READ_TOC:
	if( !gdrom_get_toc( data_buffer ) ) {
	    ide_set_packet_error( PKT_ERR_NODISC ); /* No disc in drive */
	    return;
	} 
	length = (cmd[3]<<8) | cmd[4];
	if( length > sizeof(struct gdrom_toc) )
	    length = sizeof(struct gdrom_toc);
	ide_set_read_buffer( data_buffer, length, blocksize );
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
	    ide_set_packet_error( PKT_ERR_BADFIELD );
	    return;
	}

	status = gdrom_read_sectors( lba, length, mode, data_buffer, &data_buffer_len );
	if( status != 0 ) {
	    ide_set_packet_error( status );
	    data_buffer[6] = (lba >> 8) & 0xFF;
	    data_buffer[7] = lba & 0xFF;
	    return;
	}
	ide_set_read_buffer( data_buffer, datalen, blocksize );
	break;
    default:
	ide_set_packet_error( PKT_ERR_BADCMD ); /* Invalid command */
	return;
    }
    ide_set_packet_error( PKT_ERR_OK );
}

void ide_write_buffer( unsigned char *data, int datalen ) {
    switch( idereg.command ) {
        case IDE_CMD_PACKET:
	    ide_packet_command( data );
            break;
    }
}

/**
 * DMA read request
 *
 * This method is called from the ASIC side when a DMA read request is
 * initiated. If there is a pending DMA transfer already, we copy the
 * data immediately, otherwise we record the DMA buffer for use when we
 * get to actually doing the transfer.
 */
void ide_dma_read_req( uint32_t addr, uint32_t length )
{
    

}
