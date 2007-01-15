/**
 * $Id: testyuv.c,v 1.1 2007-01-15 08:30:50 nkeynes Exp $
 * 
 * Renderer test cases
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
#include <stdio.h>
#include "timer.h"
#include "testdata.h"
#include "pvr.h"
#include "lib.h"
#include "asic.h"

#define PVR_BASE 0xA05F8000
#define PVR_RESET (PVR_BASE+0x008)
#define YUV_ADDR (PVR_BASE+0x148)
#define YUV_CONFIG (PVR_BASE+0x14C)
#define YUV_STATUS (PVR_BASE+0x150)
#define DISPLAY_MODE    (PVR_BASE+0x044)
#define DISPLAY_ADDR1   (PVR_BASE+0x050)
#define DISPLAY_ADDR2   (PVR_BASE+0x054)
#define DISPLAY_SIZE    (PVR_BASE+0x05C)

struct yuv_poly {
    uint32_t poly[8];
    struct {
	uint32_t mode;
	float x,y,z,u,v;
	uint32_t colour;
	uint32_t pad;
    } vertex[4];
    uint32_t end[8];
} test_yuv_poly = { { 0x8084000A, 0xE0000000, 0x2083242D, 0, 0, 0, 0, 0 },
		    { { 0xE0000000, 0.0, 0.0, 0.2, 0.0, 0.0, 0xFFFFFFFF, 0 },
		      { 0xE0000000, 640.0, 0.0, 0.2, 0.625, 0.0, 0xFFFFFFFF, 0 },
		      { 0xE0000000, 0.0, 480.0, 0.2, 0.0, 0.9375, 0xFFFFFFFF, 0 },
		      { 0xF0000000, 640.0, 480.0, 0.2, 0.625, 0.9375, 0xFFFFFFFF, 0 } },
		    { 0,0,0,0,0,0,0,0 } };

int test_yuv( test_data_t test_case )
{
    int i;
    char tabuf[512];
    char *p = DMA_ALIGN(&tabuf);

    /* Check input */
    test_data_block_t input = get_test_data(test_case, "input");
    if( input == NULL ) {
	fprintf( stderr, "Skipping test '%s' - no input\n", test_case->test_name );
	return -1;
    }

    memset( (void *)(0xA5000000), 0xFE, 512000 );

    pvr_init();
    asic_clear();

    fprintf( stdout, "Writing %d bytes\n", input->length );
    long_write( YUV_CONFIG, 0x00001D14 );
    long_write( YUV_ADDR, 0x00000000 );
    timer_start();
    uint32_t status1 = long_read( YUV_STATUS );
    if( pvr_dma_write( 0x10800000, input->data, input->length, 0 ) != 0 ) {
	return -1; 
    }
    uint32_t timeus = timer_gettime_us();
    uint32_t status2 = long_read( YUV_STATUS );

    /* Render the thing */
    memcpy( p, &test_yuv_poly, sizeof(test_yuv_poly) );
    if( pvr_dma_write( 0x10000000, p, sizeof(test_yuv_poly) ) != 0 ) {
	return -1;
    }

    return 0;
}

int main( int argc, char *argv[] ) 
{
    int test_cases = 0;
    int test_failures = 0;
    test_data_t test_data = load_test_dataset(stdin);
    test_data_t test_case = test_data;

    asic_mask_all();
    pvr_init();

    while( test_case != NULL ) {
	test_cases++;
	int result = test_yuv(test_case);
	if( result != 0 ) {
	    test_failures++;
	}
	test_case = test_case->next;
    }
    free_test_dataset(test_data);
    if( test_failures != 0 ) {
	fprintf( stderr, "%d/%d test failures!\n", test_failures, test_cases );
	return 1;
    } else {
	fprintf( stderr, "%d tests OK\n", test_cases );
	return 0;
    }
}
