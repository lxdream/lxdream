/**
 * $Id: testrend.c,v 1.2 2006-08-19 01:51:16 nkeynes Exp $
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
#include "testdata.h"
#include "pvr.h"
#include "lib.h"
#include "asic.h"

#define OBJ_START 0x00010000
#define OBJ_LENGTH 0x00010000
#define TILE_START 0x00060000
#define TILE_LENGTH 0x00010000
#define TILEMAP_ADDR 0x050B2C8
#define RENDER_ADDR 0x00600000

struct ta_config default_ta_config = { 0x00111111, GRID_SIZE(640,480), OBJ_START,
				       OBJ_START+OBJ_LENGTH, TILE_START+TILE_LENGTH,
				       TILE_START, TILE_START+TILE_LENGTH };

struct render_config default_render_config = { OBJ_START, TILEMAP_ADDR, RENDER_ADDR, 640, 480, 
					       0x00000009, 0.2, 1.0 };

int test_render( test_data_t test_case )
{
    int i;

    /* Check input */
    test_data_block_t input = get_test_data(test_case, "input");
    test_data_block_t event = get_test_data(test_case, "event");
    test_data_block_t backplane = get_test_data(test_case, "backplane");
    if( input == NULL ) {
	fprintf( stderr, "Skipping test '%s' - no input\n", test_case->test_name );
	return -1;
    }
    if( event == NULL ) {
	fprintf( stderr, "Skipping test '%s' - no event list\n", test_case->test_name );
    }

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
    
    /* Send TA data */
    asic_clear();
    pvr_init();
    ta_init(config);
    default_render_config.polybuf = config->obj_start & 0x00F00000;
    if( pvr_dma_write( 0x10000000, input->data, input->length, 0 ) == -1 ) {
	return -1;
    }
    
    /* Wait for events */
    for( i=0; i<event->length; i++ ) {
	if( asic_wait( event->data[i] ) == -1 ) {
	    fprintf( stderr, "Test %s: failed (Timeout waiting for event %d)\n",
		     test_case->test_name, event->data[i] );
	    asic_dump( stderr );
	    return -1;
	}
    }

    /* Write backplane (if any) */
    if( backplane != NULL ) {
	uint32_t bgplane = pvr_get_objbuf_posn();
	memcpy( (char *)(PVR_VRAM_BASE + bgplane), backplane->data, backplane->length );
	bgplane -= default_render_config.polybuf;
	render_set_backplane( (bgplane << 1) | 0x01000000 );
    } else {
	render_set_backplane( 0 );
    }
    /* Setup the tilemap */
    pvr_build_tilemap1( TILEMAP_ADDR, config, 0x20000000 );
    render_start( &default_render_config );
    if( asic_wait( EVENT_PVR_RENDER_DONE ) == -1 ) {
	fprintf( stderr, "Test %s: failed (timeout waiting for render)\n",
		 test_case->test_name );
	asic_dump( stderr );
	return -1;
    }
    asic_wait( EVENT_RETRACE );
    display_render( &default_render_config );
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
	int result = test_render(test_case);
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
