/**
 * $Id: testregs.c,v 1.1 2006-08-02 04:13:15 nkeynes Exp $
 * 
 * Register mask tests. These are simple "write value to register and check
 * that we read back what we expect" tests.
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
#include <stdio.h>

/**
 * Constant to mean "same as previous value". Can't be used otherwise.
 */
#define UNCHANGED 0xDEADBEEF

struct test {
    unsigned int reg;
    unsigned int write;
    unsigned int expect;
};



struct test test_cases[] = {
    { 0xA05F8000, 0xFFFFFFFF, 0x17FD11DB }, /* PVRID read-only */
    { 0xA05F8004, 0xFFFFFFFF, 0x00000011 }, /* PVRVER read-only */
    //    { 0xA05F8014, 0xFFFFFFFF, 0x00000000 }, /* Render start */
    { 0xA05F8018, 0xFFFFFFFF, 0x000007FF }, /* ??? */   
    { 0xA05F801C, 0xFFFFFFFF, 0x00000000 }, /* ??? */   
    { 0xA05F8020, 0xFFFFFFFF, 0x00F00000 }, /* Render poly buffer address ??? */
    { 0xA05F8024, 0xFFFFFFFF, 0x00000000 }, /* ??? */
    { 0xA05F8028, 0xFFFFFFFF, 0x00000000 }, /* ??? */   
    { 0xA05F802C, 0xFFFFFFFF, 0x00FFFFFC }, /* Render Tile buffer address */
    { 0xA05F8030, 0xFFFFFFFF, 0x00010101 }, /* Render TSP cache? */   
    { 0xA05F8040, 0xFFFFFFFF, 0x01FFFFFF }, /* Display border colour */   
    { 0xA05F8044, 0xFFFFFFFF, 0x00FFFF7F }, /* Display config */   
    { 0xA05F8048, 0xFFFFFFFF, 0x00FFFF0F }, /* Render config */
    { 0xA05F804C, 0xFFFFFFFF, 0x000001FF }, /* Render size */
    { 0xA05F8050, 0xFFFFFFFF, 0x00FFFFFC }, /* Display address 1 */ 
    { 0xA05F8054, 0xFFFFFFFF, 0x00FFFFFC }, /* Display address 2 */
    { 0xA05F8058, 0xFFFFFFFF, 0x00000000 }, /* ??? */
    { 0xA05F805C, 0xFFFFFFFF, 0x3FFFFFFF }, /* Display size */
    { 0xA05F8060, 0xFFFFFFFF, 0x01FFFFFC }, /* Render address 1 */
    { 0xA05F8064, 0xFFFFFFFF, 0x01FFFFFC }, /* Render address 2 */
    { 0xA05F8068, 0xFFFFFFFF, 0x07FF07FF }, /* Render horizontal clip */
    { 0xA05F806C, 0xFFFFFFFF, 0x03FF03FF }, /* Render vertical clip */
    { 0xA05F8074, 0xFFFFFFFF, 0x000001FF }, /* Render shadow mode */
    { 0xA05F807C, 0xFFFFFFFF, 0x003FFFFF }, /* Render object config */
    { 0xA05F8084, 0xFFFFFFFF, 0x7FFFFFFF }, /* Render tsp clip */
    { 0xA05F808C, 0xFFFFFFFF, 0x1FFFFFFF }, /* Render background plane config */
    { 0xA05F8098, 0xFFFFFFFF, 0x00FFFFF9 }, /* ISP config? */
    { 0xA05F80C4, 0xFFFFFFFF, UNCHANGED },  /* Gun pos */
    { 0xA05F80C8, 0xFFFFFFFF, 0x03FF33FF }, /* Horizontal scanline irq */
    { 0xA05F80CC, 0xFFFFFFFF, 0x03FF03FF }, /* Vertical scanline irq */
    { 0xA05F8124, 0xFFFFFFFF, 0x00FFFFE0 }, /* TA Tile matrix base */
    { 0xA05F8128, 0xFFFFFFFF, 0x00FFFFFC }, /* TA Polygon base */
    { 0xA05F812C, 0xFFFFFFFF, 0x00FFFFE0 }, /* TA Tile matrix end */
    { 0xA05F8130, 0xFFFFFFFF, 0x00FFFFFC }, /* TA Polygon end */
    { 0xA05F8134, 0xFFFFFFFF, UNCHANGED  }, /* TA Tilelist posn */
    { 0xA05F8138, 0xFFFFFFFF, UNCHANGED  }, /* TA polygon posn */
    { 0xA05F813C, 0xFFFFFFFF, 0x000F003F }, /* TA tile matrix size */
    { 0xA05F8140, 0xFFFFFFFF, 0x00133333 }, /* TA object config */
    { 0xA05F8144, 0xFFFFFFFF, 0x00000000 }, /* TA initialize */
    { 0xA05F8164, 0xFFFFFFFF, 0x00FFFFE0 }, /* TA Tile list start */
    { 0, 0, 0 } };
    
int main( int argc, char *argv[] )
{
    int i;
    int failures = 0;
    int tests = 0;
    
    for( i=0; test_cases[i].reg != 0; i++ ) {
    	unsigned int oldval = long_read( test_cases[i].reg );
    	unsigned int newval;
	long_write( test_cases[i].reg, test_cases[i].write );
	newval = long_read( test_cases[i].reg );
	if( test_cases[i].expect == UNCHANGED ) {
	    if( newval != oldval ) {
		fprintf( stderr, "Test %d (%08X) failed. Expected %08X but was %08X\n",
		  	 i+1, test_cases[i].reg, oldval, newval );
		failures++;
	    }
	} else {
	    if( newval != test_cases[i].expect ) {
		fprintf( stderr, "Test %d (%08X) failed. Expected %08X but was %08X\n",
		  	 i+1, test_cases[i].reg, test_cases[i].expect, newval );
		failures++;
	    }
	}
	long_write( test_cases[i].reg, oldval );
	tests++;
    }

    fprintf( stdout, "%d/%d test cases passed successfully\n", (tests-failures), tests );
    return failures;
}
