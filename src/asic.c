#include <assert.h>
#include "dream.h"
#include "mem.h"
#include "sh4/intc.h"
#include "asic.h"
#include "maple.h"
#define MMIO_IMPL
#include "asic.h"
/*
 * Open questions:
 *   1) Does changing the mask after event occurance result in the
 *      interrupt being delivered immediately?
 *   2) If the pending register is not cleared after an interrupt, does
 *      the interrupt line remain high? (ie does the IRQ reoccur?)
 * TODO: Logic diagram of ASIC event/interrupt logic.
 *
 * ... don't even get me started on the "EXTDMA" page, about which, apparently,
 * practically nothing is publicly known...
 */

void asic_init( void )
{
    register_io_region( &mmio_region_ASIC );
    register_io_region( &mmio_region_EXTDMA );
    mmio_region_ASIC.trace_flag = 0; /* Because this is called so often */
    asic_event( EVENT_GDROM_CMD );
}

void mmio_region_ASIC_write( uint32_t reg, uint32_t val )
{
    switch( reg ) {
        case PIRQ0:
        case PIRQ1:
        case PIRQ2:
            /* Clear any interrupts */
            MMIO_WRITE( ASIC, reg, MMIO_READ(ASIC, reg)&~val );
            break;
        case MAPLE_STATE:
            MMIO_WRITE( ASIC, reg, val );
            if( val & 1 ) {
                uint32_t maple_addr = MMIO_READ( ASIC, MAPLE_DMA) &0x1FFFFFE0;
//                maple_handle_buffer( maple_addr );
		WARN( "Maple request initiated, halting" );
                MMIO_WRITE( ASIC, reg, 0 );
                sh4_stop();
            }
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
        case PIRQ0:
        case PIRQ1:
        case PIRQ2:
            val = MMIO_READ(ASIC, reg);
//            WARN( "Read from ASIC (%03X => %08X) [%s: %s]",
//                  reg, val, MMIO_REGID(ASIC,reg), MMIO_REGDESC(ASIC,reg) );
            return val;            
        case G2STATUS:
            return 0; /* find out later if there's any cases we actually need to care about */
        default:
            val = MMIO_READ(ASIC, reg);
            WARN( "Read from ASIC (%03X => %08X) [%s: %s]",
                  reg, val, MMIO_REGID(ASIC,reg), MMIO_REGDESC(ASIC,reg) );
            return val;
    }
           
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



MMIO_REGION_WRITE_FN( EXTDMA, reg, val )
{
    MMIO_WRITE( EXTDMA, reg, val );
}

MMIO_REGION_READ_FN( EXTDMA, reg )
{
    switch( reg ) {
        case GDBUSY: return 0;
        default:
            return MMIO_READ( EXTDMA, reg );
    }
}

