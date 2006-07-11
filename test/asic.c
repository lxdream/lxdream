/**
 * $Id: asic.c,v 1.1 2006-07-11 01:35:23 nkeynes Exp $
 * 
 * General ASIC support code
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

#include "lib.h"

#define ASIC_BASE 0xA05F6000
#define ASIC_PIRQ(n) (ASIC_BASE + 0x900 + (n<<2))
#define ASIC_IRQA(n) (ASIC_BASE + 0x910 + (n<<2))
#define ASIC_IRQB(n) (ASIC_BASE + 0x920 + (n<<2))
#define ASIC_IRQC(n) (ASIC_BASE + 0x930 + (n<<2))
#define TIMEOUT 10000000

/**
 * Wait for an ASIC event. 
 * @return 0 if the event occurred, otherwise -1 if the wait timed out.
 */
int asic_wait( int event )
{
    int n = event >> 5;
    unsigned int mask = (1<< (event&0x1f));
    int i;
    for( i=0; i<TIMEOUT; i++ ) {
	if( long_read(ASIC_PIRQ(n)) & mask ) {
	    return 0;
	}
    }
    return -1; /* Timeout */
}

/**
 * Clear all asic events
 */
void asic_clear()
{
    long_write(ASIC_PIRQ(0), 0xFFFFFFFF);
    long_write(ASIC_PIRQ(1), 0xFFFFFFFF);
    long_write(ASIC_PIRQ(2), 0xFFFFFFFF);
}

void asic_mask_all()
{
    long_write(ASIC_IRQA(0), 0);
    long_write(ASIC_IRQA(1), 0);
    long_write(ASIC_IRQA(2), 0);
    long_write(ASIC_IRQB(0), 0);
    long_write(ASIC_IRQB(1), 0);
    long_write(ASIC_IRQB(2), 0);
    long_write(ASIC_IRQC(0), 0);
    long_write(ASIC_IRQC(1), 0);
    long_write(ASIC_IRQC(2), 0);
}

/**
 * Print the contents of the ASIC event registers to the supplied FILE
 */
void asic_dump( FILE *f )
{
    int i,j;
    fprintf( f, "Events: " );
    for( i=0; i<3; i++ ) {
	uint32_t val = long_read(ASIC_PIRQ(i));
	for( j=0; j<32; j++ ) {
	    if( val & (1<<j) ) {
		fprintf( f, "%d ", (i<<5)+j );
	    }
	}
    }
    fprintf( f, "\n" );
}
