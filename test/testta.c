/**
 * $Id: testta.c,v 1.4 2006-08-18 09:32:32 nkeynes Exp $
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
#define OBJ_LENGTH 0x00010000
#define TILE_START 0x00060000
#define TILE_LENGTH 0x00010000

#define MEM_FILL 0xFE

int ta_tile_sizes[4] = { 0, 32, 64, 128 };

#define TILE_SIZE(cfg, tile) ta_tile_sizes[((((cfg->ta_cfg) >> (4*tile))&0x03))]

struct ta_config default_ta_config = { 0x00111111, GRID_SIZE(640,480), OBJ_START,
				       OBJ_START+OBJ_LENGTH, TILE_START+TILE_LENGTH,
				       TILE_START, TILE_START+TILE_LENGTH };


int tile_sizes[5];
int tile_events[5] = { EVENT_PVR_OPAQUE_DONE, EVENT_PVR_OPAQUEMOD_DONE,
		       EVENT_PVR_TRANS_DONE, EVENT_PVR_TRANSMOD_DONE,
		       EVENT_PVR_PUNCHOUT_DONE };
char *tile_names[5] = { "Opaque", "Opaque Mod", "Trans", "Trans Mod", "Punch Out" };

#define FLOAT(p) *((float *)(p))

void make_expected_buffer( test_data_block_t expected_block, char *expect, int length )
{
    memset( expect, MEM_FILL,length );
    
    if( expected_block != NULL ) {
	if( expected_block->length > length ) {
	    fprintf( stderr, "Test data error: expected tile length is %d, but tile size is only %d\n", expected_block->length, length );
	    return;
	}
	memcpy( expect, expected_block->data, expected_block->length );
	
	if( expected_block->length <= length-4 ) {
	    *((unsigned int *)&expect[expected_block->length]) = 0xF0000000;
	}
    }
}

int tilematrix_block_compare( test_data_block_t expected_block, char *tile_ptrs[], int tile_type, int offset )
{
    int tile_size = tile_sizes[tile_type];
    char expect[tile_size];

    make_expected_buffer(expected_block, expect, tile_size);
    return memcmp( expect, tile_ptrs[tile_type]+(offset*tile_sizes[tile_type]), tile_size );
}

/**
 * Copy from vram, wrapping appropriately 
 */
int memcpy_from_vram( char *dest, char *src, int len ) 
{
    while( len > 0 ) {
	*dest++ = *src++;
	src = (char *)( ((unsigned int)src) & 0xFF7FFFFF );
	len--;
    }
}

int test_ta( test_data_t test_case )
{
    char buf[1024];
    unsigned int *p = DMA_ALIGN(buf);
    unsigned int *data = p;
    int haveFailure = 0;
    int checkedTile[5] = {0,0,0,0,0};
    int i;
    int hsegs,vsegs;
    char *tile_ptrs[5];

    asic_clear();

    memset( PVR_VRAM_BASE, MEM_FILL,  0x00090000 );
    test_data_block_t config_data = get_test_data( test_case, "config" );
    struct ta_config *config = &default_ta_config;
    if( config_data != NULL ) {
	if( config_data->length != sizeof(struct ta_config) ) {
	    fprintf( stderr, "Invalid config data length %d - aborting test %s\n",
		     config_data->length, test_case->test_name );
	    return -1;
	}
	config = (struct ta_config *)config_data->data;
    }
    char *result = (char *)(PVR_VRAM_BASE+config->obj_start);
    char *tilematrix = (char *)(PVR_VRAM_BASE+config->tile_start);

    ta_init(config);
    for( i=0; i<5; i++ ) {
	tile_sizes[i] = TILE_SIZE(config,i);
    }
    hsegs = (config->grid_size & 0xFFFF)+1;
    vsegs = (config->grid_size >> 16) + 1;
    tile_ptrs[0] = tilematrix;
    tile_ptrs[1] = tile_ptrs[0] + (hsegs*vsegs*tile_sizes[0]);
    tile_ptrs[2] = tile_ptrs[1] + (hsegs*vsegs*tile_sizes[1]);
    tile_ptrs[3] = tile_ptrs[2] + (hsegs*vsegs*tile_sizes[2]);
    tile_ptrs[4] = tile_ptrs[3] + (hsegs*vsegs*tile_sizes[3]);


    test_data_block_t input = get_test_data(test_case, "input");
    test_data_block_t input2 = get_test_data(test_case, "input2");
    test_data_block_t output = get_test_data(test_case, "output");
    test_data_block_t error = get_test_data(test_case, "error");
    if( input == NULL || output == NULL ) {
	fprintf( stderr, "Skipping test case '%s': data incomplete\n", test_case->test_name );
	return -1;
    }

    if( pvr_dma_write( 0x10000000, input->data, input->length, 0 ) == -1 ) {
	return -1;
    }

    if( input2 != NULL ) {
	ta_reinit();
	pvr_dma_write( 0x10000000, input2->data, input2->length, 0 );
    }
    

    if( error != NULL ) {
	for( i=0; i<error->length; i++ ) {
	    if( asic_wait( error->data[i] ) == -1 ) {
		fprintf( stderr, "Test %s: failed (Timeout waiting for error event %d)\n",
			 test_case->test_name, error->data[i] );
		asic_dump( stderr );
		return -1;
	    }
	}
    }

    for( i=0; i<MAX_DATA_BLOCKS; i++ ) {
	test_data_block_t data = &test_case->item[i];
	int tile, x, y, offset;
	if( data->name != NULL ) {
	    int result = sscanf( data->name, "tile %d %dx%d", &tile, &x, &y );
	    if( result == 1 ) {
		x = y = 0;
	    } else if( result != 3 ) {
		continue;
	    }
	    tile--;
	    offset = x + (y * hsegs);

	    if( checkedTile[tile] == 0 ) {
		if( asic_wait( tile_events[tile] ) == -1 ) {
		    fprintf( stderr, "Test %s: failed (Timeout waiting for %s done event)\n", 
			     test_case->test_name, tile_names[tile] );
		    ta_dump_regs();
		    asic_dump( stderr );
		    haveFailure = 1;
		}
	    }

	    if( tilematrix_block_compare( data, tile_ptrs, tile, offset ) != 0 ) {
		fprintf( stderr, "Test %s: Failed (%s matrix %dx%d). ", 
			 test_case->test_name, tile_names[tile], x, y );
		fwrite_diff32( stderr, data->data, data->length, 
			       tile_ptrs[tile] + (tile_sizes[tile]*offset), tile_sizes[tile] );
		haveFailure = 1;
	    }
	    checkedTile[tile] = 1;
	}
    }

    /* Overflow */
    test_data_block_t plist = get_test_data(test_case, "plist" );
    if( plist != NULL ) {
	unsigned int plist_posn, plist_end;
	if( config->ta_cfg & 0x00100000 ) { /* Descending */
	    plist_posn = pvr_get_plist_posn(); //+ tile_sizes[0];
	    plist_end = config->plist_start;
	} else {
	    plist_posn = config->plist_start;
	    plist_end = pvr_get_plist_posn() + tile_sizes[0];
	}
	char *plist_data = (char *)(PVR_VRAM_BASE + plist_posn);
	if( test_block_compare( plist, plist_data, plist_end-plist_posn ) != 0 ) {
	    fprintf( stderr, "Test %s: Failed (Plist buffer)", test_case->test_name );
	    fwrite_diff32( stderr, plist->data, plist->length, (char *)plist_data, 
			   plist_end - plist_posn );
	    haveFailure = 1;
	}
	char block[tile_sizes[0]];
	memset( block, MEM_FILL, tile_sizes[0] );
	if( memcmp( block, plist_data - tile_sizes[0], tile_sizes[0] ) != 0 ) {
	    fprintf( stderr, "Test %s: Failed (Plist buffer)", test_case->test_name );
	    fwrite_diff32( stderr, block, tile_sizes[0], plist_data - tile_sizes[0],
			   tile_sizes[0]);
	    haveFailure = 1;
	}
    }

    /* Vertex buffer */
    int result_length = pvr_get_objbuf_size();
    char tmp[result_length];
    memcpy_from_vram( tmp, result, result_length );
    if( test_block_compare( output, tmp, result_length ) != 0 ) {
	fprintf( stderr, "Test %s: Failed (Vertex buffer). ", test_case->test_name );
	fwrite_diff32( stderr, output->data, output->length, tmp, result_length );
	haveFailure = 1;
    }
    

    for( i=0; i<5; i++ ) {
	if( checkedTile[i] == 0 ) {
	    if( tilematrix_block_compare( NULL, tile_ptrs, i, 0 ) != 0 ) {
		fprintf( stderr, "Test %s: Failed (%s matrix). ", test_case->test_name, tile_names[i] );
                fprintf( stderr, "Expected empty buffer at %08X, but was =>\n", 
			 (unsigned int)(tile_ptrs[i]) );
		fwrite_dump( stderr, tile_ptrs[i], tile_sizes[i] );
		//		fwrite_dump( stderr, tile_ptrs[i] - 128, 256 );
			 
	    }
	}
    }

    if( error == NULL ) {
	if( asic_check(EVENT_TA_ERROR) || asic_check(EVENT_PVR_PRIM_ALLOC_FAIL) ||
	    asic_check(EVENT_PVR_MATRIX_ALLOC_FAIL) || asic_check(EVENT_PVR_BAD_INPUT) ) {
	    fprintf( stderr, "Test %s: Failed (unexpected error events)\n", test_case->test_name );
	    asic_dump( stderr );
	    haveFailure = 1;
	}
    }

    if( haveFailure )
	return -1;

    fprintf( stdout, "Test %s: OK\n", test_case->test_name );
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
