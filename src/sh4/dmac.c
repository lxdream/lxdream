/**
 * $Id$
 * 
 * SH4 onboard DMA controller (DMAC) peripheral.
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
#define MODULE sh4_module

#include "dream.h"
#include "mem.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#include "sh4/intc.h"
#include "sh4/dmac.h"

static int DMAC_xfer_size[8] = {8, 1, 2, 4, 32, 1, 1, 1};

/* Control flags */
#define CHCR_IE 0x04  /* Interrupt Enable */
#define CHCR_TE 0x02  /* Transfer End */
#define CHCR_DE 0x01  /* DMAC Enable */

#define IS_DMAC_ENABLED() ((MMIO_READ(DMAC,DMAOR)&0x07) == 0x01)

#define IS_AUTO_REQUEST(val) ((val & 0x0C00) == 0x0400)
#define CHANNEL_RESOURCE(val) ((val >> 8) & 0x0F)

#define DMA_SOURCE(chan) MMIO_READ(DMAC, SAR0 + (chan<<4))
#define DMA_DEST(chan) MMIO_READ(DMAC, DAR0 + (chan<<4))
#define DMA_COUNT(chan) (MMIO_READ(DMAC, DMATCR0 + (chan<<4)) & 0x00FFFFFF)
#define DMA_CONTROL(chan) MMIO_READ(DMAC, CHCR0 + (chan<<4))
#define IS_CHANNEL_ENABLED(ctrl) ((ctrl & 0x03) == 0x01)
#define IS_CHANNEL_IRQ_ENABLED(ctrl) (ctrl & CHCR_IE)
#define IS_CHANNEL_IRQ_ACTIVE(ctrl) ((ctrl & (CHCR_IE|CHCR_TE)) == (CHCR_IE|CHCR_TE))

#define DMARES_MEMORY_TO_MEMORY 0x00
#define DMARES_MEMORY_TO_DEVICE 0x02
#define DMARES_DEVICE_TO_MEMORY 0x03
#define DMARES_MEMORY_TO_MEMORY_AUTO 0x04
#define DMARES_MEMORY_TO_PERIPH_AUTO 0x05
#define DMARES_PERIPH_TO_MEMORY_AUTO 0x06
#define DMARES_SCI_TRANSMIT_EMPTY 0x08
#define DMARES_SCI_RECEIVE_FULL 0x09
#define DMARES_SCIF_TRANSMIT_EMPTY 0x0A
#define DMARES_SCIF_RECEIVE_FULL 0x0B
#define DMARES_MEMORY_TO_MEMORY_TMU 0x0C
#define DMARES_MEMORY_TO_PERIPH_TMU 0x0D
#define DMARES_PERIPH_TO_MEMORY_TMU 0x0E

void DMAC_set_control( uint32_t channel, uint32_t val ) 
{
    uint32_t oldval = DMA_CONTROL(channel);
    int resource;
    MMIO_WRITE( DMAC, CHCR0 + (channel<<4), val );
    
    /* If TE or IE are cleared, clear the interrupt request */
    if( IS_CHANNEL_IRQ_ACTIVE(oldval) &&
	!IS_CHANNEL_IRQ_ACTIVE(val) )
	intc_clear_interrupt( INT_DMA_DMTE0+channel );
    
    resource = CHANNEL_RESOURCE(val);
    if( IS_CHANNEL_ENABLED(val) ) {
	if( resource >= DMARES_MEMORY_TO_MEMORY_AUTO && 
	    resource < DMARES_SCI_TRANSMIT_EMPTY ) {
	    /* Autorun */
	}
    }

    /* Everything else we don't need to care about until we actually try to
     * run the channel
     */
}

int32_t mmio_region_DMAC_read( uint32_t reg )
{
    return MMIO_READ( DMAC, reg );
}

void mmio_region_DMAC_write( uint32_t reg, uint32_t val ) 
{
    switch( reg ) {
    case DMAOR:
	MMIO_WRITE( DMAC, reg, val );
	break;
    case CHCR0: DMAC_set_control( 0, val ); break;
    case CHCR1: DMAC_set_control( 1, val ); break;
    case CHCR2: DMAC_set_control( 2, val ); break;
    case CHCR3: DMAC_set_control( 3, val ); break;
    default:
	MMIO_WRITE( DMAC, reg, val );
    }
}

/**
 * Execute up to run_count transfers on the specified channel. Assumes the
 * trigger for the channel has been received.
 *
 * @param channel Channel number (0-3) to run.
 * @param run_count number of transfers to execute, or 0 to run to the 
 * end of the transfer count.
 */
void DMAC_run_channel( uint32_t channel, uint32_t run_count )
{

#if 0 /* Should really finish this */
    char burst[32]; /* Transfer burst */
    uint32_t control = DMA_CONTROL(channel);

    if( IS_CHANNEL_ENABLED(control) ) {
	uint32_t source = DMA_SOURCE(channel);
	uint32_t dest = DMA_DEST(channel);
	uint32_t count = DMA_COUNT( channel );
	if( count == 0 )
	    count = 0x01000000;
	if( run_count == 0 || run_count > count )
	    run_count = count;
	uint32_t xfersize = DMAC_xfer_size[ (control >> 4)&0x07 ];
	int source_step, dest_step;
	int resource = (control >> 8) & 0x0F;
	switch( (control >> 14) & 0x03 ) {
	case 0: dest_step = 0; break;
	case 1: dest_step = xfersize; break;
	case 2: dest_step = -xfersize; break;
	case 3: dest_step = 0; break; /* Illegal */
	}
	switch( (control >> 12) & 0x03 ) {
	case 0: source_step = 0; break;
	case 1: source_step = xfersize; break;
	case 2: source_step = -xfersize; break;
	case 3: source_step = 0; break; /* Illegal */
	}
	
	while( run_count > 0 ) {
	    /* Origin */
	    if( (resource & 0x02) == 0 ) {
		/* Source is a normal memory address */
		
	    } else {
		/* Device */
	    }
	    
	    /* Destination */
	    if( (resource & 0x01) == 0 ) {
		/* Destination is a normal memory address */
	    } else {
	    }
	    run_count--; 
	    count--;
	}
    }
#endif
}

/**
 * Fetch a block of data by DMA from memory to an external device (ie the
 * ASIC). The DMA channel must be configured for Mem=>dev or it will return
 * no bytes and whinge mightily. Note this is NOT used for SH4 peripheral
 * transfers.
 *
 * @return the number of bytes actually transferred.
 */
uint32_t DMAC_get_buffer( int channel, sh4ptr_t buf, uint32_t numBytes )
{
    uint32_t control = DMA_CONTROL(channel);
    uint32_t source, count, run_count, size, i;
    char tmp[32];

    if( !IS_CHANNEL_ENABLED(control) || !IS_DMAC_ENABLED() )
	return 0;
    
    if( ((control >> 8) & 0x0F) !=  DMARES_MEMORY_TO_DEVICE ) {
	/* Error? */
	
	return 0;
    } 
    
    source = DMA_SOURCE(channel);
    count = DMA_COUNT(channel);
    if( count == 0 ) count = 0x01000000;

    size = DMAC_xfer_size[ (control >> 4)&0x07 ];
    run_count = numBytes / size;
    if( run_count > count || run_count == 0 )
	run_count = count;

    /* Do copy - FIXME: doesn't work when crossing regions */
    sh4ptr_t region = mem_get_region( source );
    switch( (control >> 12) & 0x03 ) {
    case 0: 
	memcpy( tmp, region, size );
	for( i=0; i<run_count; i++ ) {
	    memcpy( buf, tmp, size );
	    buf += size;
	}
	break;
    case 1: 
	i = run_count * size;
	memcpy( buf, region, i );
	source += i;
	break;
    case 2: 
	for( i=0; i<run_count; i++ ) {
	    memcpy( buf, region, size );
	    buf += size;
	    region -= size;
	}
	source -= (run_count * size);
	break;
    default:
	return 0; /* Illegal */
    }

    /* Update the channel registers */
    count -= run_count;
    MMIO_WRITE( DMAC, SAR0 + (channel<<4), source );
    MMIO_WRITE( DMAC, DMATCR0 + (channel<<4), count );
    if( count == 0 ) {
	control |= CHCR_TE; 
	if( IS_CHANNEL_IRQ_ENABLED(control) )
	    intc_raise_interrupt( INT_DMA_DMTE0 + channel );
	MMIO_WRITE( DMAC, CHCR0 + (channel<<4), control );
    }

    return run_count * size;
}

uint32_t DMAC_put_buffer( int channel, sh4ptr_t buf, uint32_t numBytes )
{
    uint32_t control = DMA_CONTROL(channel);
    uint32_t dest, count, run_count, size, i;

    if( !IS_CHANNEL_ENABLED(control) || !IS_DMAC_ENABLED() )
	return 0;
    
    if( ((control >> 8) & 0x0F) !=  DMARES_DEVICE_TO_MEMORY ) {
	/* Error? */
	return 0;
    } 
    
    dest = DMA_DEST(channel);
    count = DMA_COUNT(channel);
    if( count == 0 ) count = 0x01000000;

    size = DMAC_xfer_size[ (control >> 4)&0x07 ];
    run_count = numBytes / size;
    if( run_count > count || run_count == 0 )
	run_count = count;

    /* Do copy - FIXME: doesn't work when crossing regions */
    sh4ptr_t region = mem_get_region( dest );
    switch( (control >> 12) & 0x03 ) {
    case 0: 
	for( i=0; i<run_count; i++ ) { 
	    /* Doesn't make a whole lot of sense, but hey... */
	    memcpy( region, buf, size );
	    buf += size;
	}
	break;
    case 1: 
	i = run_count * size;
	memcpy( region, buf, i );
	dest += i;
	break;
    case 2: 
	for( i=0; i<run_count; i++ ) {
	    memcpy( region, buf, size );
	    buf += size;
	    region -= size;
	}
	dest -= (run_count * size);
	break;
    default:
	return 0; /* Illegal */
    }

    /* Update the channel registers */
    count -= run_count;
    MMIO_WRITE( DMAC, DAR0 + (channel<<4), dest );
    MMIO_WRITE( DMAC, DMATCR0 + (channel<<4), count );
    if( count == 0 ) {
	control |= CHCR_TE; 
	if( IS_CHANNEL_IRQ_ENABLED(control) )
	    intc_raise_interrupt( INT_DMA_DMTE0 + channel );
	MMIO_WRITE( DMAC, CHCR0 + (channel<<4), control );
    }
    return run_count * size;
}

void DMAC_reset( void )
{

}

void DMAC_save_state( FILE *F ) 
{

}

int DMAC_load_state( FILE *f )
{
    return 0;
}

void DMAC_trigger( int resource )
{
    int i;
    if( !IS_DMAC_ENABLED() )
	return;
    for( i=0; i<4; i++ ) {
	uint32_t control = DMA_CONTROL(i);
	if( IS_CHANNEL_ENABLED(control) ) {
	    uint32_t channel_res = CHANNEL_RESOURCE(control);
	    switch( resource ) {
	    case DMAC_EXTERNAL:
		if( channel_res == DMARES_MEMORY_TO_MEMORY )
		    DMAC_run_channel(i,1);
		break;
	    case DMAC_SCI_TDE:
		if( channel_res == DMARES_SCI_TRANSMIT_EMPTY )
		    DMAC_run_channel(i,1);
		break;
	    case DMAC_SCI_RDF:
		if( channel_res == DMARES_SCI_RECEIVE_FULL )
		    DMAC_run_channel(i,1);
		break;
	    case DMAC_SCIF_TDE:
		if( channel_res == DMARES_SCIF_TRANSMIT_EMPTY )
		    DMAC_run_channel(i,1);
		break;
	    case DMAC_SCIF_RDF:
		if( channel_res == DMARES_SCIF_RECEIVE_FULL )
		    DMAC_run_channel(i,1);
		break;
	    case DMAC_TMU_ICI:
		if( channel_res >= DMARES_MEMORY_TO_MEMORY_TMU ) 
		    DMAC_run_channel(i,1);
		break;
	    }
	}
    }
}
