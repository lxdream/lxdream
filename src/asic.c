/**
 * $Id: asic.c,v 1.16 2006-06-15 10:32:38 nkeynes Exp $
 *
 * Support for the miscellaneous ASIC functions (Primarily event multiplexing,
 * and DMA). 
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

#define MODULE asic_module

#include <assert.h>
#include <stdlib.h>
#include "dream.h"
#include "mem.h"
#include "sh4/intc.h"
#include "sh4/dmac.h"
#include "dreamcast.h"
#include "maple/maple.h"
#include "gdrom/ide.h"
#include "asic.h"
#define MMIO_IMPL
#include "asic.h"
/*
 * Open questions:
 *   1) Does changing the mask after event occurance result in the
 *      interrupt being delivered immediately?
 * TODO: Logic diagram of ASIC event/interrupt logic.
 *
 * ... don't even get me started on the "EXTDMA" page, about which, apparently,
 * practically nothing is publicly known...
 */

static void asic_check_cleared_events( void );
static void asic_init( void );
static void asic_reset( void );
static void asic_save_state( FILE *f );
static int asic_load_state( FILE *f );

struct dreamcast_module asic_module = { "ASIC", asic_init, asic_reset, NULL, NULL,
					NULL, asic_save_state, asic_load_state };

#define G2_BIT5_TICKS 8
#define G2_BIT4_TICKS 16
#define G2_BIT0_ON_TICKS 24
#define G2_BIT0_OFF_TICKS 24

struct asic_g2_state {
    unsigned int bit5_off_timer;
    unsigned int bit4_on_timer;
    unsigned int bit4_off_timer;
    unsigned int bit0_on_timer;
    unsigned int bit0_off_timer;
};

static struct asic_g2_state g2_state;

static void asic_init( void )
{
    register_io_region( &mmio_region_ASIC );
    register_io_region( &mmio_region_EXTDMA );
    mmio_region_ASIC.trace_flag = 0; /* Because this is called so often */
    asic_reset();
}

static void asic_reset( void )
{
    memset( &g2_state, 0, sizeof(g2_state) );
}    

static void asic_save_state( FILE *f )
{
    fwrite( &g2_state, sizeof(g2_state), 1, f );
}

static int asic_load_state( FILE *f )
{
    if( fread( &g2_state, sizeof(g2_state), 1, f ) != 1 )
	return 1;
    else
	return 0;
}


/* FIXME: Handle rollover */
void asic_g2_write_word()
{
    g2_state.bit5_off_timer = sh4r.icount + G2_BIT5_TICKS;
    if( g2_state.bit4_off_timer < sh4r.icount )
	g2_state.bit4_on_timer = sh4r.icount + G2_BIT5_TICKS;
    g2_state.bit4_off_timer = max(sh4r.icount,g2_state.bit4_off_timer) + G2_BIT4_TICKS;
    if( g2_state.bit0_off_timer < sh4r.icount ) {
	g2_state.bit0_on_timer = sh4r.icount + G2_BIT0_ON_TICKS;
	g2_state.bit0_off_timer = g2_state.bit0_on_timer + G2_BIT0_OFF_TICKS;
    } else {
	g2_state.bit0_off_timer += G2_BIT0_OFF_TICKS;
    }
    MMIO_WRITE( ASIC, G2STATUS, MMIO_READ(ASIC, G2STATUS) | 0x20 );
}

static uint32_t g2_read_status()
{
    uint32_t val = MMIO_READ( ASIC, G2STATUS );
    if( g2_state.bit5_off_timer <= sh4r.icount )
	val = val & (~0x20);
    if( g2_state.bit4_off_timer <= sh4r.icount )
	val = val & (~0x10);
    else if( g2_state.bit4_on_timer <= sh4r.icount )
	val = val | 0x10;
    if( g2_state.bit0_off_timer <= sh4r.icount )
	val = val & (~0x01);
    else if( g2_state.bit0_on_timer <= sh4r.icount )
	val = val | 0x01;
    return val | 0x0E;
}   


void asic_event( int event )
{
    int offset = ((event&0x60)>>3);
    int result = (MMIO_READ(ASIC, PIRQ0 + offset))  |=  (1<<(event&0x1F));

    if( result & MMIO_READ(ASIC, IRQA0 + offset) )
        intc_raise_interrupt( INT_IRQ13 );
    if( result & MMIO_READ(ASIC, IRQB0 + offset) )
        intc_raise_interrupt( INT_IRQ11 );
    if( result & MMIO_READ(ASIC, IRQC0 + offset) )
        intc_raise_interrupt( INT_IRQ9 );
}

void asic_clear_event( int event ) {
    int offset = ((event&0x60)>>3);
    uint32_t result = MMIO_READ(ASIC, PIRQ0 + offset)  & (~(1<<(event&0x1F)));
    MMIO_WRITE( ASIC, PIRQ0 + offset, result );

    asic_check_cleared_events();
}

void asic_check_cleared_events( )
{
    int i, setA = 0, setB = 0, setC = 0;
    uint32_t bits;
    for( i=0; i<3; i++ ) {
	bits = MMIO_READ( ASIC, PIRQ0 + i );
	setA |= (bits & MMIO_READ(ASIC, IRQA0 + i ));
	setB |= (bits & MMIO_READ(ASIC, IRQB0 + i ));
	setC |= (bits & MMIO_READ(ASIC, IRQC0 + i ));
    }
    if( setA == 0 )
	intc_clear_interrupt( INT_IRQ13 );
    if( setB == 0 )
	intc_clear_interrupt( INT_IRQ11 );
    if( setC == 0 )
	intc_clear_interrupt( INT_IRQ9 );
}


void asic_ide_dma_transfer( )
{	
    if( MMIO_READ( EXTDMA, IDEDMACTL2 ) == 1 ) {
	if( MMIO_READ( EXTDMA, IDEDMACTL1 ) == 1 ) {
	    MMIO_WRITE( EXTDMA, IDEDMATXSIZ, 0 );
	    
	    uint32_t addr = MMIO_READ( EXTDMA, IDEDMASH4 );
	    uint32_t length = MMIO_READ( EXTDMA, IDEDMASIZ );
	    int dir = MMIO_READ( EXTDMA, IDEDMADIR );
	    
	    uint32_t xfer = ide_read_data_dma( addr, length );
	    MMIO_WRITE( EXTDMA, IDEDMATXSIZ, xfer );
	    MMIO_WRITE( EXTDMA, IDEDMACTL2, 0 );
	} else { /* 0 */
	    MMIO_WRITE( EXTDMA, IDEDMACTL2, 0 );
	}
    }

}


void mmio_region_ASIC_write( uint32_t reg, uint32_t val )
{
    switch( reg ) {
    case PIRQ1:
	val = val & 0xFFFFFFFE; /* Prevent the IDE event from clearing */
	/* fallthrough */
    case PIRQ0:
    case PIRQ2:
	/* Clear any interrupts */
	MMIO_WRITE( ASIC, reg, MMIO_READ(ASIC, reg)&~val );
	asic_check_cleared_events();
	break;
    case MAPLE_STATE:
	MMIO_WRITE( ASIC, reg, val );
	if( val & 1 ) {
	    uint32_t maple_addr = MMIO_READ( ASIC, MAPLE_DMA) &0x1FFFFFE0;
	    // WARN( "Maple request initiated at %08X, halting", maple_addr );
	    maple_handle_buffer( maple_addr );
	    MMIO_WRITE( ASIC, reg, 0 );
	}
	break;
    case PVRDMACTL: /* Initiate PVR DMA transfer */
	MMIO_WRITE( ASIC, reg, val );
	if( val & 1 ) {
	    uint32_t dest_addr = MMIO_READ( ASIC, PVRDMADEST) &0x1FFFFFE0;
	    uint32_t count = MMIO_READ( ASIC, PVRDMACNT );
	    char *data = alloca( count );
	    uint32_t rcount = DMAC_get_buffer( 2, data, count );
	    if( rcount != count )
		WARN( "PVR received %08X bytes from DMA, expected %08X", rcount, count );
	    mem_copy_to_sh4( dest_addr, data, rcount );
	    asic_event( EVENT_PVR_DMA );
	}
	break;
    case PVRDMADEST: case PVRDMACNT: case MAPLE_DMA:
	MMIO_WRITE( ASIC, reg, val );
	break;
    default:
	MMIO_WRITE( ASIC, reg, val );
	WARN( "Write to ASIC (%03X <= %08X) [%s: %s]",
	      reg, val, MMIO_REGID(ASIC,reg), MMIO_REGDESC(ASIC,reg) );
    }
}

int32_t mmio_region_ASIC_read( uint32_t reg )
{
    int32_t val;
    switch( reg ) {
        /*
        case 0x89C:
            sh4_stop();
            return 0x000000B;
        */     
    case PIRQ0:
    case PIRQ1:
    case PIRQ2:
    case IRQA0:
    case IRQA1:
    case IRQA2:
    case IRQB0:
    case IRQB1:
    case IRQB2:
    case IRQC0:
    case IRQC1:
    case IRQC2:
    case MAPLE_STATE:
	val = MMIO_READ(ASIC, reg);
	//            WARN( "Read from ASIC (%03X => %08X) [%s: %s]",
	//                  reg, val, MMIO_REGID(ASIC,reg), MMIO_REGDESC(ASIC,reg) );
	return val;            
    case G2STATUS:
	return g2_read_status();
    default:
	val = MMIO_READ(ASIC, reg);
	WARN( "Read from ASIC (%03X => %08X) [%s: %s]",
	      reg, val, MMIO_REGID(ASIC,reg), MMIO_REGDESC(ASIC,reg) );
	return val;
    }
    
}

MMIO_REGION_WRITE_FN( EXTDMA, reg, val )
{
    // WARN( "EXTDMA write %08X <= %08X", reg, val );

    switch( reg ) {
    case IDEALTSTATUS: /* Device control */
	ide_write_control( val );
	break;
    case IDEDATA:
	ide_write_data_pio( val );
	break;
    case IDEFEAT:
	if( ide_can_write_regs() )
	    idereg.feature = (uint8_t)val;
	break;
    case IDECOUNT:
	if( ide_can_write_regs() )
	    idereg.count = (uint8_t)val;
	break;
    case IDELBA0:
	if( ide_can_write_regs() )
	    idereg.lba0 = (uint8_t)val;
	break;
    case IDELBA1:
	if( ide_can_write_regs() )
	    idereg.lba1 = (uint8_t)val;
	break;
    case IDELBA2:
	if( ide_can_write_regs() )
	    idereg.lba2 = (uint8_t)val;
	break;
    case IDEDEV:
	if( ide_can_write_regs() )
	    idereg.device = (uint8_t)val;
	break;
    case IDECMD:
	if( ide_can_write_regs() ) {
	    ide_write_command( (uint8_t)val );
	}
	break;
    case IDEDMACTL1:
	MMIO_WRITE( EXTDMA, reg, val );
    case IDEDMACTL2:
	MMIO_WRITE( EXTDMA, reg, val );
	asic_ide_dma_transfer( );
	break;
    default:
            MMIO_WRITE( EXTDMA, reg, val );
    }
}

MMIO_REGION_READ_FN( EXTDMA, reg )
{
    uint32_t val;
    switch( reg ) {
    case IDEALTSTATUS: 
	val = idereg.status;
	return val;
    case IDEDATA: return ide_read_data_pio( );
    case IDEFEAT: return idereg.error;
    case IDECOUNT:return idereg.count;
    case IDELBA0: return idereg.disc;
    case IDELBA1: return idereg.lba1;
    case IDELBA2: return idereg.lba2;
    case IDEDEV: return idereg.device;
    case IDECMD:
	val = ide_read_status();
	return val;
    default:
	val = MMIO_READ( EXTDMA, reg );
	return val;
    }
}

