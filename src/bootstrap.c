#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "ipbin.h"
#include "gui/gui.h"

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

void parse_ipbin( char *data )
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
    INFO( "Bootstrap loaded, Name: %s   Author: %-16.16s",
          buf, head->vendor_id );
    sprintf( buf, "%4.4s", head->crc );
    crc = compute_crc16(head);
    hcrc = strtol( buf, NULL, 16 );
    emit( NULL, crc == hcrc ? EMIT_INFO : EMIT_WARN, MODULE_ID, 
          "  Header CRC:   %04X (Computed %04X)", hcrc, crc );
    INFO( "  Boot File:    %-16.16s", head->boot_file );
    INFO( "  Product ID:   %-10.10s   Product Ver: %-6.6s   Date: %-8.8s",
          head->product_id, head->product_ver, head->product_date );
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
#if 0
    INFO( "  Area protection symbols:", NULL );
    for( i=0; i<8; i++ )
        INFO( "    %d: %28.28s", i, &prot_symbols[(i*32)+4] );
#endif
}
