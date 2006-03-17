/**
 * $Id: intc.c,v 1.5 2006-03-17 12:12:49 nkeynes Exp $
 *
 * SH4 onboard interrupt controller (INTC) implementation
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

#include <assert.h>
#include "sh4mmio.h"
#include "sh4core.h"
#include "intc.h"

int priorities[12] = {0,0,0,0,0,0,0,0,0,0,0,0};

struct intc_sources_t {
    char *name;
    uint32_t code;
    int priority;
};

#define PRIORITY(which) intc_sources[which].priority
#define INTCODE(which) intc_sources[which].code

static struct intc_sources_t intc_sources[] = {
    { "IRQ0", 0x200, 15 }, { "IRQ1", 0x220, 14 }, { "IRQ2", 0x240, 13 },
    { "IRQ3", 0x260, 12 }, { "IRQ4", 0x280, 11 }, { "IRQ5", 0x2A0, 10 },
    { "IRQ6", 0x2C0, 9 },  { "IRQ7", 0x2E0, 8 },  { "IRQ8", 0x300, 7 },
    { "IRQ9", 0x320, 6 },  { "IRQ10",0x340, 5 },  { "IRQ11",0x360, 4 },
    { "IRQ12",0x380, 3 },  { "IRQ13",0x3A0, 2 },  { "IRQ14",0x3C0, 1 },
    { "NMI", 0x1C0, 16 },  { "H-UDI",0x600, 0 },  { "GPIOI",0x620, 0 },
    { "DMTE0",0x640, 0 },  { "DMTE1",0x660, 0 },  { "DMTE2",0x680, 0 },
    { "DMTE3",0x6A0, 0 },  { "DMTAE",0x6C0, 0 },  { "TUNI0",0x400, 0 },
    { "TUNI1",0x420, 0 },  { "TUNI2",0x440, 0 },  { "TICPI2",0x460, 0 },
    { "RTC_ATI",0x480, 0 },{ "RTC_PRI",0x4A0, 0 },{ "RTC_CUI",0x4C0, 0 },
    { "SCI_ERI",0x4E0, 0 },{ "SCI_RXI",0x500, 0 },{ "SCI_TXI",0x520, 0 },
    { "SCI_TEI",0x540, 0 },
    { "SCIF_ERI",0x700, 0 },{ "SCIF_RXI",0x720, 0 },{ "SCIF_BRI",0x740, 0 },
    { "SCIF_TXI",0x760, 0 },
    { "WDT_ITI",0x560, 0 },{ "RCMI",0x580, 0 },   { "ROVI",0x5A0, 0 } };

int intc_pending[INT_NUM_SOURCES];
int intc_num_pending = 0;

void mmio_region_INTC_write( uint32_t reg, uint32_t val )
{
    /* Well it saves having to use an intermediate table... */
    switch( reg ) {
        case ICR: /* care about this later */
            break;
        case IPRA:
            PRIORITY(INT_TMU_TUNI0) = (val>>12)&0x000F;
            PRIORITY(INT_TMU_TUNI1) = (val>>8)&0x000F;
            PRIORITY(INT_TMU_TUNI2) =
                PRIORITY(INT_TMU_TICPI2) = (val>>4)&0x000F;
            PRIORITY(INT_RTC_ATI) =
                PRIORITY(INT_RTC_PRI) =
                PRIORITY(INT_RTC_CUI) = val&0x000F;
            break;
        case IPRB:
            PRIORITY(INT_WDT_ITI) = (val>>12)&0x000F;
            PRIORITY(INT_REF_RCMI) =
                PRIORITY(INT_REF_ROVI) = (val>>8)&0x000F;
            PRIORITY(INT_SCI_ERI) =
                PRIORITY(INT_SCI_RXI) =
                PRIORITY(INT_SCI_TXI) =
                PRIORITY(INT_SCI_TEI) = (val>>4)&0x000F;
            /* Bits 0-3 reserved */
            break;
        case IPRC:
            PRIORITY(INT_GPIO) = (val>>12)&0x000F;
            PRIORITY(INT_DMA_DMTE0) =
                PRIORITY(INT_DMA_DMTE1) =
                PRIORITY(INT_DMA_DMTE2) =
                PRIORITY(INT_DMA_DMTE3) =
                PRIORITY(INT_DMA_DMAE) = (val>>8)&0x000F;
            PRIORITY(INT_SCIF_ERI) =
                PRIORITY(INT_SCIF_RXI) =
                PRIORITY(INT_SCIF_BRI) =
                PRIORITY(INT_SCIF_TXI) = (val>>4)&0x000F;
            PRIORITY(INT_HUDI) = val&0x000F;
            break;
    }
    MMIO_WRITE( INTC, reg, val );
}

int32_t mmio_region_INTC_read( uint32_t reg )
{
    return MMIO_READ( INTC, reg );
}
        
/* We basically maintain a priority queue here, raise_interrupt adds an entry,
 * accept_interrupt takes it off. At the moment this is does as a simple
 * ordered array, on the basis that in practice there's unlikely to be more
 * than one at a time. There are lots of ways to optimize this if it turns out
 * to be necessary, but I'd doubt it will be...
 */

void intc_raise_interrupt( int which )
{
    int i, j, pri;
    
    pri = PRIORITY(which);
    if( pri == 0 ) return; /* masked off */
    
    for( i=0; i<intc_num_pending; i++ ) {
        if( intc_pending[i] == which ) return; /* Don't queue more than once */
        if( PRIORITY(intc_pending[i]) > pri ||
            (PRIORITY(intc_pending[i]) == pri &&
             intc_pending[i] < which))
            break;
    }
    /* i == insertion point */
    for( j=intc_num_pending; j > i; j-- )
        intc_pending[j] = intc_pending[j-1];
    intc_pending[i] = which;

    if( i == intc_num_pending && (sh4r.sr&SR_BL)==0 && SH4_INTMASK() < pri )
        sh4r.int_pending = 1;

    intc_num_pending++;
}

void intc_clear_interrupt( int which )
{
    int i;
    for( i=intc_num_pending-1; i>=0; i-- ) {
	if( intc_pending[i] == which ) {
	    /* Shift array contents down */
	    while( i < intc_num_pending-1 ) {
		intc_pending[i] = intc_pending[++i];
	    }
	    intc_num_pending--;
	    intc_mask_changed();
	    break;
	}
    }
    
}

uint32_t intc_accept_interrupt( void )
{
    assert(intc_num_pending > 0);
    return INTCODE(intc_pending[intc_num_pending-1]);
}

void intc_mask_changed( void )
{   
    if( intc_num_pending > 0 && (sh4r.sr&SR_BL)==0 &&
        SH4_INTMASK() < PRIORITY(intc_pending[intc_num_pending-1]) )
        sh4r.int_pending = 1;
    else sh4r.int_pending = 0;
}
    

char *intc_get_interrupt_name( int code )
{
    return intc_sources[code].name;
}

void intc_reset( void )
{
    intc_num_pending = 0;
    sh4r.int_pending = 0;
}
