/**
 * $Id$
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
    { 0xA05F6800, 0xFFFFFFFF, 0x13FFFFE0 },
    { 0xA05F6800, 0x00000000, 0x10000000 },
    { 0xA05F6804, 0xFFFFFFFF, 0x00FFFFE0 },
//    { 0xA05F6808, 0xFFFFFFFF, 0x00000001 },
//    { 0xA05F6808, 0x00000000, 0x00000000 },  // DMA start
//    { 0xA05F680C, 0xFFFFFFFF, 0x00000000 },  // Not a register afaik
//    { 0xA05F7400, 0xFFFFFFFF, 0x00000000 },  // Not a register
    { 0xA05F7404, 0xFFFFFFFF, 0x1FFFFFE0 },
    { 0xA05F7404, 0x00000000, 0x00000000 },
    { 0xA05F7408, 0xFFFFFFFF, 0x01FFFFFE },
    { 0xA05F740C, 0xFFFFFFFF, 0x00000001 },
//    { 0xA05F7410, 0xFFFFFFFF, 0x00000000 },  // Not a register
    { 0xA05F7414, 0xFFFFFFFF, 0x00000001 },
//    { 0xA05F7418, 0xFFFFFFFF, 0x00000001 },  // DMA start
//    { 0xA05F741C, 0xFFFFFFFF, 0x00000000 },  // Not a register
    { 0xA05F7800, 0xFFFFFFFF, 0x9FFFFFE0 },
    { 0xA05F7800, 0x00000000, 0x00000000 },
    { 0xA05F7804, 0xFFFFFFFF, 0x9FFFFFE0 },
    { 0xA05F7808, 0xFFFFFFFF, 0x9FFFFFE0 },
    { 0xA05F780C, 0xFFFFFFFF, 0x00000001 }, 
    { 0xA05F7810, 0xFFFFFFFF, 0x00000007 },
    { 0xA05F7814, 0xFFFFFFFF, 0x00000001 },
//    { 0xA05F7818, 0xFFFFFFFF, 0x00000000 },  // DMA start
    { 0xA05F781C, 0xFFFFFFFF, 0x00000037 },
    { 0xA05F8000, 0xFFFFFFFF, 0x17FD11DB }, /* PVRID read-only */
    { 0xA05F8004, 0xFFFFFFFF, 0x00000011 }, /* PVRVER read-only */
    { 0xA05F8008, 0xFFFFFFFF, 0x00000007 }, /* Reset */
    { 0xA05F8010, 0xFFFFFFFF, 0 },
    //    { 0xA05F8014, 0xFFFFFFFF, 0x00000000 }, /* Render start */
    { 0xA05F8018, 0xFFFFFFFF, 0x000007FF }, /* ??? */   
    { 0xA05F801C, 0xFFFFFFFF, 0x00000000 }, /* ??? */   
    { 0xA05F8020, 0xFFFFFFFF, 0x00F00000 }, /* Render poly buffer address ??? */
    { 0xA05F8024, 0xFFFFFFFF, 0x00000000 }, /* ??? */
    { 0xA05F8028, 0xFFFFFFFF, 0x00000000 }, /* ??? */   
    { 0xA05F802C, 0xFFFFFFFF, 0x00FFFFFC }, /* Render Tile buffer address */
    { 0xA05F8030, 0xFFFFFFFF, 0x00010101 }, /* Render TSP cache? */   
    { 0xA05F8034, 0xFFFFFFFF, 0 },
    { 0xA05F8038, 0xFFFFFFFF, 0 },
    { 0xA05F803C, 0xFFFFFFFF, 0 },
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
    { 0xA05F8070, 0xFFFFFFFF, 0 },
    { 0xA05F8074, 0xFFFFFFFF, 0x000001FF }, /* Render shadow mode */
    { 0xA05F8078, 0xFFFFFFFF, 0x7FFFFFFF }, /* Near z clip */
    { 0xA05F807C, 0xFFFFFFFF, 0x003FFFFF }, /* Render object config */
    { 0xA05F8080, 0xFFFFFFFF, 0x00000007 }, /* ??? */
    { 0xA05F8084, 0xFFFFFFFF, 0x7FFFFFFF }, /* Render tsp clip */
    { 0xA05F8088, 0xFFFFFFFF, 0xFFFFFFF0 }, /* Far z clip */
    { 0xA05F808C, 0xFFFFFFFF, 0x1FFFFFFF }, /* Render background plane config */
    { 0xA05F8090, 0xFFFFFFFF, 0 },
    { 0xA05F8094, 0xFFFFFFFF, 0 },
    { 0xA05F8098, 0xFFFFFFFF, 0x00FFFFF9 }, /* ISP config? */
    { 0xA05F809C, 0xFFFFFFFF, 0 },
    { 0xA05F80A0, 0xFFFFFFFF, 0x000000FF }, /* Vram cfg1? */
    { 0xA05F80A4, 0xFFFFFFFF, 0x003FFFFF },
    { 0xA05F80A8, 0xFFFFFFFF, 0x1FFFFFFF },
    { 0xA05F80AC, 0xFFFFFFFF, 0 },
    { 0xA05F80B0, 0xFFFFFFFF, 0x00FFFFFF },
    { 0xA05F80B4, 0xFFFFFFFF, 0x00FFFFFF },
    { 0xA05F80B8, 0xFFFFFFFF, 0x0000FFFF },
    { 0xA05F80BC, 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xA05F80C0, 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xA05F80C4, 0xFFFFFFFF, UNCHANGED },  /* Gun pos */
    { 0xA05F80C8, 0xFFFFFFFF, 0x03FF33FF }, /* Horizontal scanline irq */
    { 0xA05F80CC, 0xFFFFFFFF, 0x03FF03FF }, /* Vertical scanline irq */
    { 0xA05F80D0, 0xFFFFFFFF, 0x000003FF },
    { 0xA05F80D4, 0xFFFFFFFF, 0x03FF03FF },
    { 0xA05F80D8, 0xFFFFFFFF, 0x03FF03FF },
    { 0xA05F80DC, 0xFFFFFFFF, 0x03FF03FF },
    { 0xA05F80E0, 0xFFFFFFFF, 0xFFFFFF7F },
    { 0xA05F80E4, 0xFFFFFFFF, 0x00031F1F },
    { 0xA05F80E8, 0xFFFFFFFF, 0x003F01FF },
    { 0xA05F80EC, 0xFFFFFFFF, 0x000003FF },
    { 0xA05F80F0, 0xFFFFFFFF, 0x03FF03FF },
    { 0xA05F80F4, 0xFFFFFFFF, 0x0007FFFF },
    { 0xA05F80F8, 0xFFFFFFFF, 0 },
    { 0xA05F80FC, 0xFFFFFFFF, 0 },
    { 0xA05F8100, 0xFFFFFFFF, 0 },
    { 0xA05F8104, 0xFFFFFFFF, 0 },
    { 0xA05F8108, 0xFFFFFFFF, 0x00000003 },
    { 0xA05F810C, 0xFFFFFFFF, UNCHANGED },
    { 0xA05F8110, 0xFFFFFFFF, 0x000FFF3F },
    { 0xA05F8114, 0xFFFFFFFF, UNCHANGED },
    { 0xA05F8118, 0xFFFFFFFF, 0x0000FFFF },
    { 0xA05F811C, 0xFFFFFFFF, 0x000000FF },
    { 0xA05F8120, 0xFFFFFFFF, 0 },
    { 0xA05F8124, 0xFFFFFFFF, 0x00FFFFE0 }, /* TA Tile matrix base */
    { 0xA05F8128, 0xFFFFFFFF, 0x00FFFFFC }, /* TA Polygon base */
    { 0xA05F812C, 0xFFFFFFFF, 0x00FFFFE0 }, /* TA Tile matrix end */
    { 0xA05F8130, 0xFFFFFFFF, 0x00FFFFFC }, /* TA Polygon end */
    { 0xA05F8134, 0xFFFFFFFF, UNCHANGED  }, /* TA Tilelist posn */
    { 0xA05F8138, 0xFFFFFFFF, UNCHANGED  }, /* TA polygon posn */
    { 0xA05F813C, 0xFFFFFFFF, 0x000F003F }, /* TA tile matrix size */
    { 0xA05F8140, 0xFFFFFFFF, 0x00133333 }, /* TA object config */
    { 0xA05F8144, 0xFFFFFFFF, 0x00000000 }, /* TA initialize */
    { 0xA05F8148, 0xFFFFFFFF, 0x00FFFFF8 },
    { 0xA05F814C, 0xFFFFFFFF, 0x01013F3F },
    { 0xA05F8150, 0xFFFFFFFF, 0 },
    { 0xA05F8154, 0xFFFFFFFF, 0 },
    { 0xA05F8158, 0xFFFFFFFF, 0 },
    { 0xA05F815C, 0xFFFFFFFF, 0 },
    { 0xA05F8160, 0xFFFFFFFF, 0 },
    { 0xA05F8164, 0xFFFFFFFF, 0x00FFFFE0 }, /* TA Tile list start */
    { 0xA05F8168, 0xFFFFFFFF, 0 },
    { 0xA05F816C, 0xFFFFFFFF, 0 },
    { 0xA05F8170, 0xFFFFFFFF, 0 },
    { 0xA05F8174, 0xFFFFFFFF, 0 },
    { 0xA05F8178, 0xFFFFFFFF, 0 },
    { 0xA05F817C, 0xFFFFFFFF, 0 },
    { 0xA05F8180, 0xFFFFFFFF, 0 },
    { 0xA05F8184, 0xFFFFFFFF, 0 },
    { 0xA05F8188, 0xFFFFFFFF, 0 },
    { 0xA05F818C, 0xFFFFFFFF, 0 },
    { 0xA05F8190, 0xFFFFFFFF, 0 },
    { 0xA05F8194, 0xFFFFFFFF, 0 },
    { 0xA05F8198, 0xFFFFFFFF, 0 },
    { 0xA05F819C, 0xFFFFFFFF, 0 },
    { 0xA05F81A0, 0xFFFFFFFF, 0 },
    { 0xA05F81A4, 0xFFFFFFFF, 0 },
    { 0xA05F81A8, 0xFFFFFFFF, 0x00000001 },
    { 0xA05F81A8, 0x00000000, 0x00000000 },    	
    { 0xA05F81AC, 0xFFFFFFFF, 0 },
    { 0xA05F81B0, 0xFFFFFFFF, 0 },
    { 0xA05F81B4, 0xFFFFFFFF, 0 },
    { 0xA05F81B8, 0xFFFFFFFF, 0 },
    { 0xA05F81BC, 0xFFFFFFFF, 0 },
    { 0xA05F81C0, 0xFFFFFFFF, 0 },
    { 0xA05F81C4, 0xFFFFFFFF, 0 },
    { 0xA05F81C8, 0xFFFFFFFF, 0 },
    { 0xA05F81CC, 0xFFFFFFFF, 0 },
    { 0xA05F81D0, 0xFFFFFFFF, 0 },
    { 0xA05F81D4, 0xFFFFFFFF, 0 },
    { 0xA05F81D8, 0xFFFFFFFF, 0 },
    { 0xA05F81DC, 0xFFFFFFFF, 0 },
    { 0xA05F81E0, 0xFFFFFFFF, 0 },
    { 0xA05F81E4, 0xFFFFFFFF, 0 },
    { 0xA05F81E8, 0xFFFFFFFF, 0 },
    { 0xA05F81EC, 0xFFFFFFFF, 0 },
    { 0xA05F81F0, 0xFFFFFFFF, 0 },
    { 0xA05F81F4, 0xFFFFFFFF, 0 },
    { 0xA05F81F8, 0xFFFFFFFF, 0 },
    { 0xA05F81FC, 0xFFFFFFFF, 0 },
    { 0, 0, 0 } };
    
int main( int argc, char *argv[] )
{
    int i;
    int failures = 0;
    int tests = 0;

    ide_init();
    
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
