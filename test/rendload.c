/**
 * $Id$
 *
 * Scene-save loader support. This is the other side of rendsave.c
 *
 * Copyright (c) 2005 Nathan Keynes.
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
#include <errno.h>
#include "lib.h"
#include "asic.h"
#include "pvr.h"

char *scene_magic = "%!-lxDream!Scene";

#define PVR2_BASE 0xA05F8000

#define COPYREG(x) long_write( (PVR2_BASE+x), scene_header.pvr2_regs[(x>>2)] )

int pvr2_render_load_scene( FILE *f )
{
    struct {
	char magic[16];
	uint32_t version;
	uint32_t timestamp;
	uint32_t frame_count;
	uint32_t pvr2_regs[0x400];
	uint32_t palette[0x400];
    } scene_header;
    uint32_t start = 0xFFFFFFFF, length;
    
    if( fread( &scene_header, sizeof(scene_header), 1, f ) != 1 ) {
	fprintf( stderr, "Failed to read scene header (%s)\n", strerror(errno) );
	return -1;
    }
    if( memcmp( scene_magic, scene_header.magic, 16 ) != 0 ) {
	fprintf( stderr, "Failed to load scene data: Header not found\n" );
	fclose(f);
	return -1;
    }

    fprintf( stdout, "Loaded header\n" );
    /* Load the VRAM sections */
    while(1) {
	fread( &start, sizeof(uint32_t), 1, f );
	if( start == 0xFFFFFFFF ) { /* EOF */
	    break;
	}
	fread( &length, sizeof(uint32_t), 1, f );
	int read = fread( (char *)(0xA5000000 + start), 1, length, f );
	if( read != length ) {
	    fprintf( stderr, "Failed to load %d bytes at %08X (Got %d - %s)\n",
		     length, start, read, strerror(errno) );
	    fclose(f);
	    return -2;
	} else {
	    fprintf( stdout, "Loaded %d bytes at %08X\n", length, start );
	}
    }

    /* Copy the fog table */
    memcpy( (char *)0xA05F8200, ((char *)scene_header.pvr2_regs) + 0x200, 0x200 );
    /* Copy the palette data */
    memcpy( (char *)0xA05F9000, scene_header.palette, 4096 );
    /* And now the other registers */
    COPYREG(0x018);
    COPYREG(0x020);
    COPYREG(0x02C);
    COPYREG(0x030);
    COPYREG(0x044);
    COPYREG(0x048);
    COPYREG(0x04C);
    COPYREG(0x05C);
    COPYREG(0x060);
    COPYREG(0x064);
    COPYREG(0x068);
    COPYREG(0x06C);
    COPYREG(0x074);
    COPYREG(0x078);
    COPYREG(0x07C);
    COPYREG(0x080);
    COPYREG(0x084);
    COPYREG(0x088);
    COPYREG(0x08C);
    COPYREG(0x098);
    COPYREG(0x0B0);
    COPYREG(0x0B4);
    COPYREG(0x0B8);
    COPYREG(0x0BC);
    COPYREG(0x0C0);
    COPYREG(0x0E4);
    COPYREG(0x0F4);
    COPYREG(0x108);
    COPYREG(0x110);
    COPYREG(0x114);
    COPYREG(0x118);    
    fprintf( stdout, "Load complete\n" );
    fclose(f);
}


void pvr2_render_loaded_scene()
{
    asic_clear();
    long_write( (PVR2_BASE+0x14), 0xFFFFFFFF );
    asic_wait( EVENT_PVR_RENDER_DONE );
    asic_clear();
    asic_wait( EVENT_RETRACE );
    uint32_t addr = long_read( (PVR2_BASE+0x060) );
    uint32_t mod = long_read( (PVR2_BASE+0x04C) );
    long_write( (PVR2_BASE+0x050), addr);
    long_write( (PVR2_BASE+0x054), addr + (mod<<3) );
}


int main()
{
    pvr_init();
    pvr2_render_load_scene(stdin);
    pvr2_render_loaded_scene();
    asic_clear();
    asic_wait(EVENT_RETRACE);
    asic_clear();
    asic_wait(EVENT_RETRACE);
    asic_clear();
    asic_wait(EVENT_RETRACE);
}
