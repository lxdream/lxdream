#include "dream.h"
#include "mem.h"
#include "sh4core.h"
#include "sh4mmio.h"
#define MMIO_IMPL
#include "sh4mmio.h"

/********************************* MMU *************************************/

MMIO_REGION_READ_STUBFN( MMU )

#define OCRAM_START (0x1C000000>>PAGE_BITS)
#define OCRAM_END   (0x20000000>>PAGE_BITS)

static char *cache = NULL;

void mmio_region_MMU_write( uint32_t reg, uint32_t val )
{
    switch(reg) {
        case CCR:
            mmu_set_cache_mode( val & (CCR_OIX|CCR_ORA) );
            INFO( "Cache mode set to %08X", val );
            break;
        default:
            break;
    }
    MMIO_WRITE( MMU, reg, val );
}


void mmu_init() 
{
  cache = mem_alloc_pages(2);
}

void mmu_set_cache_mode( int mode )
{
    uint32_t i;
    switch( mode ) {
        case MEM_OC_INDEX0: /* OIX=0 */
            for( i=OCRAM_START; i<OCRAM_END; i++ )
                page_map[i] = cache + ((i&0x02)<<(PAGE_BITS-1));
            break;
        case MEM_OC_INDEX1: /* OIX=1 */
            for( i=OCRAM_START; i<OCRAM_END; i++ )
                page_map[i] = cache + ((i&0x02000000)>>(25-PAGE_BITS));
            break;
        default: /* disabled */
            for( i=OCRAM_START; i<OCRAM_END; i++ )
                page_map[i] = NULL;
            break;
    }
}


/********************************* BSC *************************************/

uint16_t bsc_output_mask_lo = 0, bsc_output_mask_hi = 0;
uint16_t bsc_input_mask_lo = 0, bsc_input_mask_hi = 0;
uint32_t bsc_output = 0, bsc_input = 0x0300;

void bsc_out( int output, int mask )
{
    /* Go figure... The BIOS won't start without this mess though */
    if( ((output | (~mask)) & 0x03) == 3 ) {
        bsc_output |= 0x03;
    } else {
        bsc_output &= ~0x03;
    }
}

void mmio_region_BSC_write( uint32_t reg, uint32_t val )
{
    int i;
    switch( reg ) {
        case PCTRA:
            bsc_input_mask_lo = bsc_output_mask_lo = 0;
            for( i=0; i<16; i++ ) {
                int bits = (val >> (i<<1)) & 0x03;
                if( bits == 2 ) bsc_input_mask_lo |= (1<<i);
                else if( bits != 0 ) bsc_output_mask_lo |= (1<<i);
            }
            bsc_output = (bsc_output&0x000F0000) |
                (MMIO_READ( BSC, PDTRA ) & bsc_output_mask_lo);
            bsc_out( MMIO_READ( BSC, PDTRA ) | ((MMIO_READ(BSC,PDTRB)<<16)),
                     bsc_output_mask_lo | (bsc_output_mask_hi<<16) );
            break;
        case PCTRB:
            bsc_input_mask_hi = bsc_output_mask_hi = 0;
            for( i=0; i<4; i++ ) {
                int bits = (val >> (i>>1)) & 0x03;
                if( bits == 2 ) bsc_input_mask_hi |= (1<<i);
                else if( bits != 0 ) bsc_output_mask_hi |= (1<<i);
            }
            bsc_output = (bsc_output&0xFFFF) |
                ((MMIO_READ( BSC, PDTRA ) & bsc_output_mask_hi)<<16);
            break;
        case PDTRA:
            bsc_output = (bsc_output&0x000F0000) |
                (val & bsc_output_mask_lo );
            bsc_out( val | ((MMIO_READ(BSC,PDTRB)<<16)),
                     bsc_output_mask_lo | (bsc_output_mask_hi<<16) );
            break;
        case PDTRB:
            bsc_output = (bsc_output&0xFFFF) |
                ( (val & bsc_output_mask_hi)<<16 );
            break;
    }
    WARN( "Write to (mostly) unimplemented BSC (%03X <= %08X) [%s: %s]",
          reg, val, MMIO_REGID(BSC,reg), MMIO_REGDESC(BSC,reg) );
    MMIO_WRITE( BSC, reg, val );
}

int32_t mmio_region_BSC_read( uint32_t reg )
{
    int32_t val;
    switch( reg ) {
        case PDTRA:
            val = (bsc_input & bsc_input_mask_lo) | (bsc_output&0xFFFF);
            break;
        case PDTRB:
            val = ((bsc_input>>16) & bsc_input_mask_hi) | (bsc_output>>16);
            break;
        default:
            val = MMIO_READ( BSC, reg );
    }
    WARN( "Read from (mostly) unimplemented BSC (%03X => %08X) [%s: %s]",
          reg, val, MMIO_REGID(BSC,reg), MMIO_REGDESC(BSC,reg) );
    return val;
}

/********************************* UBC *************************************/

MMIO_REGION_STUBFNS( UBC )

/********************************* CPG *************************************/

MMIO_REGION_STUBFNS( CPG )

/********************************* DMAC *************************************/

MMIO_REGION_STUBFNS( DMAC )

/********************************** RTC *************************************/

MMIO_REGION_STUBFNS( RTC )

/********************************** TMU *************************************/

int timer_divider[3] = {16,16,16};
MMIO_REGION_READ_DEFFN( TMU )

int get_timer_div( int val )
{
    switch( val & 0x07 ) {
        case 0: return 16; /* assume peripheral clock is IC/4 */
        case 1: return 64;
        case 2: return 256;
        case 3: return 1024;
        case 4: return 4096;
    }
    return 1;
}

void mmio_region_TMU_write( uint32_t reg, uint32_t val )
{
    switch( reg ) {
        case TCR0:
            timer_divider[0] = get_timer_div(val);
            break;
        case TCR1:
            timer_divider[1] = get_timer_div(val);
            break;
        case TCR2:
            timer_divider[2] = get_timer_div(val);
            break;
    }
    MMIO_WRITE( TMU, reg, val );
}

void run_timers( int cycles )
{
    int tcr = MMIO_READ( TMU, TSTR );
    cycles *= 16;
    if( tcr & 1 ) {
        int count = cycles / timer_divider[0];
        int *val = MMIO_REG( TMU, TCNT0 );
        if( *val < count ) {
            MMIO_READ( TMU, TCR0 ) |= 0x100;
            /* interrupt goes here */
            count -= *val;
            *val = MMIO_READ( TMU, TCOR0 ) - count;
        } else {
            *val -= count;
        }
    }
}

/********************************** SCI *************************************/

MMIO_REGION_STUBFNS( SCI )

/********************************* SCIF *************************************/

MMIO_REGION_STUBFNS( SCIF )

