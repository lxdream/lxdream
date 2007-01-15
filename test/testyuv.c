/**
 * $Id: testyuv.c,v 1.2 2007-01-15 10:41:30 nkeynes Exp $
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
#define TEXSIZE (PVR_BASE+0x0E4)
#define DISPLAY_MODE    (PVR_BASE+0x044)
#define DISPLAY_ADDR1   (PVR_BASE+0x050)
#define DISPLAY_ADDR2   (PVR_BASE+0x054)
#define DISPLAY_SIZE    (PVR_BASE+0x05C)

#define OBJ_START 0x00010000
#define OBJ_LENGTH 0x00010000
#define TILE_START 0x00060000
#define TILE_LENGTH 0x00010000
#define TILEMAP_ADDR 0x050B2C8
#define RENDER_ADDR 0x00600000

#define OBJCFG 0xA05F807C
#define ISPCFG 0xA05F8098

struct ta_config default_ta_config = { 0x00100002, GRID_SIZE(640,480), OBJ_START,
				       OBJ_START+OBJ_LENGTH, TILE_START+TILE_LENGTH,
				       TILE_START, TILE_START+TILE_LENGTH };



struct render_config default_render_config = { OBJ_START, TILEMAP_ADDR, RENDER_ADDR, 640, 480, 
					       0x00000009, 0.0001f, 1.0f };


struct backplane {
    uint32_t mode1, mode2, mode3;
    float x1, y1, z1;
    uint32_t col1;
    float x2, y2, z2;
    uint32_t col2;
    float x3, y3, z3;
    uint32_t col3;
};


#define START_BLEND 0
#define BG_COLOUR 0xFF808080

struct backplane default_backplane = { 0x90800000, 0x20800440, 0,
				       0.0, 0.0, 0.2, BG_COLOUR,
				       640.0, 0.0, 0.2, BG_COLOUR,
				       0.0, 480.0, 0.2, BG_COLOUR };

struct yuv_poly {
    uint32_t poly[8];
    struct {
	uint32_t mode;
	float x,y,z,u,v;
	uint32_t colour;
	uint32_t pad;
    } vertex[4];
    uint32_t end[8];
} test_yuv_poly = { { 0x808C010A, 0xE0000000, 0x2091A4F6, 0x1E000000, 0, 0, 0, 0 },
		    { { 0xE0000000, 0.0, 0.0, 2.0, 0.0, 0.0, 0xFFFFFFFF, 0 },
		      { 0xE0000000, 640.0, 0.0, 2.0, 0.625, 0.0, 0xFFFFFFFF, 0 },
		      { 0xE0000000, 0.0, 480.0, 2.0, 0.0, 0.9375, 0xFFFFFFFF, 0 },
		      { 0xF0000000, 640.0, 480.0, 2.0, 0.625, 0.9375, 0xFFFFFFFF, 0 } },
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

    struct ta_config *config = &default_ta_config;
    
    /* Send TA data */
    asic_clear();
    pvr_init();
    ta_init(config);
    memcpy( p, &test_yuv_poly, sizeof(test_yuv_poly) );
    if( pvr_dma_write( 0x10000000, p, sizeof(test_yuv_poly), 0 ) != 0 ) {
	return -1;
    }
    asic_wait( EVENT_PVR_OPAQUE_DONE );
    if( asic_check( EVENT_PVR_PRIM_ALLOC_FAIL ) ||
	asic_check( EVENT_PVR_MATRIX_ALLOC_FAIL ) ||
	asic_check( EVENT_PVR_BAD_INPUT ) ) {
	asic_dump(stderr);
	return -1;
    }

    /* Write backplane (if any) */
    uint32_t bgplane = pvr_get_objbuf_posn();
    memcpy( (char *)(PVR_VRAM_BASE + bgplane), &default_backplane, sizeof(default_backplane) );
    bgplane -= default_render_config.polybuf;
    render_set_backplane( (bgplane << 1) | 0x01000000 );

    /* Render the damn thing */
    long_write( OBJCFG, 0x0027DF77 );
    long_write( ISPCFG, 0x00800409 );
    long_write( TEXSIZE, 0x0000000A );
    pvr_build_tilemap2( TILEMAP_ADDR, config, 0x10000000 );
    render_start( &default_render_config );
    if( asic_wait( EVENT_PVR_RENDER_DONE ) == -1 ) {
	fprintf( stderr, "Test render failed (timeout waiting for render)\n" );
	asic_dump( stderr );
	return -1;
    }
    
    asic_clear();
    asic_wait( EVENT_RETRACE );
    display_render( &default_render_config );
    asic_clear();
    asic_wait(EVENT_RETRACE);
    asic_clear();
    asic_wait(EVENT_RETRACE);
    asic_clear();
    asic_wait(EVENT_RETRACE);
    asic_clear();
    asic_wait(EVENT_RETRACE);
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
