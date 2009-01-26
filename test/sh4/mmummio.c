#include <assert.h>
#include <stdio.h>
#include "../lib.h"

#define PTEH  0xFF000000
#define PTEL  0xFF000004
#define TTB   0xFF000008
#define TEA   0xFF00000C
#define MMUCR 0xFF000010
#define PTEA  0xFF000034

#define ITLB_ADDR(entry) (0xF2000000 + (entry<<8))
#define ITLB_DATA(entry) (0xF3000000 + (entry<<8))
#define UTLB_ADDR(entry) (0xF6000000 + (entry<<8))
#define UTLB_DATA1(entry) (0xF7000000 + (entry<<8))
#define UTLB_DATA2(entry) (0xF7800000 + (entry<<8))

/* Bang on the mmio side of the TLBs to make sure the bits
 * respond appropriately (with AT disabled so we don't risk
 * doing a hard crash) */
void test_tlb_mmio()
{
    int entry;
    for( entry=0; entry<64; entry++ ) {
	long_write( UTLB_DATA1(entry), 0 );
	long_write( UTLB_ADDR(entry), 0xFFFFFFFF );
	assert( long_read( UTLB_ADDR(entry) ) == 0xFFFFFFFF );
	assert( long_read( UTLB_DATA1(entry) ) == 0x00000104 );
	long_write( UTLB_ADDR(entry), 0x00000000 );
	assert( long_read( UTLB_ADDR(entry) ) == 0x00000000 );
	assert( long_read( UTLB_DATA1(entry) ) == 0x00000000 );
	long_write( UTLB_DATA1(entry), 0xFFFFFFFF );
	assert( long_read( UTLB_DATA1(entry) ) == 0x1FFFFDFF );
	assert( long_read( UTLB_ADDR(entry) ) == 0x00000300 );
	long_write( UTLB_DATA1(entry), 0x00000000 );
	assert( long_read( UTLB_DATA1(entry) ) == 0x00000000 );
	assert( long_read( UTLB_ADDR(entry) ) == 0x00000000 );
	long_write( UTLB_DATA2(entry), 0xFFFFFFFF );
	assert( long_read( UTLB_DATA2(entry) ) == 0x0000000F );
	long_write( UTLB_DATA2(entry), 0x00000000 );
	assert( long_read( UTLB_DATA2(entry) ) == 0x00000000 );
    }
    
    for( entry=0; entry<4; entry++ ) {
	long_write( ITLB_DATA(entry), 0 );
	long_write( ITLB_ADDR(entry), 0xFFFFFFFF );
	assert( long_read( ITLB_ADDR(entry) ) == 0xFFFFFDFF );
	assert( long_read( ITLB_DATA(entry) ) == 0x00000100 );
	long_write( ITLB_ADDR(entry), 0x00000000 );
	assert( long_read( ITLB_ADDR(entry) ) == 0x00000000 );
	assert( long_read( ITLB_DATA(entry) ) == 0x00000000 );
	long_write( ITLB_DATA(entry), 0xFFFFFFFF );
	assert( long_read( ITLB_DATA(entry) ) == 0x1FFFFDDA );
	assert( long_read( ITLB_ADDR(entry) ) == 0x00000100 );
	long_write( ITLB_DATA(entry), 0x00000000 );
	assert( long_read( ITLB_DATA(entry) ) == 0x00000000 );
	assert( long_read( ITLB_ADDR(entry) ) == 0x00000000 );
	
    }
}

