/**
 * $Id$
 * SCIF (Serial Communication Interface with FIFO) implementation - part of the 
 * SH4 standard on-chip peripheral set. The SCIF is hooked up to the DCs
 * external serial port
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

#include <glib.h>
#include "dream.h"
#include "mem.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#include "sh4/intc.h"
#include "sh4/dmac.h"
#include "clock.h"
#include "serial.h"

void SCIF_set_break(void);
void SCIF_run_to(uint32_t nanosecs);
/************************* External serial interface ************************/

/**
 * Note: serial_* operations are called from outside the SH4, and as such are
 * named relative to the external serial device. SCIF_* operations are only
 * called internally to the SH4 and so are named relative to the CPU.
 */

/**
 * Storage space for inbound/outbound data blocks. It's a little more
 * convenient for serial consumers to be able to deal with block-sized pieces
 * rather than a byte at a time, even if it makes all this look rather
 * complicated.
 *
 * Currently there's no limit on the number of blocks that can be queued up.
 */
typedef struct serial_data_block {
    uint32_t length;
    uint32_t offset;
    struct serial_data_block *next;
    char data[];
} *serial_data_block_t;

serial_data_block_t serial_recvq_head = NULL, serial_recvq_tail = NULL;
serial_device_t serial_device = NULL;

serial_device_t serial_get_device( )
{
    return serial_device;
}

serial_device_t serial_attach_device( serial_device_t dev ) 
{
    serial_device_t olddev = serial_device;
    if( serial_device != NULL )
        serial_detach_device();
    serial_device = dev;
    if( serial_device != NULL && serial_device->attach != NULL )
        serial_device->attach(serial_device);
    return olddev;
}


serial_device_t serial_detach_device( void )
{
    serial_device_t dev = serial_device;
    if( serial_device != NULL && serial_device->detach != NULL ) {
        serial_device->detach(serial_device);
    }
    serial_device = NULL;
    return dev;
}

void serial_destroy_device( serial_device_t dev )
{
    if( dev != NULL ) {
        if( serial_device == dev )
            serial_detach_device();
        if( dev->destroy )
            dev->destroy(dev);
    }
}

/**
 * Add a block of data to the serial receive queue. The data will be received
 * by the CPU at the appropriate baud rate.
 */
void serial_transmit_data( char *data, int length ) {
    if( length == 0 )
        return;
    serial_data_block_t block = 
        g_malloc( sizeof( struct serial_data_block ) + length );
    block->length = length;
    block->offset = 0;
    block->next = NULL;
    memcpy( block->data, data, length );

    if( serial_recvq_head == NULL ) {
        serial_recvq_head = serial_recvq_tail = block;
    } else {
        serial_recvq_tail->next = block;
        serial_recvq_tail = block;
    }
}

/**
 * Dequeue a byte from the serial input queue
 */
static int serial_transmit_dequeue( ) {
    if( serial_recvq_head != NULL ) {
        uint8_t val = serial_recvq_head->data[serial_recvq_head->offset++];
        if( serial_recvq_head->offset >= serial_recvq_head->length ) {
            serial_data_block_t next = serial_recvq_head->next;
            g_free( serial_recvq_head );
            serial_recvq_head = next;
            if( next == NULL )
                serial_recvq_tail = NULL;
        }
        return (int)(unsigned int)val;
    }
    return -1;

}

void serial_transmit_break() {
    SCIF_set_break();
}

/********************************* SCIF *************************************/

#define FIFO_LENGTH 16
#define FIFO_ARR_LENGTH (FIFO_LENGTH+1)

/* Serial control register flags */
#define SCSCR2_TIE  0x80
#define SCSCR2_RIE  0x40
#define SCSCR2_TE   0x20
#define SCSCR2_RE   0x10
#define SCSCR2_REIE 0x08
#define SCSCR2_CKE 0x02

#define IS_TRANSMIT_IRQ_ENABLED() (MMIO_READ(SCIF,SCSCR2) & SCSCR2_TIE)
#define IS_RECEIVE_IRQ_ENABLED() (MMIO_READ(SCIF,SCSCR2) & SCSCR2_RIE)
#define IS_RECEIVE_ERROR_IRQ_ENABLED() (MMIO_READ(SCIF,SCSCR2) & (SCSCR2_RIE|SCSCR2_REIE))
/* Receive is enabled if the RE bit is set in SCSCR2, and the ORER bit is cleared in SCLSR2 */
#define IS_RECEIVE_ENABLED() ( (MMIO_READ(SCIF,SCSCR2) & SCSCR2_RE) && ((MMIO_READ(SCIF,SCLSR2) & SCLSR2_ORER) == 0) )
/* Transmit is enabled if the TE bit is set in SCSCR2 */
#define IS_TRANSMIT_ENABLED() (MMIO_READ(SCIF,SCSCR2) & SCSCR2_TE)
#define IS_LOOPBACK_ENABLED() (MMIO_READ(SCIF,SCFCR2) & SCFCR2_LOOP)

/* Serial status register flags */
#define SCFSR2_ER   0x80
#define SCFSR2_TEND 0x40
#define SCFSR2_TDFE 0x20
#define SCFSR2_BRK  0x10
#define SCFSR2_RDF  0x02
#define SCFSR2_DR   0x01

/* FIFO control register flags */
#define SCFCR2_MCE   0x08
#define SCFCR2_TFRST 0x04
#define SCFCR2_RFRST 0x02
#define SCFCR2_LOOP  0x01

/* Line Status Register */
#define SCLSR2_ORER 0x01

struct SCIF_fifo {
    int head;
    int tail;
    int trigger;
    uint8_t data[FIFO_ARR_LENGTH];
};

int SCIF_recvq_triggers[4] = {1, 4, 8, 14};
struct SCIF_fifo SCIF_recvq = {0,0,1};

int SCIF_sendq_triggers[4] = {8, 4, 2, 1};
struct SCIF_fifo SCIF_sendq = {0,0,8};

/**
 * Flag to indicate if data was received (ie added to the receive queue)
 * during the last SCIF clock tick. Used to determine when to set the DR
 * flag.
 */
gboolean SCIF_rcvd_last_tick = FALSE;

uint32_t SCIF_tick_period = 0;
uint32_t SCIF_tick_remainder = 0;
uint32_t SCIF_slice_cycle = 0;

void SCIF_save_state( FILE *f ) 
{
    fwrite( &SCIF_recvq, sizeof(SCIF_recvq), 1, f );
    fwrite( &SCIF_sendq, sizeof(SCIF_sendq), 1, f );
    fwrite( &SCIF_rcvd_last_tick, sizeof(gboolean), 1, f );

}

int SCIF_load_state( FILE *f ) 
{
    fread( &SCIF_recvq, sizeof(SCIF_recvq), 1, f );
    fread( &SCIF_sendq, sizeof(SCIF_sendq), 1, f );
    fread( &SCIF_rcvd_last_tick, sizeof(gboolean), 1, f );
    return 0;
}

static inline uint8_t SCIF_recvq_size( ) 
{
    int val = SCIF_recvq.tail - SCIF_recvq.head;
    if( val < 0 ) {
        val = FIFO_ARR_LENGTH - SCIF_recvq.head + SCIF_recvq.tail;
    }
    return val;
}

int SCIF_recvq_dequeue( gboolean clearFlags )
{
    uint8_t result;
    uint32_t tmp, length;
    if( SCIF_recvq.head == SCIF_recvq.tail )
        return -1; /* No data */
    result = SCIF_recvq.data[SCIF_recvq.head++];
    if( SCIF_recvq.head > FIFO_LENGTH )
        SCIF_recvq.head = 0;

    /* Update data count register */
    tmp = MMIO_READ( SCIF, SCFDR2 ) & 0xF0;
    length = SCIF_recvq_size();
    MMIO_WRITE( SCIF, SCFDR2, tmp | length );

    /* Clear flags (if requested ) */
    if( clearFlags && length < SCIF_recvq.trigger ) {
        tmp = SCFSR2_RDF;
        if( length == 0 )
            tmp |= SCFSR2_DR;
        tmp = MMIO_READ( SCIF, SCFSR2 ) & (~tmp);
        MMIO_WRITE( SCIF, SCFSR2, tmp );
        /* If both flags are cleared, clear the interrupt as well */
        if( (tmp & (SCFSR2_DR|SCFSR2_RDF)) == 0 && IS_RECEIVE_IRQ_ENABLED() )
            intc_clear_interrupt( INT_SCIF_RXI );
    }

    return (int)(unsigned int)result;
}

gboolean SCIF_recvq_enqueue( uint8_t value )
{
    uint32_t tmp, length;
    int newpos = SCIF_recvq.tail + 1;
    if( newpos > FIFO_LENGTH )
        newpos = 0;
    if( newpos == SCIF_recvq.head ) {
        /* FIFO full - set ORER and discard the value */
        MMIO_WRITE( SCIF, SCLSR2, SCLSR2_ORER );
        if( IS_RECEIVE_ERROR_IRQ_ENABLED() )
            intc_raise_interrupt( INT_SCIF_ERI );
        return FALSE;
    }
    SCIF_recvq.data[SCIF_recvq.tail] = value;

    /* Update data count register */
    tmp = MMIO_READ( SCIF, SCFDR2 ) & 0xF0;
    length = SCIF_recvq_size();
    MMIO_WRITE( SCIF, SCFDR2, tmp | length );

    /* Update status register */
    tmp = MMIO_READ( SCIF, SCFSR2 );
    if( length >= SCIF_recvq.trigger ) {
        tmp |= SCFSR2_RDF;
        if( IS_RECEIVE_IRQ_ENABLED() ) 
            intc_raise_interrupt( INT_SCIF_RXI );
        DMAC_trigger( DMAC_SCIF_RDF );
    }
    MMIO_WRITE( SCIF, SCFSR2, tmp );
    return TRUE;
}


/**
 * Reset the receive FIFO to its initial state. Manual is unclear as to
 * whether this also clears flags/interrupts, but we're assuming here that
 * it does until proven otherwise.
 */
void SCIF_recvq_clear( void ) 
{
    SCIF_recvq.head = SCIF_recvq.tail = 0;
    MMIO_WRITE( SCIF, SCFDR2, MMIO_READ( SCIF, SCFDR2 ) & 0xF0 );
    MMIO_WRITE( SCIF, SCFSR2, MMIO_READ( SCIF, SCFSR2 ) & ~(SCFSR2_DR|SCFSR2_RDF) );
    if( IS_RECEIVE_IRQ_ENABLED() )
        intc_clear_interrupt( INT_SCIF_RXI );
}

static inline uint8_t SCIF_sendq_size( ) 
{
    int val = SCIF_sendq.tail - SCIF_sendq.head;
    if( val < 0 ) {
        val = FIFO_ARR_LENGTH - SCIF_sendq.head + SCIF_sendq.tail;
    }
    return val;
}

/**
 * Dequeue one byte from the SCIF transmit queue (ie transmit the byte),
 * updating all status flags as required.
 * @return The byte dequeued, or -1 if the queue is empty.
 */
int SCIF_sendq_dequeue( )
{
    uint8_t result;
    uint32_t tmp, length;
    if( SCIF_sendq.head == SCIF_sendq.tail )
        return -1; /* No data */

    /* Update queue head pointer */
    result = SCIF_sendq.data[SCIF_sendq.head++];
    if( SCIF_sendq.head > FIFO_LENGTH )
        SCIF_sendq.head = 0;

    /* Update data count register */
    tmp = MMIO_READ( SCIF, SCFDR2 ) & 0x0F;
    length = SCIF_sendq_size();
    MMIO_WRITE( SCIF, SCFDR2, tmp | (length << 8) );

    /* Update status register */
    if( length <= SCIF_sendq.trigger ) {
        tmp = MMIO_READ( SCIF, SCFSR2 ) | SCFSR2_TDFE;
        if( length == 0 )
            tmp |= SCFSR2_TEND; /* Transmission ended - no data waiting */
        if( IS_TRANSMIT_IRQ_ENABLED() ) 
            intc_raise_interrupt( INT_SCIF_TXI );
        DMAC_trigger( DMAC_SCIF_TDE );
        MMIO_WRITE( SCIF, SCFSR2, tmp );
    }
    return (int)(unsigned int)result;
}

/**
 * Enqueue a single byte in the SCIF transmit queue. If the queue is full,
 * the value will be discarded.
 * @param value to be queued.
 * @param clearFlags TRUE if the TEND/TDFE flags should be cleared
 *   if the queue exceeds the trigger level. (According to the manual,
 *   DMAC writes will clear the flag, whereas regular SH4 writes do NOT
 *   automatically clear it. Go figure).
 * @return gboolean TRUE if the value was queued, FALSE if the queue was
 *   full.
 */
gboolean SCIF_sendq_enqueue( uint8_t value, gboolean clearFlags )
{
    uint32_t tmp, length;
    int newpos = SCIF_sendq.tail + 1;
    if( newpos > FIFO_LENGTH )
        newpos = 0;
    if( newpos == SCIF_sendq.head ) {
        /* FIFO full - discard */
        return FALSE;
    }
    SCIF_sendq.data[SCIF_sendq.tail] = value;
    SCIF_sendq.tail = newpos;

    /* Update data count register */
    tmp = MMIO_READ( SCIF, SCFDR2 ) & 0x0F;
    length = SCIF_sendq_size();
    MMIO_WRITE( SCIF, SCFDR2, tmp | (length << 8) );

    /* Update flags if requested */
    if( clearFlags ) {
        tmp = SCFSR2_TEND;
        if( length > SCIF_sendq.trigger ) {
            tmp |= SCFSR2_TDFE;
            if( IS_TRANSMIT_IRQ_ENABLED() )
                intc_clear_interrupt( INT_SCIF_TXI );
        }
        tmp = MMIO_READ( SCIF, SCFSR2 ) & (~tmp);
        MMIO_WRITE( SCIF, SCFSR2, tmp );
    }
    return TRUE;
}

void SCIF_sendq_clear( void ) 
{
    SCIF_sendq.head = SCIF_sendq.tail = 0;
    MMIO_WRITE( SCIF, SCFDR2, MMIO_READ( SCIF, SCFDR2 ) & 0x0F );
    MMIO_WRITE( SCIF, SCFSR2, MMIO_READ( SCIF, SCFSR2 ) | SCFSR2_TEND | SCFSR2_TDFE );
    if( IS_TRANSMIT_IRQ_ENABLED() ) {
        intc_raise_interrupt( INT_SCIF_TXI );
        DMAC_trigger( DMAC_SCIF_TDE );
    }
}

/**
 * Update the SCFSR2 status register with the given mask (ie clear any values
 * that are set to 0 in the mask. According to a strict reading of the doco
 * though, the bits will only actually clear if the flag state is no longer
 * true, so we need to recheck everything...
 */
void SCIF_update_status( uint32_t mask )
{
    uint32_t value = MMIO_READ( SCIF, SCFSR2 );
    uint32_t result = value & mask;
    uint32_t sendq_size = SCIF_sendq_size();
    uint32_t recvq_size = SCIF_recvq_size();

    if( sendq_size != 0 )
        result |= SCFSR2_TEND;

    if( sendq_size <= SCIF_sendq.trigger )
        result |= SCFSR2_TDFE;
    else if( (result & SCFSR2_TDFE) == 0 && IS_TRANSMIT_IRQ_ENABLED() )
        intc_clear_interrupt( INT_SCIF_TXI );

    if( recvq_size >= SCIF_recvq.trigger )
        result |= SCFSR2_RDF;
    if( (value & SCFSR2_DR) != 0 && (result & SCFSR2_DR) == 0 &&
            recvq_size != 0 )
        result |= SCFSR2_DR;
    if( (result & (SCFSR2_DR|SCFSR2_RDF)) == 0 && IS_RECEIVE_IRQ_ENABLED() )
        intc_clear_interrupt( INT_SCIF_RXI );

    if( IS_RECEIVE_ERROR_IRQ_ENABLED() ) {
        if( (result & SCFSR2_BRK) == 0 )
            intc_clear_interrupt( INT_SCIF_BRI );
        if( (result & SCFSR2_ER) == 0 && 
                (MMIO_READ( SCIF, SCLSR2 ) & SCLSR2_ORER) == 0 )
            intc_clear_interrupt( INT_SCIF_ERI );
    }
}

/**
 * Set the break detected flag
 */
void SCIF_set_break( void ) 
{
    MMIO_WRITE( SCIF, SCFSR2, MMIO_READ( SCIF, SCFSR2 ) | SCFSR2_BRK );
    if( IS_RECEIVE_ERROR_IRQ_ENABLED() )
        intc_raise_interrupt( INT_SCIF_BRI );
}

const static int SCIF_CLOCK_MULTIPLIER[4] = {1, 4, 16, 64};

/**
 * Calculate the current line speed.
 */
void SCIF_update_line_speed( void )
{
    /* If CKE1 is set, use the external clock as a base */
    if( MMIO_READ( SCIF, SCSCR2 ) & SCSCR2_CKE ) {


    } else {

        /* Otherwise, SH4 peripheral clock divided by n */
        int mult = SCIF_CLOCK_MULTIPLIER[MMIO_READ( SCIF, SCSMR2 ) & 0x03];

        /* Then process the bitrate register */
        int bbr = MMIO_READ( SCIF, SCBRR2 ) & 0xFF;

        int baudrate = sh4_peripheral_freq / (32 * mult * (bbr+1) );

        if( serial_device != NULL && serial_device->set_line_speed != NULL )
            serial_device->set_line_speed( serial_device, baudrate );

        SCIF_tick_period = sh4_peripheral_period * (32 * mult * (bbr+1));

        /*
	  clock_set_tick_rate( CLOCK_SCIF, baudrate / 10 );
         */
    }
}

MMIO_REGION_READ_FN( SCIF, reg )
{
    SCIF_run_to(sh4r.slice_cycle);
    reg &= 0xFFF;
    switch( reg ) {
    case SCFRDR2: /* Receive data */
        return SCIF_recvq_dequeue(FALSE);
    default:
        return MMIO_READ( SCIF, reg );
    }
}
MMIO_REGION_READ_DEFSUBFNS(SCIF)


MMIO_REGION_WRITE_FN( SCIF, reg, val )
{
    SCIF_run_to(sh4r.slice_cycle);
    uint32_t tmp;
    reg &= 0xFFF;
    switch( reg ) {
    case SCSMR2: /* Serial mode register */
        /* Bit 6 => 0 = 8-bit, 1 = 7-bit
         * Bit 5 => 0 = Parity disabled, 1 = parity enabled
         * Bit 4 => 0 = Even parity, 1 = Odd parity
         * Bit 3 => 0 = 1 stop bit, 1 = 2 stop bits
         * Bits 0-1 => Clock select 00 = P, 01 = P/4, 10 = P/16, 11 = P/64
         */
        val &= 0x007B;
        if( serial_device != NULL ) {
            serial_device->set_line_params( serial_device, val );
        }
        tmp = MMIO_READ( SCIF, SCSMR2 );
        if( (tmp & 0x03) != (val & 0x03) ) {
            /* Clock change */
            SCIF_update_line_speed( );
        }
        /* Save for later read-back */
        MMIO_WRITE( SCIF, SCSMR2, val );
        break;
    case SCBRR2: /* Bit rate register */
        MMIO_WRITE( SCIF, SCBRR2, val );
        SCIF_update_line_speed( );
        break;
    case SCSCR2: /* Serial control register */
        /* Bit 7 => Transmit-FIFO-data-empty interrupt enabled 
         * Bit 6 => Receive-data-full interrupt enabled 
         * Bit 5 => Transmit enable 
         * Bit 4 => Receive enable 
         * Bit 3 => Receive-error/break interrupt enabled
         * Bit 1 => Clock enable
         */
        val &= 0x00FA;
        /* Clear any interrupts that just became disabled */
        if( (val & SCSCR2_TIE) == 0 )
            intc_clear_interrupt( INT_SCIF_TXI );
        if( (val & SCSCR2_RIE) == 0 )
            intc_clear_interrupt( INT_SCIF_RXI );
        if( (val & (SCSCR2_RIE|SCSCR2_REIE)) == 0 ) {
            intc_clear_interrupt( INT_SCIF_ERI );
            intc_clear_interrupt( INT_SCIF_BRI );
        }

        MMIO_WRITE( SCIF, reg, val );
        break;
    case SCFTDR2: /* Transmit FIFO data register */
        SCIF_sendq_enqueue( val, FALSE );
        break;
    case SCFSR2: /* Serial status register */
        /* Bits 12-15 Parity error count
         * Bits 8-11 Framing erro count 
         * Bit 7 - Receive error
         * Bit 6 - Transmit end
         * Bit 5 - Transmit FIFO data empty
         * Bit 4 - Break detect
         * Bit 3 - Framing error
         * Bit 2 - Parity error
         * Bit 1 - Receive FIFO data full
         * Bit 0 - Receive data ready
         */
        /* Clear off any flags/interrupts that are being set to 0 */
        SCIF_update_status( val );
        break;
    case SCFCR2: /* FIFO control register */
        val &= 0x0F;
        SCIF_recvq.trigger = SCIF_recvq_triggers[val >> 6];
        SCIF_sendq.trigger = SCIF_sendq_triggers[(val >> 4) & 0x03];
        if( val & SCFCR2_TFRST ) {
            SCIF_sendq_clear();
        }
        if( val & SCFCR2_RFRST ) {
            SCIF_recvq_clear();
        }

        MMIO_WRITE( SCIF, reg, val );
        break;
    case SCSPTR2: /* Serial Port Register */
        MMIO_WRITE( SCIF, reg, val );
        /* NOT IMPLEMENTED - 'direct' serial I/O */
        if( val != 0 ) {
            WARN( "SCSPTR2 not implemented: Write %08X", val );
        }
        break;
    case SCLSR2:
        val = val & SCLSR2_ORER;
        if( val == 0 ) {
            MMIO_WRITE( SCIF, SCLSR2, val );
            if( (MMIO_READ( SCIF, SCFSR2 ) & SCFSR2_ER) == 0 &&
                    IS_RECEIVE_ERROR_IRQ_ENABLED() ) 
                intc_clear_interrupt( INT_SCIF_ERI );
        }

        break;
    }
}

/**
 * Actions for a single tick of the serial clock, defined as the transmission
 * time of a single frame.
 *
 * If transmit queue is non-empty:
 *    Transmit one byte and remove from queue
 * If input receive source is non-empty:
 *    Transfer one byte to the receive queue (if queue is full, byte is lost)
 * If recvq is non-empty, less than the trigger level, and no data has been
 *    received in the last 2 ticks (including this one), set the DR flag and
 *    IRQ if appropriate.
 */
void SCIF_clock_tick( void ) 
{
    gboolean rcvd = FALSE;

    if( IS_LOOPBACK_ENABLED() ) {
        if( IS_TRANSMIT_ENABLED() ) {
            int val = SCIF_sendq_dequeue();
            if( val != -1 && IS_RECEIVE_ENABLED() ) {
                SCIF_recvq_enqueue( val );
                rcvd = TRUE;
            }
        }
    } else {
        if( IS_TRANSMIT_ENABLED() ) {
            int val = SCIF_sendq_dequeue();
            if( val != -1 && serial_device != NULL && 
                    serial_device->receive_data != NULL ) {
                serial_device->receive_data( serial_device, val );
            }
        }

        if( IS_RECEIVE_ENABLED() ) {
            int val = serial_transmit_dequeue();
            if( val != -1 ) {
                SCIF_recvq_enqueue( val );
                rcvd = TRUE;
            }
        }
    }

    /* Check if we need to set the DR flag */
    if( !rcvd && !SCIF_rcvd_last_tick &&
            SCIF_recvq.head != SCIF_recvq.tail &&
            SCIF_recvq_size() < SCIF_recvq.trigger ) {
        uint32_t tmp = MMIO_READ( SCIF, SCFSR2 );
        if( (tmp & SCFSR2_DR) == 0 ) {
            MMIO_WRITE( SCIF, SCFSR2, tmp | SCFSR2_DR );
            if( IS_RECEIVE_IRQ_ENABLED() )
                intc_raise_interrupt( INT_SCIF_RXI );
            DMAC_trigger( DMAC_SCIF_RDF );
        }
    }
    SCIF_rcvd_last_tick = rcvd;
}

void SCIF_reset( void )
{
    SCIF_recvq_clear();
    SCIF_sendq_clear();
    SCIF_update_line_speed();
}

void SCIF_run_to( uint32_t nanosecs )
{
    SCIF_tick_remainder += nanosecs - SCIF_slice_cycle;
    while( SCIF_tick_remainder >= SCIF_tick_period ) {
        SCIF_tick_remainder -= SCIF_tick_period;
        SCIF_clock_tick();
    }
}

void SCIF_run_slice( uint32_t nanosecs )
{
    SCIF_run_to(nanosecs);
    SCIF_slice_cycle = 0;
}
