/**
 * $Id$
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
#include "eventq.h"
#include "dream.h"
#include "mem.h"
#include "sh4/intc.h"
#include "sh4/dmac.h"
#include "sh4/sh4.h"
#include "dreamcast.h"
#include "maple/maple.h"
#include "gdrom/ide.h"
#include "pvr2/pvr2.h"
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
static uint32_t asic_run_slice( uint32_t nanosecs );
static void asic_save_state( FILE *f );
static int asic_load_state( FILE *f );
static uint32_t g2_update_fifo_status( uint32_t slice_cycle );

struct dreamcast_module asic_module = { "ASIC", asic_init, asic_reset, NULL, asic_run_slice,
        NULL, asic_save_state, asic_load_state };

#define G2_BIT5_TICKS 60
#define G2_BIT4_TICKS 160
#define G2_BIT0_ON_TICKS 120
#define G2_BIT0_OFF_TICKS 420

struct asic_g2_state {
    int bit5_off_timer;
    int bit4_on_timer;
    int bit4_off_timer;
    int bit0_on_timer;
    int bit0_off_timer;
};

static struct asic_g2_state g2_state;

static uint32_t asic_run_slice( uint32_t nanosecs )
{
    g2_update_fifo_status(nanosecs);
    if( g2_state.bit5_off_timer <= (int32_t)nanosecs ) {
        g2_state.bit5_off_timer = -1;
    } else {
        g2_state.bit5_off_timer -= nanosecs;
    }

    if( g2_state.bit4_off_timer <= (int32_t)nanosecs ) {
        g2_state.bit4_off_timer = -1;
    } else {
        g2_state.bit4_off_timer -= nanosecs;
    }
    if( g2_state.bit4_on_timer <= (int32_t)nanosecs ) {
        g2_state.bit4_on_timer = -1;
    } else {
        g2_state.bit4_on_timer -= nanosecs;
    }

    if( g2_state.bit0_off_timer <= (int32_t)nanosecs ) {
        g2_state.bit0_off_timer = -1;
    } else {
        g2_state.bit0_off_timer -= nanosecs;
    }
    if( g2_state.bit0_on_timer <= (int32_t)nanosecs ) {
        g2_state.bit0_on_timer = -1;
    } else {
        g2_state.bit0_on_timer -= nanosecs;
    }

    return nanosecs;
}

static void asic_init( void )
{
    register_io_region( &mmio_region_ASIC );
    register_io_region( &mmio_region_EXTDMA );
    asic_reset();
}

static void asic_reset( void )
{
    memset( &g2_state, 0xFF, sizeof(g2_state) );
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


/**
 * Setup the timers for the 3 FIFO status bits following a write through the G2
 * bus from the SH4 side. The timing is roughly as follows: (times are
 * approximate based on software readings - I wouldn't take this as gospel but
 * it seems to be enough to fool most programs). 
 *    0ns: Bit 5 (Input fifo?) goes high immediately on the write
 *   40ns: Bit 5 goes low and bit 4 goes high
 *  120ns: Bit 4 goes low, bit 0 goes high
 *  240ns: Bit 0 goes low.
 *
 * Additional writes while the FIFO is in operation extend the time that the
 * bits remain high as one might expect, without altering the time at which
 * they initially go high.
 */
void asic_g2_write_word()
{
    if( g2_state.bit5_off_timer < (int32_t)sh4r.slice_cycle ) {
        g2_state.bit5_off_timer = sh4r.slice_cycle + G2_BIT5_TICKS;
    } else {
        g2_state.bit5_off_timer += G2_BIT5_TICKS;
    }

    if( g2_state.bit4_on_timer < (int32_t)sh4r.slice_cycle ) {
        g2_state.bit4_on_timer = sh4r.slice_cycle + G2_BIT5_TICKS;
    }

    if( g2_state.bit4_off_timer < (int32_t)sh4r.slice_cycle ) {
        g2_state.bit4_off_timer = g2_state.bit4_on_timer + G2_BIT4_TICKS;
    } else {
        g2_state.bit4_off_timer += G2_BIT4_TICKS;
    }

    if( g2_state.bit0_on_timer < (int32_t)sh4r.slice_cycle ) {
        g2_state.bit0_on_timer = sh4r.slice_cycle + G2_BIT0_ON_TICKS;
    }

    if( g2_state.bit0_off_timer < (int32_t)sh4r.slice_cycle ) {
        g2_state.bit0_off_timer = g2_state.bit0_on_timer + G2_BIT0_OFF_TICKS;
    } else {
        g2_state.bit0_off_timer += G2_BIT0_OFF_TICKS;
    }

    MMIO_WRITE( ASIC, G2STATUS, MMIO_READ(ASIC, G2STATUS) | 0x20 );
}

static uint32_t g2_update_fifo_status( uint32_t nanos )
{
    uint32_t val = MMIO_READ( ASIC, G2STATUS );
    if( ((uint32_t)g2_state.bit5_off_timer) <= nanos ) {
        val = val & (~0x20);
        g2_state.bit5_off_timer = -1;
    }
    if( ((uint32_t)g2_state.bit4_on_timer) <= nanos ) {
        val = val | 0x10;
        g2_state.bit4_on_timer = -1;
    }
    if( ((uint32_t)g2_state.bit4_off_timer) <= nanos ) {
        val = val & (~0x10);
        g2_state.bit4_off_timer = -1;
    } 

    if( ((uint32_t)g2_state.bit0_on_timer) <= nanos ) {
        val = val | 0x01;
        g2_state.bit0_on_timer = -1;
    }
    if( ((uint32_t)g2_state.bit0_off_timer) <= nanos ) {
        val = val & (~0x01);
        g2_state.bit0_off_timer = -1;
    } 

    MMIO_WRITE( ASIC, G2STATUS, val );
    return val;
}   

static int g2_read_status() {
    return g2_update_fifo_status( sh4r.slice_cycle );
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

    if( event >= 64 ) { /* Third word */
        asic_event( EVENT_CASCADE2 );
    } else if( event >= 32 ) { /* Second word */
        asic_event( EVENT_CASCADE1 );
    }
}

void asic_clear_event( int event ) {
    int offset = ((event&0x60)>>3);
    uint32_t result = MMIO_READ(ASIC, PIRQ0 + offset)  & (~(1<<(event&0x1F)));
    MMIO_WRITE( ASIC, PIRQ0 + offset, result );
    if( result == 0 ) {
        /* clear cascades if necessary */
        if( event >= 64 ) {
            MMIO_WRITE( ASIC, PIRQ0, MMIO_READ( ASIC, PIRQ0 ) & 0x7FFFFFFF );
        } else if( event >= 32 ) {
            MMIO_WRITE( ASIC, PIRQ0, MMIO_READ( ASIC, PIRQ0 ) & 0xBFFFFFFF );
        }
    }

    asic_check_cleared_events();
}

void asic_check_cleared_events( )
{
    int i, setA = 0, setB = 0, setC = 0;
    uint32_t bits;
    for( i=0; i<12; i+=4 ) {
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

void asic_event_mask_changed( )
{
    int i, setA = 0, setB = 0, setC = 0;
    uint32_t bits;
    for( i=0; i<12; i+=4 ) {
        bits = MMIO_READ( ASIC, PIRQ0 + i );
        setA |= (bits & MMIO_READ(ASIC, IRQA0 + i ));
        setB |= (bits & MMIO_READ(ASIC, IRQB0 + i ));
        setC |= (bits & MMIO_READ(ASIC, IRQC0 + i ));
    }
    if( setA == 0 ) 
        intc_clear_interrupt( INT_IRQ13 );
    else
        intc_raise_interrupt( INT_IRQ13 );
    if( setB == 0 )
        intc_clear_interrupt( INT_IRQ11 );
    else
        intc_raise_interrupt( INT_IRQ11 );
    if( setC == 0 )
        intc_clear_interrupt( INT_IRQ9 );
    else
        intc_raise_interrupt( INT_IRQ9 );
}

void g2_dma_transfer( int channel )
{
    uint32_t offset = channel << 5;

    if( MMIO_READ( EXTDMA, G2DMA0CTL1 + offset ) == 1 ) {
        if( MMIO_READ( EXTDMA, G2DMA0CTL2 + offset ) == 1 ) {
            uint32_t extaddr = MMIO_READ( EXTDMA, G2DMA0EXT + offset );
            uint32_t sh4addr = MMIO_READ( EXTDMA, G2DMA0SH4 + offset );
            uint32_t length = MMIO_READ( EXTDMA, G2DMA0SIZ + offset ) & 0x1FFFFFFF;
            uint32_t dir = MMIO_READ( EXTDMA, G2DMA0DIR + offset );
            // uint32_t mode = MMIO_READ( EXTDMA, G2DMA0MOD + offset );
            unsigned char buf[length];
            if( dir == 0 ) { /* SH4 to device */
                mem_copy_from_sh4( buf, sh4addr, length );
                mem_copy_to_sh4( extaddr, buf, length );
            } else { /* Device to SH4 */
                mem_copy_from_sh4( buf, extaddr, length );
                mem_copy_to_sh4( sh4addr, buf, length );
            }
            MMIO_WRITE( EXTDMA, G2DMA0CTL2 + offset, 0 );
            asic_event( EVENT_G2_DMA0 + channel );
        } else {
            MMIO_WRITE( EXTDMA, G2DMA0CTL2 + offset, 0 );
        }
    }
}

void asic_ide_dma_transfer( )
{	
    if( MMIO_READ( EXTDMA, IDEDMACTL2 ) == 1 ) {
        if( MMIO_READ( EXTDMA, IDEDMACTL1 ) == 1 ) {
            MMIO_WRITE( EXTDMA, IDEDMATXSIZ, 0 );

            uint32_t addr = MMIO_READ( EXTDMA, IDEDMASH4 );
            uint32_t length = MMIO_READ( EXTDMA, IDEDMASIZ );
            // int dir = MMIO_READ( EXTDMA, IDEDMADIR );

            uint32_t xfer = ide_read_data_dma( addr, length );
            MMIO_WRITE( EXTDMA, IDEDMATXSIZ, xfer );
            MMIO_WRITE( EXTDMA, IDEDMACTL2, 0 );
            asic_event( EVENT_IDE_DMA );            
        } else { /* 0 */
            MMIO_WRITE( EXTDMA, IDEDMACTL2, 0 );
        }
    }
}

void pvr_dma_transfer( )
{
    sh4addr_t destaddr = MMIO_READ( ASIC, PVRDMADEST) &0x1FFFFFE0;
    uint32_t count = MMIO_READ( ASIC, PVRDMACNT );
    unsigned char data[8192];
    uint32_t rcount;

    while( count ) {
        uint32_t chunksize = (count < sizeof(data)) ? count : sizeof(data);
        rcount = DMAC_get_buffer( 2, data, chunksize );
        pvr2_dma_write( destaddr, data, rcount );
        if( destaddr & 0x01000000 ) {
            destaddr += rcount;
        }
        count -= rcount;
        if( rcount != chunksize ) {
            WARN( "PVR received %08X bytes from DMA, expected %08X", rcount, chunksize );
            break;
        }
    }

    MMIO_WRITE( ASIC, PVRDMACTL, 0 );
    MMIO_WRITE( ASIC, PVRDMACNT, 0 );
    if( destaddr & 0x01000000 ) { /* Write to texture RAM */
        MMIO_WRITE( ASIC, PVRDMADEST, destaddr );
    }
    asic_event( EVENT_PVR_DMA );
}

void pvr_dma2_transfer()
{
    if( MMIO_READ( EXTDMA, PVRDMA2CTL2 ) == 1 ) {
        if( MMIO_READ( EXTDMA, PVRDMA2CTL1 ) == 1 ) {
            sh4addr_t extaddr = MMIO_READ( EXTDMA, PVRDMA2EXT );
            sh4addr_t sh4addr = MMIO_READ( EXTDMA, PVRDMA2SH4 );
            int dir = MMIO_READ( EXTDMA, PVRDMA2DIR );
            uint32_t length = MMIO_READ( EXTDMA, PVRDMA2SIZ );
            unsigned char buf[length];
            if( dir == 0 ) { /* SH4 to PVR */
                mem_copy_from_sh4( buf, sh4addr, length );
                mem_copy_to_sh4( extaddr, buf, length );
            } else { /* PVR to SH4 */
                mem_copy_from_sh4( buf, extaddr, length );
                mem_copy_to_sh4( sh4addr, buf, length );
            }
            MMIO_WRITE( EXTDMA, PVRDMA2CTL2, 0 );
            asic_event( EVENT_PVR_DMA2 );
        }
    }
}

void sort_dma_transfer( )
{
    sh4addr_t table_addr = MMIO_READ( ASIC, SORTDMATBL );
    sh4addr_t data_addr = MMIO_READ( ASIC, SORTDMADATA );
    int table_size = MMIO_READ( ASIC, SORTDMATSIZ );
    int addr_shift = MMIO_READ( ASIC, SORTDMAASIZ ) ? 5 : 0;
    int count = 1;

    uint32_t *table32 = (uint32_t *)mem_get_region( table_addr );
    uint16_t *table16 = (uint16_t *)table32;
    uint32_t next = table_size ? (*table32++) : (uint32_t)(*table16++);
    while(1) {
        next &= 0x07FFFFFF;
        if( next == 1 ) {
            next = table_size ? (*table32++) : (uint32_t)(*table16++);
            count++;
            continue;
        } else if( next == 2 ) {
            asic_event( EVENT_SORT_DMA );
            break;
        } 
        uint32_t *data = (uint32_t *)mem_get_region(data_addr + (next<<addr_shift));
        if( data == NULL ) {
            break;
        }

        uint32_t *poly = pvr2_ta_find_polygon_context(data, 128);
        if( poly == NULL ) {
            asic_event( EVENT_SORT_DMA_ERR );
            break;
        }
        uint32_t size = poly[6] & 0xFF;
        if( size == 0 ) {
            size = 0x100;
        }
        next = poly[7];
        pvr2_ta_write( (unsigned char *)data, size<<5 );
    }

    MMIO_WRITE( ASIC, SORTDMACNT, count );
    MMIO_WRITE( ASIC, SORTDMACTL, 0 );
}

void maple_set_dma_state( uint32_t val )
{
    gboolean in_transfer = MMIO_READ( ASIC, MAPLE_STATE ) & 1;
    gboolean transfer_requested = val & 1;
    if( !in_transfer && transfer_requested ) {
        /* Initiate new DMA transfer */
        uint32_t maple_addr = MMIO_READ( ASIC, MAPLE_DMA) &0x1FFFFFE0;
        maple_handle_buffer( maple_addr );
    }
    else if ( in_transfer && !transfer_requested ) {
        /* Cancel current DMA transfer */
        event_cancel( EVENT_MAPLE_DMA );
    }
    MMIO_WRITE( ASIC, MAPLE_STATE, val );
}

gboolean asic_enable_ide_interface( gboolean enable )
{
    gboolean oldval = idereg.interface_enabled;
    idereg.interface_enabled = enable;
    return oldval;
}

MMIO_REGION_READ_FN( ASIC, reg )
{
    int32_t val;
    reg &= 0xFFF;
    switch( reg ) {
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
        return val;            
    case G2STATUS:
        return g2_read_status();
    default:
        val = MMIO_READ(ASIC, reg);
        return val;
    }

}

MMIO_REGION_READ_DEFSUBFNS(ASIC)

MMIO_REGION_WRITE_FN( ASIC, reg, val )
{
    reg &= 0xFFF;
    switch( reg ) {
    case PIRQ1:
        break; /* Treat this as read-only for the moment */
    case PIRQ0:
        val = val & 0x3FFFFFFF; /* Top two bits aren't clearable */
        MMIO_WRITE( ASIC, reg, MMIO_READ(ASIC, reg)&~val );
        asic_check_cleared_events();
        break;
    case PIRQ2:
        /* Clear any events */
        val = MMIO_READ(ASIC, reg)&(~val);
        MMIO_WRITE( ASIC, reg, val );
        if( val == 0 ) { /* all clear - clear the cascade bit */
            MMIO_WRITE( ASIC, PIRQ0, MMIO_READ( ASIC, PIRQ0 ) & 0x7FFFFFFF );
        }
        asic_check_cleared_events();
        break;
    case IRQA0:
    case IRQA1:
    case IRQA2:
    case IRQB0:
    case IRQB1:
    case IRQB2:
    case IRQC0:
    case IRQC1:
    case IRQC2:
        MMIO_WRITE( ASIC, reg, val );
        asic_event_mask_changed();
        break;
    case SYSRESET:
        if( val == 0x7611 ) {
            dreamcast_reset();
        } else {
            WARN( "Unknown value %08X written to SYSRESET port", val );
        }
        break;
    case MAPLE_STATE:
        maple_set_dma_state( val );
        break;
    case PVRDMADEST:
        MMIO_WRITE( ASIC, reg, (val & 0x03FFFFE0) | 0x10000000 );
        break;
    case PVRDMACNT: 
        MMIO_WRITE( ASIC, reg, val & 0x00FFFFE0 );
        break;
    case PVRDMACTL: /* Initiate PVR DMA transfer */
        val = val & 0x01;
        MMIO_WRITE( ASIC, reg, val );
        if( val == 1 ) {
            pvr_dma_transfer();
        }
        break;
    case SORTDMATBL: case SORTDMADATA:
        MMIO_WRITE( ASIC, reg, (val & 0x0FFFFFE0) | 0x08000000 );
        break;
    case SORTDMATSIZ: case SORTDMAASIZ:
        MMIO_WRITE( ASIC, reg, (val & 1) );
        break;
    case SORTDMACTL:
        val = val & 1;
        MMIO_WRITE( ASIC, reg, val );
        if( val == 1 ) {
            sort_dma_transfer();
        }
        break;
    case MAPLE_DMA:
        MMIO_WRITE( ASIC, reg, val );
        break;
    default:
        MMIO_WRITE( ASIC, reg, val );
    }
}

MMIO_REGION_READ_FN( EXTDMA, reg )
{
    uint32_t val;
    reg &= 0xFFF;
    if( !idereg.interface_enabled && IS_IDE_REGISTER(reg) ) {
        return 0xFFFFFFFF; /* disabled */
    }

    switch( reg ) {
    case IDEALTSTATUS: 
        val = idereg.status;
        return val;
    case IDEDATA: return ide_read_data_pio( );
    case IDEFEAT: return idereg.error;
    case IDECOUNT:return idereg.count;
    case IDELBA0: return ide_get_drive_status();
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
MMIO_REGION_READ_DEFSUBFNS(EXTDMA)


MMIO_REGION_WRITE_FN( EXTDMA, reg, val )
{
    reg &= 0xFFF;
    if( !idereg.interface_enabled && IS_IDE_REGISTER(reg) ) {
        return; /* disabled */
    }

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
        if( ide_can_write_regs() || val == IDE_CMD_NOP ) {
            ide_write_command( (uint8_t)val );
        }
        break;
    case IDEDMASH4:
        MMIO_WRITE( EXTDMA, reg, val & 0x1FFFFFE0 );
        break;
    case IDEDMASIZ:
        MMIO_WRITE( EXTDMA, reg, val & 0x01FFFFFE );
        break;
    case IDEDMADIR:
        MMIO_WRITE( EXTDMA, reg, val & 1 );
        break;
    case IDEDMACTL1:
    case IDEDMACTL2:
        MMIO_WRITE( EXTDMA, reg, val & 0x01 );
        asic_ide_dma_transfer( );
        break;
    case IDEACTIVATE:
        if( val == 0x001FFFFF ) {
            idereg.interface_enabled = TRUE;
            /* Conventional wisdom says that this is necessary but not
             * sufficient to enable the IDE interface.
             */
        } else if( val == 0x000042FE ) {
            idereg.interface_enabled = FALSE;
        }
        break;
    case G2DMA0EXT: case G2DMA0SH4: case G2DMA0SIZ:
    case G2DMA1EXT: case G2DMA1SH4: case G2DMA1SIZ:
    case G2DMA2EXT: case G2DMA2SH4: case G2DMA2SIZ:
    case G2DMA3EXT: case G2DMA3SH4: case G2DMA3SIZ:
        MMIO_WRITE( EXTDMA, reg, val & 0x9FFFFFE0 );
        break;
    case G2DMA0MOD: case G2DMA1MOD: case G2DMA2MOD: case G2DMA3MOD:
        MMIO_WRITE( EXTDMA, reg, val & 0x07 );
        break;
    case G2DMA0DIR: case G2DMA1DIR: case G2DMA2DIR: case G2DMA3DIR:
        MMIO_WRITE( EXTDMA, reg, val & 0x01 );
        break;
    case G2DMA0CTL1:
    case G2DMA0CTL2:
        MMIO_WRITE( EXTDMA, reg, val & 1);
        g2_dma_transfer( 0 );
        break;
    case G2DMA0STOP:
        MMIO_WRITE( EXTDMA, reg, val & 0x37 );
        break;
    case G2DMA1CTL1:
    case G2DMA1CTL2:
        MMIO_WRITE( EXTDMA, reg, val & 1);
        g2_dma_transfer( 1 );
        break;

    case G2DMA1STOP:
        MMIO_WRITE( EXTDMA, reg, val & 0x37 );
        break;
    case G2DMA2CTL1:
    case G2DMA2CTL2:
        MMIO_WRITE( EXTDMA, reg, val &1 );
        g2_dma_transfer( 2 );
        break;
    case G2DMA2STOP:
        MMIO_WRITE( EXTDMA, reg, val & 0x37 );
        break;
    case G2DMA3CTL1:
    case G2DMA3CTL2:
        MMIO_WRITE( EXTDMA, reg, val &1 );
        g2_dma_transfer( 3 );
        break;
    case G2DMA3STOP:
        MMIO_WRITE( EXTDMA, reg, val & 0x37 );
        break;
    case PVRDMA2CTL1:
    case PVRDMA2CTL2:
        MMIO_WRITE( EXTDMA, reg, val & 1 );
        pvr_dma2_transfer();
        break;
    default:
        MMIO_WRITE( EXTDMA, reg, val );
    }
}

