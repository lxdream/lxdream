/**
 * $Id: testide.c,v 1.4 2006-12-21 10:14:24 nkeynes Exp $
 *
 * IDE interface test cases. Covers all (known) IDE registers in the 
 * 5F7000 - 5F74FF range including DMA, but does not cover any GD-Rom
 * device behaviour (ie packet comands).
 *
 * These tests should be run with the drive empty.
 *
 * Copyright (c) 2006 Nathan Keynes.
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
#include <stdio.h>
#include "lib.h"
#include "ide.h"
#include "asic.h"

unsigned int test_count = 0, test_failures = 0;

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

#define IDE_DISC       IDE_LBA0
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

#define CHECK_REG_EQUALS( a, b, c ) if( b != c ) { fprintf(stderr, "Assertion failed at %s:%d %s(): expected %08X from register %08X, but was %08X\n", __FILE__, __LINE__, __func__, b, a, c ); return -1; }

/* Wait for the standard timeout for an INTRQ. If none is received, print an
 * error and return -1
 */
#define EXPECT_INTRQ() if( ide_wait_irq() != 0 ) { fprintf(stderr, "Timeout at %s:%d %s(): waiting for INTRQ\n", __FILE__, __LINE__, __func__ ); return -1; }

/* Check if the INTRQ line is currently cleared (ie inactive) */
#define CHECK_INTRQ_CLEAR() if ( (long_read( ASIC_STATUS1 ) & 1) != 0 ) { fprintf(stderr, "Assertion failed at %s:%d %s(): expected INTRQ to be cleared, but was raised.\n", __FILE__, __LINE__, __func__ ); return -1; }

#define EXPECT_READY() if( ide_wait_ready() != 0 ) { fprintf(stderr, "Timeout at %s:%d %s(): waiting for BSY flag to clear\n", __FILE__, __LINE__, __func__ ); return -1; }

int check_regs( uint32_t *regs,const char *file, int line, const char *fn ) 
{
    int i;
    int rv = 0;
    for( i=0; regs[i] != 0; i+=2 ) {
	uint32_t addr = regs[i];
	uint32_t val = regs[i+1];
	uint32_t actual;
	if( addr == IDE_DATA ) {
	    actual = (uint32_t)word_read(addr);
	    if( val != actual ) { 
		fprintf(stderr, "Assertion failed at %s:%d %s(): expected %04X from register %08X, but was %04X\n", file, line, fn, val, addr, actual ); 
		rv = -1;
	    }
	} else if( addr <= IDE_COMMAND ) {
	    actual = (uint32_t)byte_read(addr);
	    if( val != actual ) { 
		fprintf(stderr, "Assertion failed at %s:%d %s(): expected %02X from register %08X, but was %02X\n", file, line, fn, val, addr, actual ); 
		rv = -1;
	    }
	} else {
	    actual = long_read(addr);
	    if( val != actual ) { 
		fprintf(stderr, "Assertion failed at %s:%d %s(): expected %08X from register %08X, but was %08X\n", file, line, fn, val, addr, actual ); 
		rv = -1;
	    }
	}
    }
    return rv;
}

#define CHECK_REGS( r ) if( check_regs(r, __FILE__, __LINE__, __func__) != 0 ) { return -1; }


uint32_t post_packet_ready_regs[] = 
    { IDE_ALTSTATUS, 0x58,
      IDE_COUNT, 0x01,
      IDE_LBA1, 8,
      IDE_LBA2, 0,
      IDE_DEVICE, 0,
      IDE_STATUS, 0x58, 0, 0 };

uint32_t post_packet_cmd_regs[] = 
    { IDE_ALTSTATUS, 0xD0,
      IDE_ERROR, 0x00,
      IDE_COUNT, 0x01,
      IDE_LBA1, 8,
      IDE_LBA2, 0,
      IDE_DEVICE, 0,
      IDE_STATUS, 0xD0, 0, 0 };

uint32_t packet_cmd_error6_regs[] = 
    { IDE_ALTSTATUS, 0x51,
      IDE_ERROR, 0x60,
      IDE_COUNT, 0x03,
      IDE_LBA1, 8,
      IDE_LBA2, 0,
      IDE_DEVICE, 0,
      IDE_STATUS, 0x51, 0, 0 };

uint32_t packet_data_ready_regs[] = 
    { IDE_ALTSTATUS, 0x58,
      IDE_ERROR, 0x00,
      IDE_COUNT, 0x02,
      IDE_LBA1, 0x0C,
      IDE_LBA2, 0,
      IDE_DEVICE, 0,
      IDE_STATUS, 0x58, 0, 0 };


uint32_t post_packet_data_regs[] = 
    { IDE_ALTSTATUS, 0xD0,
      IDE_ERROR, 0x00,
      IDE_COUNT, 0x02,
      IDE_LBA1, 0x0C,
      IDE_LBA2, 0,
      IDE_DEVICE, 0,
      IDE_STATUS, 0xD0, 0, 0 };

uint32_t packet_complete_regs[] = 
    { IDE_ALTSTATUS, 0x50,
      IDE_ERROR, 0x00,
      IDE_COUNT, 0x03,
      IDE_LBA1, 0x0C,
      IDE_LBA2, 0,
      IDE_DEVICE, 0,
      IDE_STATUS, 0x50, 0, 0 };

int send_packet_command( const char *cmd )
{
    unsigned short *spkt = (unsigned short *)cmd;
    int i;

    EXPECT_READY();
    byte_write( IDE_FEATURE, 0 );
    byte_write( IDE_COUNT, 0 );
    byte_write( IDE_LBA0, 0 );
    byte_write( IDE_LBA1, 8 );
    byte_write( IDE_LBA2, 0 );
    byte_write( IDE_DEVICE, 0 );
    byte_write( IDE_COMMAND, 0xA0 );
    byte_read(IDE_ALTSTATUS); /* delay 1 PIO cycle */
    EXPECT_READY(); /* Wait until device is ready to accept command (usually immediate) */
    CHECK_INTRQ_CLEAR();
    CHECK_REGS( post_packet_ready_regs );
    
    /* Write the command */
    for( i=0; i<6; i++ ) {
        word_write( IDE_DATA, spkt[i] );
    }

    byte_read(IDE_ALTSTATUS); 

    // CHECK_REGS( post_packet_cmd_regs );
    EXPECT_INTRQ();
    EXPECT_READY();
    return 0;
}

int read_pio( char *buf, int expected_length ) {
    uint32_t ready_regs[] = {
	IDE_ALTSTATUS, 0x58,
	IDE_ERROR, 0x00,
	IDE_COUNT, 0x02,
	IDE_LBA1, expected_length & 0xFF,
	IDE_LBA2, (expected_length >> 8),
	IDE_DEVICE, 0,
	IDE_STATUS, 0x58, 
	0, 0 };    

    int i;
    unsigned short *bufptr = (unsigned short *)buf;
    unsigned int length = 0, avail;
    int status;
    
    CHECK_REGS( ready_regs );
    for( i=0; i<expected_length; i+=2 ) {
	*bufptr++ = word_read(IDE_DATA);
    }

    EXPECT_INTRQ();
    EXPECT_READY();
    ready_regs[1] = 0x50;
    ready_regs[5] = 0x03;
    ready_regs[13] = 0x50;
    CHECK_REGS( ready_regs );
    return 0;
}

#define IDE_TEST_PACKET_OK( c,e,l ) if( ide_test_packet_ok( __FILE__, __LINE__, __func__, c, e, l ) != 0 ) { return -1; }
int ide_test_packet_ok( const char *file, int line, const char *func, 
			const char *cmd, char *expect, int expect_len ) 
{
    char buf[expect_len];
    int status = send_packet_command(cmd);
    if( status != 0 ) {
	return status;
    }
    status = byte_read( IDE_ALTSTATUS );
    if( status & 1 ) { /* Error */
	status = ide_get_sense_code();
	fprintf( stderr, "Assertion failed at %s:%d %s(): Unexpected error %04X\n",
		 file, line, func, status );
	return -1;
    }

    status = read_pio( buf, expect_len );
    if( status != 0 ) {
	return status;
    }
    if( expect != NULL && memcmp( expect, buf, expect_len ) != 0 ) {
	fprintf(stderr, "Assertion failed at %s:%d %s(): Results differ from expected:\n",file,line,func );
	fwrite_diff( stderr, expect, expect_len, buf, expect_len );
	return -1;
    }
    return 0;
}

#define IDE_TEST_PACKET_ERROR( c,e ) if( ide_test_packet_error( __FILE__, __LINE__, __func__, c, e ) != 0 ) { return -1; }
int ide_test_packet_error( char *file, int line, char *func,
			   char *cmd, int expect_error )
{
    uint32_t error_regs[] = 
    { IDE_ALTSTATUS, 0x51,
      IDE_ERROR, (expect_error & 0x0F)<<4,
      IDE_COUNT, 0x03,
      IDE_DEVICE, 0,
      IDE_STATUS, 0x51, 0, 0 };
    uint32_t error_code;
    int status = send_packet_command(cmd);
    if( status != 0 ) {
	return status;
    }
    CHECK_REGS(error_regs);
    error_code = ide_get_sense_code();
    CHECK_IEQUALS( expect_error, error_code );

    return 0;
}
    

uint32_t abort_regs[] = {
    IDE_ALTSTATUS, 0x51,
    IDE_ERROR, 0x04,
    IDE_COUNT, 0x02,
    IDE_LBA1, 0x00,
    IDE_LBA2, 0x50,
    IDE_DEVICE, 0,
    IDE_DATA, 0x0000,
    IDE_STATUS, 0x51, 
    0, 0 };

uint32_t post_reset_regs[] = {
    IDE_ALTSTATUS, 0x00,
    IDE_ERROR, 0x01,
    IDE_COUNT, 0x01,
    IDE_LBA1, 0x14,
    IDE_LBA2, 0xEB,
    IDE_DEVICE, 0,
    IDE_DATA, 0xFFFF,
    IDE_STATUS, 0x00, 
    0, 0 };

uint32_t post_set_feature_regs[] = {
    IDE_ALTSTATUS, 0x50,
    IDE_ERROR, 0x00,
    IDE_COUNT, 0x0B,
    IDE_LBA1, 0x00,
    IDE_LBA2, 0x00,
    IDE_DEVICE, 0,
    IDE_DATA, 0xFFFF,
    IDE_STATUS, 0x50, 
    0, 0 };    

uint32_t post_set_feature2_regs[] = {
    IDE_ALTSTATUS, 0x50,
    IDE_ERROR, 0x00,
    IDE_COUNT, 0x22,
    IDE_LBA1, 0x00,
    IDE_LBA2, 0x00,
    IDE_DEVICE, 0,
    IDE_DATA, 0xFFFF,
    IDE_STATUS, 0x50, 
    0, 0 };    

/************************** Interface Tests *******************************/

/**
 * Test enable/disable of the IDE interface via port
 * 0x4E4. 
 */
int test_enable()
{
    int i;
    int failed = 0;
    /* ensure deactivated */
    long_write( IDE_ACTIVATE, 0x00042FE );

    /* test registers to ensure all return 0xFF (need to wait a few cycles?) */
    for( i= IDE_BASE; i< IDE_BASE+0x400; i+= 4 ) {
	CHECK_REG_EQUALS( i, 0xFFFFFFFF, long_read( i ) );
    }

    /* enable interface */
    ide_activate();

    /* test registers have default settings */
    //    CHECK_REGS( post_reset_regs );
    

    /* disable interface and re-test */
    long_write( IDE_ACTIVATE, 0x00042FE );

    /* Test registers all 0xFF */
    for( i= IDE_BASE; i< IDE_BASE+0x400; i+= 4 ) {
	CHECK_REG_EQUALS( i, 0xFFFFFFFF, long_read( i ) );
    }

    /* Finally leave the interface in an enabled state */
    ide_activate();
    return 0;
}


uint32_t drive_ready_regs[] = {
    IDE_ALTSTATUS, 0x50,
    IDE_ERROR, 0x00,
    IDE_COUNT, 0x03,
    IDE_LBA1, 0x08,
    IDE_LBA2, 0x00,
    IDE_DEVICE, 0,
    IDE_DATA, 0xFFFF,
    IDE_STATUS, 0x50, 
    0, 0 };    

/**
 * Test the reset command
 */
int test_reset()
{
    byte_write( IDE_COMMAND, 0x08 );
    EXPECT_READY();
    CHECK_INTRQ_CLEAR();
    CHECK_REGS( post_reset_regs );
    
    /** Set Default PIO mode */
    byte_write( IDE_FEATURE, 0x03 );
    byte_write( IDE_COUNT, 0x0B );
    byte_write( IDE_COMMAND, 0xEF );
    EXPECT_READY();
    CHECK_INTRQ_CLEAR();
    CHECK_REGS( post_set_feature_regs );
    
    /** Set Multi-word DMA mode 2 */
    long_write( 0xA05F7490, 0x222 );
    long_write( 0xA05F7494, 0x222 );
    byte_write( IDE_FEATURE, 0x03 );
    byte_write( IDE_COUNT, 0x22 );
    byte_write( IDE_COMMAND, 0xEF );
    EXPECT_READY();
    CHECK_INTRQ_CLEAR();
    CHECK_REGS( post_set_feature2_regs );

    char test_ready_cmd[12] = { 0,0,0,0, 0,0,0,0, 0,0,0,0 };
    if( send_packet_command(test_ready_cmd) != 0 ) {
	return -1;
    }

    CHECK_REGS( packet_cmd_error6_regs );
    int sense = ide_get_sense_code();
    CHECK_IEQUALS( 0x2906, sense );

    if( send_packet_command(test_ready_cmd) != 0 ) {
	return -1;
    }
    CHECK_REGS( drive_ready_regs );
    return 0;
}

char expect_ident[] = { 0x00, 0xb4, 0x19, 0x00,
			0x00, 0x08, 0x53, 0x45, 0x20, 0x20, 0x20, 0x20 };

/**
 * Test the PACKET command (using the Inquiry command)
 */
int test_packet()
{
    int i;
    char cmd[12] = { 0x11, 0, 4, 0,  12, 0, 0, 0,  0, 0, 0, 0 };
    // char cmd[12] = { 0x00,0,0,0, 0,0,0,0, 0,0,0,0 };
    unsigned short *spkt;
    char result[12];

    send_packet_command( cmd );
    CHECK_REGS( packet_data_ready_regs );
    spkt = (unsigned short *)result;
    *spkt++ = word_read(IDE_DATA);
    *spkt++ = word_read(IDE_DATA);
    *spkt++ = word_read(IDE_DATA);
    *spkt++ = word_read(IDE_DATA);
    CHECK_REGS( packet_data_ready_regs );
    *spkt++ = word_read(IDE_DATA);
    *spkt++ = word_read(IDE_DATA);
//    CHECK_REGS( post_packet_data_regs );
    EXPECT_READY();
    EXPECT_INTRQ();
    CHECK_REGS( packet_complete_regs );

    if( memcmp( result, expect_ident, 12 ) != 0 ) {
	fwrite_diff( stderr, expect_ident, 12, result, 12 );
	return -1;
    }
    return 0;
}

/**
 * Test the SET FEATURE command
 */
int test_set_feature()
{
    return 0;
}

/**
 * Test DMA transfer (using the Inquiry packet comand)
 */
int test_dma()
{
    return 0;
}

/**
 * Test DMA abort
 */
int test_dma_abort()
{
    return 0;
}

/***************************** GD-Rom Tests **********************************/

int test_read_toc()
{
    char cmd[12] = { 0x14,0,0,0x00, 0x0C,0,0,0, 0,0,0,0 };
    char expect[12] = { 0x41, 0,0, 0x96, 0x41, 0, 0x2E, 0x4C, 0xFF, 0xFF, 0xFF, 0xFF };
    
    IDE_TEST_PACKET_OK( cmd, expect, 12 );
    return 0;
}

/**
 * Test interaction of Read CD (0x30) with Status (0x40,1)
 */
int test_status1() 
{
    char cmd[12] = { 0x40, 0x01, 0, 0, 16,0,0,0, 0,0,0,0 };
    char read1cmd[12] = { 0x30, 0x28, 0, 0x2E, 0x4C, 0, 0, 0, 0, 0,1,0 };
    char expect1[16] = { 0,0x15,0,0x0E, 0x41,2,1,0, 0,1,0,0, 0x2E,0x4D,0,0 };
    char read2cmd[12] = { 0x30, 0x28, 0, 0x2E, 0x4D, 0, 0, 0, 0, 0,1,0 };
    char expect2[16] = { 0,0x15,0,0x0E, 0x41,2,1,0, 0,4,0,0, 0x2E,0x50,0,0 };
    char read3cmd[12] = { 0x30, 0x28, 0, 0x2E, 0x4E, 0, 0, 0, 0, 0,1,0 };
    char expect3[16] = { 0,0x15,0,0x0E, 0x41,2,1,0, 0,5,0,0, 0x2E,0x51,0,0 };
    char expect4[16] = { 0,0x15,0,0x0E, 0x41,2,1,0, 0,2,0,0, 0x2E,0x4E,0,0 };
    char read5cmd[12] = { 0x30, 0x28, 0, 0x2F, 0x01, 0, 0, 0, 0, 0,1,0 };
    char expect5[16] = { 0,0x15,0,0x0E, 0x41,2,1,0, 0,0xB6,0,0, 0x2F,0x02,0,0 };
    char read6cmd[12] = { 0x30, 0x28, 0, 0x2F, 0x50, 0, 0, 0, 0, 0,1,0 };
    char expect6[16] = { 0,0x15,0,0x0E, 0x41,2,1,0, 0x01,0x05,0,0, 0x2F,0x51,0,0 };
    char read7cmd[12] = { 0x30, 0x28, 0, 0x2F, 0x51, 0, 0, 0, 0, 0,1,0 };
    char expect7[16] = { 0,0x15,0,0x0E, 0x41,2,1,0, 0x01,0x06,0,0, 0x2F,0x52,0,0 };
    

    IDE_TEST_PACKET_OK(read1cmd, NULL, 2048);
    IDE_TEST_PACKET_OK(cmd, expect1, 14 );
    IDE_TEST_PACKET_OK(read2cmd, NULL, 2048);
    IDE_TEST_PACKET_OK(cmd, expect2, 14 );
    IDE_TEST_PACKET_OK(read3cmd, NULL, 2048);
    IDE_TEST_PACKET_OK(cmd, expect3, 14 );
    IDE_TEST_PACKET_OK(read2cmd, NULL, 2048);
    IDE_TEST_PACKET_OK(cmd, expect4, 14 );
    IDE_TEST_PACKET_OK(read5cmd, NULL, 2048);
    IDE_TEST_PACKET_OK(cmd, expect5, 14 );
    IDE_TEST_PACKET_OK(read6cmd, NULL, 2048);
    IDE_TEST_PACKET_OK(cmd, expect6, 14 );

    return 0;
}

/********************************* Main **************************************/

typedef int (*test_func_t)();

test_func_t test_fns[] = { test_enable, test_reset, test_packet,
			   test_dma, test_dma_abort, 
			   test_read_toc,
			   test_status1, NULL };

int main() 
{
    int i;
    ide_init();

    /* run tests */
    for( i=0; test_fns[i] != NULL; i++ ) {
	test_count++;
	if( test_fns[i]() != 0 ) {
	    fprintf( stderr, "Test %d failed\n", i+1 );
	    test_failures++;
	}
    }

    /* report */
    fprintf( stderr, "%d/%d tests passed!\n", test_count - test_failures, test_count );
    return test_failures;
}
