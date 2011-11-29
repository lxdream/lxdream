/**
 * $Id$
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
#include "eventq.h"

struct intc_sources_t {
    char *name;
    uint32_t code;
} intc_sources[INT_NUM_SOURCES] = {
        { "IRQ0", 0x200 },  { "IRQ1", 0x220 },  { "IRQ2", 0x240 },
        { "IRQ3", 0x260 },  { "IRQ4", 0x280 },  { "IRQ5", 0x2A0 },
        { "IRQ6", 0x2C0 },  { "IRQ7", 0x2E0 },  { "IRQ8", 0x300 },
        { "IRQ9", 0x320 },  { "IRQ10",0x340 },  { "IRQ11",0x360 },
        { "IRQ12",0x380 },  { "IRQ13",0x3A0 },  { "IRQ14",0x3C0 },
        { "NMI", 0x1C0 },   { "H-UDI",0x600 },  { "GPIOI",0x620 },
        { "DMTE0",0x640 },  { "DMTE1",0x660 },  { "DMTE2",0x680 },
        { "DMTE3",0x6A0 },  { "DMTAE",0x6C0 },  { "TUNI0",0x400 },
        { "TUNI1",0x420 },  { "TUNI2",0x440 },  { "TICPI2",0x460 },
        { "RTC_ATI",0x480 },{ "RTC_PRI",0x4A0 },{ "RTC_CUI",0x4C0 },
        { "SCI_ERI",0x4E0 },{ "SCI_RXI",0x500 },{ "SCI_TXI",0x520 },
        { "SCI_TEI",0x540 },
        { "SCIF_ERI",0x700 },{ "SCIF_RXI",0x720 },{ "SCIF_BRI",0x740 },
        { "SCIF_TXI",0x760 },
        { "WDT_ITI",0x560 },{ "RCMI",0x580 },   { "ROVI",0x5A0 } };

static int intc_default_priority[INT_NUM_SOURCES] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 16 };

#define PRIORITY(which) intc_state.priority[which]
#define INTCODE(which) intc_sources[which].code

static struct intc_state {
    int num_pending;
    int pending[INT_NUM_SOURCES];
    int priority[INT_NUM_SOURCES];
} intc_state;

MMIO_REGION_WRITE_FN( INTC, reg, val )
{
    reg &= 0xFFF;
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

MMIO_REGION_READ_FN( INTC, reg )
{
    return MMIO_READ( INTC, reg & 0xFFF );
}

MMIO_REGION_READ_DEFSUBFNS(INTC)

void INTC_reset()
{
    int i;

    intc_state.num_pending = 0;
    for( i=0; i<INT_NUM_SOURCES; i++ )
        intc_state.priority[i] = intc_default_priority[i];
    sh4_set_event_pending( event_get_next_time() );
    sh4r.event_types &= (~PENDING_IRQ);
}


void INTC_save_state( FILE *f )
{
    fwrite( &intc_state, sizeof(intc_state), 1, f );
}

int INTC_load_state( FILE *f )
{
    if( fread(&intc_state, sizeof(intc_state), 1, f) != 1 )
        return -1;
    return 0;
}

/* We basically maintain a priority queue here, raise_interrupt adds an entry,
 * accept_interrupt takes it off. At the moment this is done as a simple
 * ordered array, on the basis that in practice there's unlikely to be more
 * than one at a time. There are lots of ways to optimize this if it turns out
 * to be necessary, but I'd doubt it will be...
 */

void intc_raise_interrupt( int which )
{
    int i, j, pri;

    pri = PRIORITY(which);
    if( pri == 0 ) return; /* masked off */

    for( i=0; i<intc_state.num_pending; i++ ) {
        if( intc_state.pending[i] == which ) return; /* Don't queue more than once */
        if( PRIORITY(intc_state.pending[i]) > pri ||
                (PRIORITY(intc_state.pending[i]) == pri &&
                        intc_state.pending[i] < which))
            break;
    }
    /* i == insertion point */
    for( j=intc_state.num_pending; j > i; j-- )
        intc_state.pending[j] = intc_state.pending[j-1];
    intc_state.pending[i] = which;

    if( i == intc_state.num_pending && (sh4r.sr&SR_BL)==0 && SH4_INTMASK() < pri ) {
        sh4_set_event_pending(0);
        sh4r.event_types |= PENDING_IRQ;
    }

    intc_state.num_pending++;
}

void intc_clear_interrupt( int which )
{
    int i;
    for( i=intc_state.num_pending-1; i>=0; i-- ) {
        if( intc_state.pending[i] == which ) {
            /* Shift array contents down */
            while( i < intc_state.num_pending-1 ) {
                intc_state.pending[i] = intc_state.pending[i+1];
                i++;
            }
            intc_state.num_pending--;
            intc_mask_changed();
            break;
        }
    }

}

uint32_t intc_accept_interrupt( void )
{
    assert(intc_state.num_pending > 0);
    return INTCODE(intc_state.pending[intc_state.num_pending-1]);
}

void intc_mask_changed( void )
{   
    if( intc_state.num_pending > 0 && (sh4r.sr&SR_BL)==0 &&
            SH4_INTMASK() < PRIORITY(intc_state.pending[intc_state.num_pending-1]) ) {
        sh4_set_event_pending(0);
        sh4r.event_types |= PENDING_IRQ ;
    }
    else {
        sh4_set_event_pending(event_get_next_time());
        sh4r.event_types &= (~PENDING_IRQ);
    }
}


char *intc_get_interrupt_name( int code )
{
    return intc_sources[code].name;
}
