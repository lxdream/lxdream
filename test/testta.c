/**
 * $Id: testta.c,v 1.1 2006-07-11 01:35:23 nkeynes Exp $
 * 
 * Tile Accelerator test cases 
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
#include "testdata.h"
#include "pvr.h"
#include "lib.h"
#include "asic.h"

#define DMA_ALIGN(x)   ((void *)((((unsigned int)(x))+0x1F)&0xFFFFFFE0))

#define OBJ_START 0x00010000
#define TILE_START 0x00060000

#define FLOAT(p) *((float *)(p))

int test_ta( test_data_t test_case )
{
    char buf[1024];
    unsigned int *p = DMA_ALIGN(buf);
    unsigned int *data = p;

    asic_clear();

    memset( PVR_VRAM_BASE, 0,  0x00080000 );
    ta_init(640,480, OBJ_START, 0x10000, TILE_START, 0x10000 );

    test_data_block_t input = get_test_data(test_case, "input");
    test_data_block_t output = get_test_data(test_case, "output");
    if( input == NULL || output == NULL ) {
	fprintf( stderr, "Skipping test case '%s': data incomplete\n", test_case->test_name );
	return -1;
    }

    fprintf( stderr, "Before test start: %s\n", test_case->test_name );
    if( pvr_dma_write( 0x10000000, input->data, input->length, 0 ) == -1 ) {
	return -1;
    }
    if( asic_wait( EVENT_PVR_OPAQUE_DONE ) == -1 ) {
	fprintf( stderr, "Timeout waiting for Opaque Done event\n" );
	ta_dump_regs();
	asic_dump( stderr );
    }

    char *result = (char *)(PVR_VRAM_BASE+OBJ_START);
    int result_length = pvr_get_objbuf_size();
    if( test_block_compare( output, result, result_length ) != 0 ) {
	fprintf( stderr, "Test %s: Failed. Expected %d bytes:\n", test_case->test_name, output->length );
	fwrite_dump( stderr, output->data, output->length );
	fprintf( stderr, "but was %d bytes =>\n", result_length );
	fwrite_dump( stderr, result, result_length );
	return -1;
    } else {
	fprintf( stdout, "Test %s: OK\n", test_case->test_name );
	return 0;
    }
    
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
	int result = test_ta(test_case);
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
