#ifndef sh4intc_H
#define sh4intc_H 1

#include "sh4core.h"

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

#define INT_IRQ0        0     /* External Interrupt request 0 */
#define INT_IRQ1        1
#define INT_IRQ2        2
#define INT_IRQ3        3
#define INT_IRQ4        4
#define INT_IRQ5        5
#define INT_IRQ6        6
#define INT_IRQ7        7
#define INT_IRQ8        8
#define INT_IRQ9        9
#define INT_IRQ10      10
#define INT_IRQ11      11
#define INT_IRQ12      12
#define INT_IRQ13      13
#define INT_IRQ14      14
#define INT_NMI        15     /* Non-Maskable Interrupt */
#define INT_HUDI       16     /* Hitachi use debug interface */
#define INT_GPIO       17     /* I/O port interrupt */
#define INT_DMA_DMTE0  18     /* DMA transfer end 0 */
#define INT_DMA_DMTE1  19     /* DMA transfer end 1 */
#define INT_DMA_DMTE2  20     /* DMA transfer end 2 */
#define INT_DMA_DMTE3  21     /* DMA transfer end 3 */
#define INT_DMA_DMAE   22     /* DMA address error */
#define INT_TMU_TUNI0  23     /* Timer underflow interrupt 0 */
#define INT_TMU_TUNI1  24     /* Timer underflow interrupt 1 */
#define INT_TMU_TUNI2  25     /* Timer underflow interrupt 2 */
#define INT_TMU_TICPI2 26     /* Timer input capture interrupt */
#define INT_RTC_ATI    27     /* RTC Alarm interrupt */
#define INT_RTC_PRI    28     /* RTC periodic interrupt */
#define INT_RTC_CUI    29     /* RTC Carry-up interrupt */
#define INT_SCI_ERI    30     /* SCI receive-error interrupt */
#define INT_SCI_RXI    31     /* SCI receive-data-full interrupt */
#define INT_SCI_TXI    32     /* SCI transmit-data-empty interrupt */
#define INT_SCI_TEI    33     /* SCI transmit-end interrupt */
#define INT_SCIF_ERI   34     /* SCIF receive-error interrupt */
#define INT_SCIF_RXI   35     /* SCIF receive-data-full interrupt */
#define INT_SCIF_BRI   36     /* SCIF break interrupt request */
#define INT_SCIF_TXI   37     /* SCIF Transmit-data-empty interrupt */
#define INT_WDT_ITI    38     /* WDT Interval timer interval (CPG) */
#define INT_REF_RCMI   39     /* Compare-match interrupt */
#define INT_REF_ROVI   40     /* Refresh counter overflow interrupt */

#define INT_NUM_SOURCES 41

char *intc_get_interrupt_name( int which );
void intc_raise_interrupt( int which );
void intc_clear_interrupt( int which );
uint32_t intc_accept_interrupt( void );
void intc_reset( void );
void intc_mask_changed( void );

#ifdef __cplusplus
}
#endif

#endif /* !sh4intc_H */
