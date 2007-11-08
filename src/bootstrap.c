/**
 * $Id: bootstrap.c,v 1.9 2007-11-08 11:54:16 nkeynes Exp $
 *
 * CD Bootstrap header parsing. Mostly for informational purposes.
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

#include "dream.h"
#include "bootstrap.h"

/**
 * Bootstrap header structure
 */
typedef struct dc_bootstrap_head {
    char hardware_id[16]; /* must be "SEGA SEGAKATANA " */ 
    char maker_id[16];    /* ditto,  "SEGA ENTERPRISES" */
    char crc[4];
    char padding;         /* normally ascii space */
    char gdrom_id[6];
    char disc_no[5];
    char regions[8];
    char peripherals[8];
    char product_id[10];
    char product_ver[6];
    char product_date[16];
    char boot_file[16];
    char vendor_id[16];
    char product_name[128];
} *dc_bootstrap_head_t;

static uint32_t compute_crc16( dc_bootstrap_head_t h )
{
    /* Note: Algorithm taken from http://mc.pp.se/dc/ip0000.bin.html */
    uint32_t i, c, n = 0xffff;
    char *data = h->product_id;
    for (i = 0; i < 16; i++)
    {
        n ^= (data[i]<<8);
        for (c = 0; c < 8; c++)
            if (n & 0x8000)
                n = (n << 1) ^ 4129;
            else
                n = (n << 1);
    }
    return n & 0xffff;
}


static char *dc_peripherals[] = { "Uses WinCE", "Unknown (0x0000002)",
                                  "Unknown (0x0000004)", "Unknown (0x0000008)",
                                  "VGA Box", "Unknown (0x0000020)",
                                  "Unknown (0x0000040)", "Unknown (0x0000080)",
                                  "Other Expansions", "Puru Puru pack",
                                  "Mike", "Memory card",
                                  "Basic controller", "C button",
                                  "D button", "X button",
                                  "Y button", "Z button",
                                  "Expanded direction buttons",
                                  "Analog R trigger", "Analog L trigger",
                                  "Analog horizontal", "Analog vertical",
                                  "Expanded analog horizontal",
                                  "Expanded analog vertical",
                                  "Gun", "Keyboard", "Mouse" };


/* Expansion units */
#define DC_PERIPH_WINCE    0x0000001
#define DC_PERIPH_VGABOX   0x0000010
#define DC_PERIPH_OTHER    0x0000100
#define DC_PERIPH_PURUPURU 0x0000200
#define DC_PERIPH_MIKE     0x0000400
#define DC_PERIPH_MEMCARD  0x0000800
/* Basic requirements */
#define DC_PERIPH_BASIC    0x0001000 /* Basic controls - start, a, b, arrows */
#define DC_PERIPH_C_BUTTON 0x0002000
#define DC_PERIPH_D_BUTTON 0x0004000
#define DC_PERIPH_X_BUTTON 0x0008000
#define DC_PERIPH_Y_BUTTON 0x0010000
#define DC_PERIPH_Z_BUTTON 0x0020000
#define DC_PERIPH_EXP_DIR  0x0040000 /* Expanded direction buttons */
#define DC_PERIPH_ANALOG_R 0x0080000 /* Analog R trigger */
#define DC_PERIPH_ANALOG_L 0x0100000 /* Analog L trigger */
#define DC_PERIPH_ANALOG_H 0x0200000 /* Analog horizontal controller */
#define DC_PERIPH_ANALOG_V 0x0400000 /* Analog vertical controller */
#define DC_PERIPH_EXP_AH   0x0800000 /* Expanded analog horizontal (?) */
#define DC_PERIPH_EXP_AV   0x1000000 /* Expanded analog vertical (?) */
/* Optional peripherals */
#define DC_PERIPH_GUN      0x2000000
#define DC_PERIPH_KEYBOARD 0x4000000
#define DC_PERIPH_MOUSE    0x8000000

/**
 * Dump the bootstrap info to the output log for infomational/debugging
 * purposes.
 * @param detail true to include a ful information dump, false for just
 *  the facts, maam.
 */
void bootstrap_dump( void *data, gboolean detail )
{
    struct dc_bootstrap_head *head;
    int i, got, periph, crc, hcrc;
    char *prot_symbols;
    char buf[512];

    /* Dump out the bootstrap metadata table */
    head = (struct dc_bootstrap_head *)data;
    prot_symbols = ((char *)data) + 0x3700;
    memcpy( buf, head->product_name, 128 );
    for( i=127; i>0 && buf[i] == ' '; i-- );
    buf[i] = '\0';
    periph = strtol( head->peripherals, NULL, 16 );
    INFO( "Name: %s   Author: %-16.16s",
          buf, head->vendor_id );
    sprintf( buf, "%4.4s", head->crc );
    crc = compute_crc16(head);
    hcrc = strtol( buf, NULL, 16 );
    INFO( "  Product ID:   %-10.10s   Product Ver: %-6.6s   Date: %-8.8s",
          head->product_id, head->product_ver, head->product_date );
    if( detail ) {
	INFO( "  Header CRC:   %04X (Computed %04X)", hcrc, crc );
	INFO( "  Boot File:    %-16.16s", head->boot_file );
	INFO( "  Disc ID:      %-11.11s  Regions:      %-8.8s   Peripherals: %07X",
	      head->gdrom_id, head->regions, periph );
	strcpy( buf, "     Supports: " );
	got = 0;
	for( i=0; i<28; i++ ) {
	    if( periph & (1<<i) ){
		if( got ) strcat( buf, ", " );
		strcat( buf, dc_peripherals[i] );
		got = 1;
	    }
	    if( i == 11 ) i = 23; /* Skip 8-23 */
	}
	INFO( buf, NULL );
	strcpy( buf, "     Requires: " );
	got = 0;
	for( i=12; i<24; i++ ) {
	    if( periph & (1<<i) ) {
		if( got ) strcat( buf, ", " );
		strcat( buf, dc_peripherals[i] );
		got = 1;
	    }
	}
	INFO( buf, NULL );
    }
}
