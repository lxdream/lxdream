#include <assert.h>
#include <stdlib.h>
#include "ide.h"
#include "lib.h"

#define IDE_BASE 0xA05F7000

#define IDE_ALTSTATUS IDE_BASE+0x018
#define IDE_UNKNOWN   IDE_BASE+0x01C
#define IDE_DATA      IDE_BASE+0x080 /* 16 bits */
#define IDE_FEATURE   IDE_BASE+0x084
#define IDE_COUNT     IDE_BASE+0x088
#define IDE_LBA0      IDE_BASE+0x08C
#define IDE_LBA1      IDE_BASE+0x090
#define IDE_LBA2      IDE_BASE+0x094
#define IDE_DEVICE    IDE_BASE+0x098
#define IDE_COMMAND   IDE_BASE+0x09C
#define IDE_ACTIVATE  IDE_BASE+0x4E4

#define IDE_DEVCONTROL IDE_ALTSTATUS
#define IDE_ERROR      IDE_FEATURE
#define IDE_STATUS     IDE_COMMAND

#define IDE_DMA_ADDR   IDE_BASE+0x404
#define IDE_DMA_SIZE   IDE_BASE+0x408
#define IDE_DMA_DIR    IDE_BASE+0x40C
#define IDE_DMA_CTL1   IDE_BASE+0x414
#define IDE_DMA_CTL2   IDE_BASE+0x418
#define IDE_DMA_MAGIC  IDE_BASE+0x4B8
#define IDE_DMA_STATUS IDE_BASE+0x4F8

#define DMA_BASE 0xFFA00000
#define DMA_SAR1    DMA_BASE+0x010
#define DMA_DAR1    DMA_BASE+0x014
#define DMA_TCR1    DMA_BASE+0x018
#define DMA_CHCR1   DMA_BASE+0x01C
#define DMA_SAR2    DMA_BASE+0x020
#define DMA_DAR2    DMA_BASE+0x024
#define DMA_TCR2    DMA_BASE+0x028
#define DMA_CHCR2   DMA_BASE+0x02C
#define DMA_SAR3    DMA_BASE+0x030
#define DMA_DAR3    DMA_BASE+0x034
#define DMA_TCR3    DMA_BASE+0x038
#define DMA_CHCR3   DMA_BASE+0x03C
#define DMA_DMAOR   DMA_BASE+0x040
#define QUEUECR0    0xFF000038
#define QUEUECR1    0xFF00003C

#define IDE_CMD_RESET 0x08
#define IDE_CMD_PACKET 0xA0
#define IDE_CMD_IDENTIFY_PACKET_DEVICE 0xA1
#define IDE_CMD_IDENTIFY_DEVICE 0xEC

#define MMC_CMD_GET_CONFIGURATION 0x46
#define GD_CMD_IDENTIFY 0x11 /* guessing */


#define IDE_DMA_MAGIC_VALUE 0x8843407F


#define MAX_WAIT     10000000
#define MAX_IRQ_WAIT 1000000000

/**
 * Dump all ide registers to stdout.
 */
void ide_dump_registers() {
    int i,j;
    printf( "IDE registers:\n");
    printf( "Stats: %02X ", byte_read(IDE_ALTSTATUS) );
    printf( "Error: %02X ", byte_read(IDE_ERROR) );
    printf( "Count: %02X ", byte_read(IDE_COUNT) );
    printf( "Dvice: %02X ", byte_read(IDE_DEVICE) );
    if( long_read(ASIC_STATUS1)&1 ) {
	printf( "INTRQ! " );
    }
    if( (long_read(ASIC_STATUS0)>>14)&1 ) {
	printf( "DMARQ! " );
    }
    printf( "\nLBA 0: %02X ", byte_read(IDE_LBA0) );
    printf( "LBA 1: %02X ", byte_read(IDE_LBA1) );
    printf( "LBA 2: %02X ", byte_read(IDE_LBA2) );
    printf( "0x01C: %02X\n", byte_read(IDE_UNKNOWN) );
    printf( "DAddr: %08X ", long_read(IDE_DMA_ADDR) );
    printf( "DSize: %08X ", long_read(IDE_DMA_SIZE) );
    printf( "DDir : %08X ", long_read(IDE_DMA_DIR) );
    printf( "DCtl1: %08X ", long_read(IDE_DMA_CTL1) );
    printf( "DCtl2: %08X\n", long_read(IDE_DMA_CTL2) );
    printf( "DStat: %08X\n", long_read(IDE_DMA_STATUS) );
    printf( "ASIC: " );
    for( i=0; i<12; i+=4 ) {
	unsigned int val = long_read(ASIC_STATUS0+i);
	for( j=0; j<32; j++ ) {
	    if( val & (1<<j) ) {
		printf( "%d ", j );
	    }
	}
	printf( "| " );
    }
    printf( "\n" );
}

/**
 * Wait for the IDE INTRQ line to go active (bit 0 of the second word)
 * @return 0 on success, non-zero on timeout
 */
int ide_wait_irq() {
    unsigned int status;
    int i;
    for( i=0; i<MAX_WAIT; i++ ) {
	status = long_read( ASIC_STATUS1 );
	if( (status&1) != 0 )
	    return 0;
    }
    return 1;
}

/**
 * Wait for the IDE BSY flag to be de-asserted.
 * @return 0 on success, non-zero on timeout
 */
int ide_wait_ready() {
    unsigned char status;
    int i;
    for( i=0; i<MAX_WAIT; i++ ) {
        status = byte_read(IDE_ALTSTATUS);
	if( (status & 0x80) != 0x80 )
	    return 0;
    }
    printf( "Timeout waiting for IDE to become ready\n" );
    ide_dump_registers();
    return 1;
}

int ide_wait_dma() {
    unsigned int status;
    int i;
    for( i=0; i<MAX_WAIT; i++ ) {
	status = long_read(IDE_DMA_CTL2);
	if( (status & 1) == 0 )
	    return 0;
    }
    printf( "[IDE] Timeout waiting for DMA to become ready\n" );
    return 1;
}

/**
 * Write the command packet out to the interface.
 * @param cmd 12 byte ATAPI command packet
 * @param dma 1 = dma mode, 0 = pio mode
 */
int ide_write_command_packet( char *cmd, int dma ) 
{
    int i, status;
    unsigned short *spkt = (unsigned short *)cmd;
    unsigned short length = 8;
    if( ide_wait_ready() )
	return 1;
    byte_write( IDE_FEATURE, dma );
    byte_write( IDE_COUNT, 0 );
    byte_write( IDE_LBA0, 0 );
    byte_write( IDE_LBA1, (length&0xFF) );
    byte_write( IDE_LBA2, (length>>8)&0xFF );
    byte_write( IDE_DEVICE, 0 );
    byte_write( IDE_COMMAND, IDE_CMD_PACKET );
    status = byte_read(IDE_ALTSTATUS); /* delay 1 PIO cycle as per spec */
    printf( "After writing PACKET command byte:\n" );
    ide_dump_registers();
    /* Wait until device is ready to accept command */
    if( ide_wait_ready() )
	return 1;
    printf( "Device ready to receive packet:\n" );
    ide_dump_registers();

    /* Write the command */
    for( i=0; i<6; i++ ) {
        word_write( IDE_DATA, spkt[i] );
    }
    printf( "After writing command packet:\n" );
    ide_dump_registers();
}

int ide_read_pio( char *buf, int buflen ) {
    int i;
    unsigned short *bufptr = (unsigned short *)buf;
    unsigned int length = 0, avail;
    int status;
    
    while(1) {
	if( ide_wait_ready() )
	    return -1;
	status = byte_read( IDE_STATUS );
	if( (status & 0xE9) == 0x48 ) {
	    /* Bytes available */
	    avail = (byte_read( IDE_LBA1 )) | (byte_read(IDE_LBA2)<<8);
	    for( i=0; i<avail; i+=2 ) {
		if( buflen > 0 ) {
		    *bufptr++ = word_read(IDE_DATA);
		    buflen-=2;
		}
	    }
	    length += avail;
	    if( avail == 0 ) {
		/* Should never happen */
		printf( "[IDE] Unexpected read length 0\n" );
		return -1;
	    }
	} else {
	    if( status&0x01 ) {
		printf( "[IDE] ERROR! (%02X)\n", status );
		return -1;
	    } else if( (status&0x08) == 0 ) {
		/* No more data */
		return length;
	    } else {
		printf( "[IDE] Unexpected status result: %02X\n", status );
		return -1;
	    }
	}
    }
}

int ide_read_dma( char *buf, int buflen ) 
{
    int status;
    
    long_write( IDE_DMA_CTL1, 1 );
    long_write( IDE_DMA_CTL2, 1 );
    
    printf( "Started DMA\n" );
    ide_dump_registers();
    
    ide_wait_irq();
    printf( "After IRQ\n" );
    ide_dump_registers();
    long_write( IDE_DMA_CTL1, 0 );
    status = ide_wait_dma();
    printf( "After DMA finished\n");
    ide_dump_registers();
    if( status != 0 ) {
	return -1;
    }
    status = long_read(ASIC_STATUS0);
    if( (status & (1<<14)) == 0 ) {
	printf( "DMARQ cleared already\n");
    } else {
	/*
	status &= ~(1<<14);
	long_write(ASIC_STATUS0, status);
	status = long_read(ASIC_STATUS0);
	*/
	byte_read(IDE_STATUS );
	if( (status & (1<<14)) == 0 ) {
	    printf( "DMARQ cleared successfully\n" );
	} else {
	    printf( "DMARQ not cleared: %08X\n", long_read(ASIC_STATUS0) );
	}
    }
    status = ide_wait_ready();
    printf( "After IDE ready\n");
    ide_dump_registers();
    if( status != 0 ) {
	return -1;
    }
    return long_read( IDE_DMA_STATUS );
}

int ide_do_packet_command_pio( char *cmd, char *buf, int length ) 
{
    ide_write_command_packet( cmd, 0 );
    length = ide_read_pio( buf, length );
    return length;
}

int ide_do_packet_command_dma( char *cmd, char *buf, int length )
{
    long_write( QUEUECR0, 0x10 );
    long_write( QUEUECR1, 0x10 );
    long_write( IDE_DMA_MAGIC, IDE_DMA_MAGIC_VALUE );
    long_write( IDE_DMA_ADDR, (unsigned int)buf );
    long_write( IDE_DMA_SIZE, length );
    long_write( IDE_DMA_DIR, 1 );
    ide_write_command_packet( cmd, 1 );
    length = ide_read_dma( buf, length );
    return length;
}

void ide_activate() {
  register unsigned long p, x;

  /* Reactivate GD-ROM drive */

  *((volatile unsigned long *)0xa05f74e4) = 0x1fffff;
  for(p=0; p<0x200000/4; p++)
    x = ((volatile unsigned long *)0xa0000000)[p];
}


int ide_init()
{
    ide_activate();

    if( ide_wait_ready() )
	return -1;

    /** Set Default PIO mode */
    byte_write( IDE_FEATURE, 0x03 );
    byte_write( IDE_COUNT, 0x0B );
    byte_write( IDE_COMMAND, 0xEF );

    if( ide_wait_ready() )
	return -1;
    
    /** Set Multi-word DMA mode 2 */
    long_write( 0xA05F7490, 0x222 );
    long_write( 0xA05F7494, 0x222 );
    byte_write( IDE_FEATURE, 0x03 );
    byte_write( IDE_COUNT, 0x22 );
    byte_write( IDE_COMMAND, 0xEF );
    if( ide_wait_ready() )
	return -1;

    word_write( 0xA05F7480, 0x400 );
    long_write( 0xA05F7488, 0x200 );
    long_write( 0xA05F748C, 0x200 );
    long_write( 0xA05F74A0, 0x2001 );
    long_write( 0xA05F74A4, 0x2001 );
    long_write( 0xA05F74B4, 0x0001 );
}

int ide_sense_error( char *buf ) 
{
    char cmd[12] = { 0x13,0,0,0, 10,0,0,0, 0,0,0,0 };
    return ide_do_packet_command_pio( cmd, buf, 10 );
}

void ide_print_sense_error()
{
    char buf[10];
    if( ide_sense_error(buf) != 10 ) {
	printf( "ERROR - Sense error failed!\n" );
	return;
    }
    int major = buf[2] & 0xFF;
    int minor = buf[8] & 0xFF;
    printf( "[IDE] Error code %02X,%02X\n", major, minor );
}

int ide_test_ready()
{
    char cmd[12] = { 0,0,0,0, 0,0,0,0, 0,0,0,0 };
    int length = ide_do_packet_command_pio( cmd, NULL, 0 );
    return length;
}

int ide_read_toc( char *buf, int length ) 
{
    char cmd[12] = { 0x14,0,0,0, 0x98,0,0,0, 0,0,0,0 };
    return ide_do_packet_command_pio( cmd, buf, length );
}

int ide_get_session( int session, struct gdrom_session *session_data )
{
    char cmd[12] = {0x15, 0, session, 0, 6,0,0,0, 0,0,0,0 };
    char buf[6];
    int length = ide_do_packet_command_pio( cmd, buf, sizeof(buf) );
    if( length < 0 )
	return length;
    if( length != 6 )
	return -1;
    assert(length == 6);
    session_data->track = ((int)buf[2])&0xFF;
    session_data->lba = (((int)buf[3])&0xFF) << 16 | 
	(((int)buf[4])&0xFF) << 8 | 
	(((int)buf[5])&0xFF);
    return 0;
}

int ide_spinup( )
{
    char cmd[12] = {0x70,0x1F,0,0, 0,0,0,0, 0,0,0,0};
    int length = ide_do_packet_command_pio( cmd, NULL, 0 );
    return length;
}

int ide_unknown71( char *buf, int length )
{
    char cmd[12] = {0x71,0,0,0, 0,0,0,0, 0,0,0,0};
    return ide_do_packet_command_pio( cmd, buf, length );
}

int ide_read_sector_pio( uint32_t sector, uint32_t count, int mode,
			 char *buf, int length )
{
    char cmd[12] = { 0x30,0,0,0, 0,0,0,0, 0,0,0,0 };

    cmd[1] = mode;
    cmd[2] = (sector>>16)&0xFF;
    cmd[3] = (sector>>8)&0xFF;
    cmd[4] = sector&0xFF;
    cmd[8] = (count>>16)&0xFF;
    cmd[9] = (count>>8)&0xFF;
    cmd[10] = count&0xFF;
    return ide_do_packet_command_pio( cmd, buf, length );
}


int ide_read_sector_dma( uint32_t sector, uint32_t count, int mode,
			 char *buf, int length )
{
    char cmd[12] = { 0x30,0,0,0, 0,0,0,0, 0,0,0,0 };

    cmd[1] = mode;
    cmd[2] = (sector>>16)&0xFF;
    cmd[3] = (sector>>8)&0xFF;
    cmd[4] = sector&0xFF;
    cmd[8] = (count>>16)&0xFF;
    cmd[9] = (count>>8)&0xFF;
    cmd[10] = count&0xFF;
    return ide_do_packet_command_dma( cmd, buf, length );
}

int ide_read_something( )
{
    char cmd[12] = { 0x12,0,0,0, 0x0a,0,0,0, 0,0,0,0 };
    char result[10];
    ide_do_packet_command_pio( cmd, result, 10 );
    return 0;
}

int ide_read_status( char *buf, int length )
{
    char cmd[12] = { 0x40,0,0,0, 0xFF,0,0,0, 0,0,0,0 };

    return ide_do_packet_command_pio( cmd, buf, length );
}
		     
int ide_play_cd( char *buf, int length )
{
    char cmd[12] = { 0x21, 0x04,0,0, 0,0,0,0, 0,0,0,0 };
    return ide_do_packet_command_pio( cmd, buf, length );
}
