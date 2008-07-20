/**
 * $Id$
 * 
 * DMA support code
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

#include "dma.h"
#include "asic.h"

#define DMA_BASE 0xFFA00000

#define DMA_SAR(c) (DMA_BASE+0x00+(c<<4))
#define DMA_DAR(c) (DMA_BASE+0x04+(c<<4))
#define DMA_TCR(c) (DMA_BASE+0x08+(c<<4))
#define DMA_CHCR(c) (DMA_BASE+0x0C+(c<<4))
#define DMA_OR (DMA_BASE+0x40)

#define ASIC_BASE 0xA05F6000
#define PVR_DMA_DEST  (ASIC_BASE+0x800)
#define PVR_DMA_COUNT (ASIC_BASE+0x804)
#define PVR_DMA_CTL   (ASIC_BASE+0x808)
#define PVR_DMA_REGION (ASIC_BASE+0x884)

#define SORT_DMA_TABLE (ASIC_BASE+0x810)
#define SORT_DMA_DATA  (ASIC_BASE+0x814)
#define SORT_DMA_TABLEBITS (ASIC_BASE+0x818)
#define SORT_DMA_DATASIZE (ASIC_BASE+0x81C)
#define SORT_DMA_CTL   (ASIC_BASE+0x820)
#define SORT_DMA_COUNT (ASIC_BASE+0x860)

void dmac_dump_channel( FILE *f, unsigned int channel )
{
    fprintf( f, "DMAC SAR: %08X  Count: %08X  Ctl: %08X  OR: %08X\n",
	     long_read(DMA_SAR(channel)), long_read(DMA_TCR(channel)), 
	     long_read(DMA_CHCR(channel)), long_read(DMA_OR) );
}


/**
 * Setup the DMAC for a transfer. Assumes 32-byte block transfer.
 * Caller is responsible for making sure no-one else is using the
 * channel already. 
 *
 * @param channel DMA channel to use, 0 to 3
 * @param source source address (if a memory source)
 * @param dest   destination address (if a memory destination)
 * @param length number of bytes to transfer (must be a multiple of
 *               32.
 * @param direction 0 = host to device, 1 = device to host
 */
void dmac_prepare_channel( int channel, uint32_t source, uint32_t dest,
			   uint32_t length, int direction )
{
    uint32_t control;

    if( direction == 0 ) {
	/* DMA Disabled, IRQ disabled, 32 byte transfer, burst mode,
	 * Memory => Device, Source addr increment, dest addr fixed
	 */
	control = 0x000012C0;
    } else {
	/* DMA Disabled, IRQ disabled, 32 byte transfer, burst mode,
	 * Device => Memory, Source addr fixed, dest addr increment
	 */
	control = 0x000043C0;
    }
    long_write( DMA_CHCR(channel), control );
    long_write( DMA_SAR(channel), source );
    long_write( DMA_DAR(channel), dest );
    long_write( DMA_TCR(channel), (length >> 5) );
    control |= 0x0001;
    long_write( DMA_CHCR(channel), control ); /* Enable DMA channel */
    long_write( DMA_OR, 0x8201 ); /* Ensure the DMAC config is set */
}


int pvr_dma_write( unsigned int target, char *buf, int len, int region )
{
    uint32_t addr =(uint32_t)buf;
    int result;
    if( (addr & 0xFFFFFFE0) != addr ) {
	fprintf( stderr, "Address error: Attempting DMA from %08X\n", addr );
	return -1;
    }
    long_write( PVR_DMA_CTL, 0 ); /* Stop PVR dma if it's already running */
    asic_clear();

    dmac_prepare_channel( 2, (uint32_t)buf, 0, len, 0 ); /* Allocate channel 2 */
    long_write( PVR_DMA_DEST, target );
    long_write( PVR_DMA_COUNT, len );
    long_write( PVR_DMA_REGION, region );

    CHECK_IEQUALS( target, long_read(PVR_DMA_DEST) );
    CHECK_IEQUALS( len, long_read(PVR_DMA_COUNT) );
    CHECK_IEQUALS( 0, long_read(PVR_DMA_REGION) );
    CHECK_IEQUALS( (uint32_t)buf, long_read(DMA_SAR(2)) );
    CHECK_IEQUALS( len/32, long_read(DMA_TCR(2)) );
    CHECK_IEQUALS( 0x12C1, long_read(DMA_CHCR(2)) );

    long_write( PVR_DMA_CTL, 1 );
    result = asic_wait(EVENT_PVR_DMA);

    if( result != 0 ) {
	fprintf( stderr, "PVR DMA failed (timeout)\n" );
	asic_dump(stderr);
	fprintf( stderr, "Dest: %08X  Count: %08X  Ctl: %08X\n", long_read(PVR_DMA_DEST),
		 long_read(PVR_DMA_COUNT), long_read(PVR_DMA_CTL) );
	dmac_dump_channel(stderr, 2);
	long_write( PVR_DMA_CTL, 0 );
    }

    CHECK_IEQUALS( 0, long_read(PVR_DMA_CTL) );
    CHECK_IEQUALS( ((uint32_t)buf)+len, long_read(DMA_SAR(2))  );
    CHECK_IEQUALS( 0, long_read(DMA_TCR(2)) );
    CHECK_IEQUALS( 0x12C3, long_read(DMA_CHCR(2)) );
    CHECK_IEQUALS( target, long_read(PVR_DMA_DEST) );
    CHECK_IEQUALS( 0, long_read(PVR_DMA_COUNT) );
    CHECK_IEQUALS( 0, long_read(PVR_DMA_REGION) );

    return result;
}

int sort_dma_write( char *sorttable, int tablelen, char *data, int datalen, int bitwidth, int datasize )
{
    int result;
    uint32_t tableaddr = (uint32_t)sorttable;
    uint32_t dataaddr = (uint32_t)data;
    
    long_write( SORT_DMA_CTL, 0 );
    asic_clear();
    
    long_write( SORT_DMA_TABLE, tableaddr );
    long_write( SORT_DMA_DATA, dataaddr );
    long_write( SORT_DMA_TABLEBITS, bitwidth );
    long_write( SORT_DMA_DATASIZE, datasize );
    long_write( SORT_DMA_CTL, 1 );
    result = asic_wait2(EVENT_SORT_DMA, EVENT_SORT_DMA_ERR);
    if( result == -1 ) {
        fprintf( stderr, "SORT DMA failed (timeout)\n" );
        asic_dump(stderr);
        fprintf( stderr, "Table: %08X Count: %08X Ctl: %08X\n", long_read(SORT_DMA_TABLE), long_read(SORT_DMA_COUNT),
                 long_read(SORT_DMA_CTL) );
        long_write( SORT_DMA_CTL, 0 );
    }
    CHECK_IEQUALS( 0, long_read(SORT_DMA_CTL) );
    return result;
}
