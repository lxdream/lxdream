/**
 * $Id: ide.c,v 1.8 2006-03-22 14:29:02 nkeynes Exp $
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

#include <stdlib.h>
#include "dream.h"
#include "asic.h"
#include "gdrom/ide.h"
#include "gdrom/gdrom.h"

#define MAX_WRITE_BUF 4096;

void ide_init( void );
void ide_init( void );

struct dreamcast_module ide_module = { "IDE", ide_init, ide_reset, NULL, NULL,
				       NULL, NULL, NULL };

struct ide_registers idereg;

static unsigned char command_buffer[12];

/* "\0\0\0\0\xb4\x19\0\0\x08SE      REV 6.42990316" */
unsigned char gdrom_ident[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0xb4, 0x19, 0x00,
                       0x00, 0x08, 0x53, 0x45, 0x20, 0x20, 0x20, 0x20,
                       0x20, 0x20, 0x52, 0x65, 0x76, 0x20, 0x36, 0x2e,
                       0x34, 0x32, 0x39, 0x39, 0x30, 0x33, 0x31, 0x36 };


gdrom_disc_t gdrom_disc = NULL;

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

void ide_init( void )
{

}

void ide_reset( void )
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
    idereg.disc = (gdrom_disc == NULL ? IDE_DISC_NONE : (IDE_DISC_CDROM|IDE_DISC_READY));
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
    idereg.status |= IDE_ST_READY | IDE_ST_SERV;
}

void ide_packet_command( unsigned char *cmd )
{
    uint32_t length;
    uint32_t lba;
    int blocksize = idereg.lba1 + (idereg.lba2<<8);

    ide_raise_interrupt( );
    /* Okay we have the packet in the command buffer */
    WARN( "ATAPI: Received Packet command: %02X", cmd[0] );
    fwrite_dump( (unsigned char *)cmd, 12, stderr );
    switch( cmd[0] ) {
    case PKT_CMD_IDENTIFY:
	/* NB: Bios sets cmd[4] = 0x08, no idea what this is for;
	 * different values here appear to have no effect.
	 */
	length = *((uint16_t*)(cmd+2));
	if( length > sizeof(gdrom_ident) )
	    length = sizeof(gdrom_ident);
	ide_set_read_buffer(gdrom_ident, length, blocksize);
	break;
    case PKT_CMD_READ_TOC:
	
	break;
    case PKT_CMD_READ_SECTOR:
	lba = cmd[2] << 16 | cmd[3] << 8 | cmd[4];
	length = cmd[8] << 16 | cmd[9] << 8 | cmd[10]; /* blocks */
	if( gdrom_disc == NULL ) {
	    ide_set_error( 0x50 );
	    return;
	}
	/*
	if( gdrom_disc->read_data_sectors( lba, length ) == FALSE ) {
	    ide_set_error( 0x50 );
	    return;
	}
	
	*/	
	break;
    }
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
