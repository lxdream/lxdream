/*
 * ide.c    31 Mar 2004  - IDE Interface implementation
 *
 * Copyright (c) 2004 Nathan Keynes. Distribution and modification permitted
 * under the terms of the GNU General Public License version 2 or later. 
 */
#include <stdlib.h>
#include "modules.h"
#include "ide.h"

#define MAX_WRITE_BUF 4096;

void ide_init( void );
void ide_init( void );

struct dreamcast_module ide_module = { "IDE", ide_init, ide_reset, NULL, NULL,
				       NULL, NULL };

struct ide_registers idereg;

static char command_buffer[12];

/* "\0\0\0\0\xb4\x19\0\0\x08SE      REV 6.42990316" */
char gdrom_ident[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0xb4, 0x19, 0x00,
                       0x00, 0x08, 0x53, 0x45, 0x20, 0x20, 0x20, 0x20,
                       0x20, 0x20, 0x52, 0x65, 0x76, 0x20, 0x36, 0x2e,
                       0x34, 0x32, 0x39, 0x39, 0x30, 0x33, 0x31, 0x36 };


void set_write_buffer( char *buf, int len )
{
    idereg.status |= IDE_ST_DATA;
    idereg.data = buf;
    idereg.datalen = len;
    idereg.writeptr = (uint16_t *)buf;
    idereg.readptr = NULL;
}

void set_read_buffer( char *buf, int len )
{
    idereg.status |= IDE_ST_DATA;
    idereg.data = buf;
    idereg.datalen = len;
    idereg.readptr = (uint16_t *)buf;
    idereg.writeptr = NULL;
    idereg.lba1 = len&0xFF;
    idereg.lba2 = len>>8;
}

void ide_clear_interrupt( void )
{
    /* TODO */
}

void ide_init( void )
{

}

void ide_reset( void )
{
    ide_clear_interrupt();
    idereg.error = 0x01;
    idereg.count = 0x01;
    idereg.lba0 = 0x21;
    idereg.lba1 = 0x14;
    idereg.lba2 = 0xeb;
    idereg.feature = 0; /* Indeterminate really */
    idereg.status = 0x00;
    idereg.device = 0x00;
    idereg.disc = IDE_DISC_GDROM | IDE_DISC_READY;
}

uint16_t ide_read_data_pio( void ) {
    if( idereg.readptr == NULL )
        return 0xFFFF;
    uint16_t rv = *idereg.readptr++;
    idereg.datalen-=2;
    if( idereg.datalen <=0 ) {
        idereg.readptr = NULL;
        idereg.status &= ~IDE_ST_DATA;
    }
    return rv;
}

void ide_write_data_pio( uint16_t val ) {
    if( idereg.writeptr == NULL )
        return;
    *idereg.writeptr++ = val;
    idereg.datalen-=2;
    if( idereg.datalen <= 0 ) {
        idereg.writeptr = NULL;
        idereg.status &= ~IDE_ST_DATA;
        ide_write_buffer( idereg.data );
    }
}

void ide_write_control( uint8_t val ) {
    /* TODO: In theory we can cause a soft-reset here, but the DC doesn't
     * appear to support it.
     */
}

void ide_write_command( uint8_t val ) {
    idereg.command = val;
    switch( val ) {
        case IDE_CMD_RESET_DEVICE:
            ide_reset();
            break;
        case IDE_CMD_PACKET:
            set_write_buffer(command_buffer,12);
            break;
        default:
            WARN( "IDE: Unimplemented command: %02X", val );
    }
    idereg.status |= IDE_ST_READY | IDE_ST_SERV;
}

void ide_write_buffer( char *data ) {
    uint16_t length;
    switch( idereg.command ) {
        case IDE_CMD_PACKET:
            /* Okay we have the packet in the command buffer */
            WARN( "ATAPI: Received Packet command: %02X", data[0] );
            
            switch( command_buffer[0] ) {
                case PKT_CMD_IDENTIFY:
                    /* NB: Bios sets data[4] = 0x08, no idea what this is for;
                     * different values here appear to have no effect.
                     */
                    length = *((uint16_t*)(data+2));
                    if( length > sizeof(gdrom_ident) )
                        length = sizeof(gdrom_ident);
                    set_read_buffer(gdrom_ident, length);
                    break;
            }
            break;
    }
}
