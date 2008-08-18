
#include <stdio.h>

#define EVENT_PVR_RENDER_DONE 2
#define EVENT_SCANLINE1 3
#define EVENT_SCANLINE2 4
#define EVENT_RETRACE   5
#define EVENT_PVR_UNK 6
#define EVENT_PVR_OPAQUE_DONE 7
#define EVENT_PVR_OPAQUEMOD_DONE 8
#define EVENT_PVR_TRANS_DONE 9
#define EVENT_PVR_TRANSMOD_DONE 10
#define EVENT_MAPLE_DMA 12
#define EVENT_MAPLE_ERR 13 /* ??? */
#define EVENT_IDE_DMA 14
#define EVENT_G2_DMA0  15
#define EVENT_G2_DMA1  16
#define EVENT_G2_DMA2  17
#define EVENT_G2_DMA3  18
#define EVENT_PVR_DMA   19
#define EVENT_SORT_DMA  20
#define EVENT_PVR_PUNCHOUT_DONE 21

#define EVENT_TA_ERROR  31
#define EVENT_IDE       32
#define EVENT_AICA      33

#define EVENT_PVR_PRIM_ALLOC_FAIL 66
#define EVENT_PVR_MATRIX_ALLOC_FAIL 67
#define EVENT_PVR_BAD_INPUT 68

#define EVENT_SORT_DMA_ERR 92

/**
 * Wait for an ASIC event. 
 * @return 0 if the event occurred, otherwise -1 if the wait timed out.
 */
int asic_wait( int event );

/**
 * Wait for either of a pair of events.
 * @return the event ID of the event that occured, or -1 if the wait timed out
 */
int asic_wait2( int event1, int event2 );

/**
 * Check if an ASIC event is active (does not wait)
 * @return 0 if inactive, nonzero if active.
 */
int asic_check( int event );

/**
 * Clear all asic events
 */
void asic_clear();

/**
 * Print the contents of the ASIC event registers to the supplied FILE
 */
void asic_dump( FILE *f );

void asic_mask_all();

/**
 * Wait until the G2 FIFO buffer is clear to write
 */
int g2_fifo_wait();
