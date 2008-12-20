/**
 * $Id$
 *
 * MMIO region and supporting function declarations. Private to the sh4
 * module.
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

#include "lxdream.h"
#include "mmio.h"

#if (defined(MMIO_IMPL) && !defined(SH4MMIO_IMPL)) || \
    (!defined(MMIO_IMPL) && !defined(SH4MMIO_IFACE))

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MMIO_IMPL
#define SH4MMIO_IMPL
#else
#define SH4MMIO_IFACE
#endif
/* SH7750 onchip mmio devices */

MMIO_REGION_BEGIN( 0xFF000000, MMU, "MMU Registers" )
    LONG_PORT( 0x000, PTEH, PORT_MRW, UNDEFINED, "Page table entry high" )
    LONG_PORT( 0x004, PTEL, PORT_MRW, UNDEFINED, "Page table entry low" )
    LONG_PORT( 0x008, TTB,  PORT_MRW, UNDEFINED, "Translation table base" )
    LONG_PORT( 0x00C, TEA,  PORT_MRW, UNDEFINED, "TLB exception address" )
    LONG_PORT( 0x010, MMUCR,PORT_MRW, 0, "MMU control register" )
    BYTE_PORT( 0x014, BASRA, PORT_MRW, UNDEFINED, "Break ASID A" ) /* UBC */
    BYTE_PORT( 0x018, BASRB, PORT_MRW, UNDEFINED, "Break ASID B" ) /* UBC */
    LONG_PORT( 0x01C, CCR,  PORT_MRW, 0, "Cache control register" )
    LONG_PORT( 0x020, TRA,  PORT_MRW, UNDEFINED, "TRAPA exception register" )
    LONG_PORT( 0x024, EXPEVT,PORT_MRW, 0, "Exception event register" )
    LONG_PORT( 0x028, INTEVT,PORT_MRW, UNDEFINED, "Interrupt event register" )
    LONG_PORT( 0x02C, MMUUNK1, PORT_MRW, 0, "Unknown MMU/general register" )
    LONG_PORT( 0x030, SH4VER, PORT_MRW, 0x040205C1, "SH4 version register (PVR)" ) /* Renamed to avoid naming conflict */
    LONG_PORT( 0x034, PTEA, PORT_MRW, UNDEFINED, "Page table entry assistance" )
    LONG_PORT( 0x038, QACR0,PORT_MRW, UNDEFINED, "Queue address control 0" )
    LONG_PORT( 0x03C, QACR1,PORT_MRW, UNDEFINED, "Queue address control 1" )
    WORD_PORT( 0x084, PMCR1, PORT_MRW, 0, "Performance counter control 1" )
    WORD_PORT( 0x088, PMCR2, PORT_MRW, 0, "Performance counter control 2" )
MMIO_REGION_END

/* Performance counter values (undocumented) */
MMIO_REGION_BEGIN( 0xFF100000, PMM, "Performance monitoring" )
    LONG_PORT( 0x004, PMCTR1H, PORT_MR, 0, "Performance counter 1 High" )
    LONG_PORT( 0x008, PMCTR1L, PORT_MR, 0, "Performance counter 1 Low" )
    LONG_PORT( 0x00C, PMCTR2H, PORT_MR, 0, "Performance counter 2 High" )
    LONG_PORT( 0x010, PMCTR2L, PORT_MR, 0, "Performance counter 2 Low" )
MMIO_REGION_END

/* User Break Controller (Page 717 [757] of sh7750h manual) */
MMIO_REGION_BEGIN( 0xFF200000, UBC, "User Break Controller" )
    LONG_PORT( 0x000, BARA, PORT_MRW, UNDEFINED, "Break address A" )
    BYTE_PORT( 0x004, BAMRA, PORT_MRW, UNDEFINED, "Break address mask A" )
    WORD_PORT( 0x008, BBRA, PORT_MRW, 0, "Break bus cycle A" )
    LONG_PORT( 0x00C, BARB, PORT_MRW, UNDEFINED, "Break address B" )
    BYTE_PORT( 0x010, BAMRB, PORT_MRW, UNDEFINED, "Break address mask B" )
    WORD_PORT( 0x014, BBRB, PORT_MRW, 0, "Break bus cycle B" )
    LONG_PORT( 0x018, BDRB, PORT_MRW, UNDEFINED, "Break data B" )
    LONG_PORT( 0x01C, BDMRB, PORT_MRW, UNDEFINED, "Break data mask B" )
    WORD_PORT( 0x020, BRCR, PORT_MRW, 0, "Break control" )
MMIO_REGION_END
/* Bus State Controller (Page 293 [333] of sh7750h manual)
 * I/O Ports */
MMIO_REGION_BEGIN( 0xFF800000, BSC, "Bus State Controller" )
    LONG_PORT( 0x000, BCR1, PORT_MRW, 0, "Bus control 1" )
    WORD_PORT( 0x004, BCR2, PORT_MRW, 0x3FFC, "Bus control 2" )
    LONG_PORT( 0x008, WCR1, PORT_MRW, 0x77777777, "Wait state control 1" )
    LONG_PORT( 0x00C, WCR2, PORT_MRW, 0xFFFEEFFF, "Wait state control 2" )
    LONG_PORT( 0x010, WCR3, PORT_MRW, 0x07777777, "Wait state control 3" )
    LONG_PORT( 0x014, MCR, PORT_MRW, 0, "Memory control register" )
    WORD_PORT( 0x018, PCR, PORT_MRW, 0, "PCMCIA control register" )
    WORD_PORT( 0x01C, RTCSR, PORT_MRW, 0, "Refresh timer control/status" )
    WORD_PORT( 0x020, RTCNT, PORT_MRW, 0, "Refresh timer counter" )
    WORD_PORT( 0x024, RTCOR, PORT_MRW, 0, "Refresh timer constant" )
    WORD_PORT( 0x028, RFCR, PORT_MRW, 0, "Refresh count" )
    LONG_PORT( 0x02C, PCTRA, PORT_MRW, 0, "Port control register A" )
    WORD_PORT( 0x030, PDTRA, PORT_RW, UNDEFINED, "Port data register A" )
    LONG_PORT( 0x040, PCTRB, PORT_MRW, 0, "Port control register B" )
    WORD_PORT( 0x044, PDTRB, PORT_RW, UNDEFINED, "Port data register B" )
    WORD_PORT( 0x048, GPIOIC, PORT_MRW, 0, "GPIO interrupt control register" )
MMIO_REGION_END

/* DMA Controller (Page 457 [497] of sh7750h manual) */
MMIO_REGION_BEGIN( 0xFFA00000, DMAC, "DMA Controller" )
    LONG_PORT( 0x000, SAR0, PORT_MRW, UNDEFINED, "DMA source address 0" )
    LONG_PORT( 0x004, DAR0, PORT_MRW, UNDEFINED, "DMA destination address 0" )
    LONG_PORT( 0x008, DMATCR0, PORT_MRW, UNDEFINED, "DMA transfer count 0" )
    LONG_PORT( 0x00C, CHCR0, PORT_MRW, 0, "DMA channel control 0" )
    LONG_PORT( 0x010, SAR1, PORT_MRW, UNDEFINED, "DMA source address 1" )
    LONG_PORT( 0x014, DAR1, PORT_MRW, UNDEFINED, "DMA destination address 1" )
    LONG_PORT( 0x018, DMATCR1, PORT_MRW, UNDEFINED, "DMA transfer count 1" )
    LONG_PORT( 0x01C, CHCR1, PORT_MRW, 0, "DMA channel control 1" )
    LONG_PORT( 0x020, SAR2, PORT_MRW, UNDEFINED, "DMA source address 2" )
    LONG_PORT( 0x024, DAR2, PORT_MRW, UNDEFINED, "DMA destination address 2" )
    LONG_PORT( 0x028, DMATCR2, PORT_MRW, UNDEFINED, "DMA transfer count 2" )
    LONG_PORT( 0x02C, CHCR2, PORT_MRW, 0, "DMA channel control 2" )
    LONG_PORT( 0x030, SAR3, PORT_MRW, UNDEFINED, "DMA source address 3" )
    LONG_PORT( 0x034, DAR3, PORT_MRW, UNDEFINED, "DMA destination address 3" )
    LONG_PORT( 0x038, DMATCR3, PORT_MRW, UNDEFINED, "DMA transfer count 3" )
    LONG_PORT( 0x03C, CHCR3, PORT_MRW, 0, "DMA channel control 3" )
    LONG_PORT( 0x040, DMAOR, PORT_MRW, 0, "DMA operation register" )
MMIO_REGION_END

#define FRQCR_CKOEN    0x0800
#define FRQCR_PLL1EN   0x0400
#define FRQCR_PLL2EN   0x0200
#define FRQCR_IFC_MASK 0x01C0
#define FRQCR_BFC_MASK 0x0038
#define FRQCR_PFC_MASK 0x0007

/* Clock Pulse Generator (page 233 [273] of sh7750h manual) */
MMIO_REGION_BEGIN( 0xFFC00000, CPG, "Clock Pulse Generator" )
    WORD_PORT( 0x000, FRQCR, PORT_MRW, 0x0E0A, "Frequency control" )
    BYTE_PORT( 0x004, STBCR, PORT_MRW, 0, "Standby control" )
    BYTE_PORT( 0x008, WTCNT, PORT_MRW, 0, "Watchdog timer counter" )
    BYTE_PORT( 0x00C, WTCSR, PORT_MRW, 0, "Watchdog timer control/status" )
    BYTE_PORT( 0x010, STBCR2, PORT_MRW, 0, "Standby control 2" )
MMIO_REGION_END

/* Real time clock (Page 253 [293] of sh7750h manual) */
MMIO_REGION_BEGIN( 0xFFC80000, RTC, "Realtime Clock" )
    BYTE_PORT( 0x000, R64CNT, PORT_R, UNDEFINED, "64 Hz counter" )
    BYTE_PORT( 0x004, RSECCNT, PORT_MRW, UNDEFINED, "Second counter" )
    /* ... */
MMIO_REGION_END

/* Interrupt controller (Page 699 [739] of sh7750h manual) */
MMIO_REGION_BEGIN( 0xFFD00000, INTC, "Interrupt Controller" )
    WORD_PORT( 0x000, ICR, PORT_MRW, 0x0000, "Interrupt control register" )
    WORD_PORT( 0x004, IPRA, PORT_MRW, 0x0000, "Interrupt priority register A" )
    WORD_PORT( 0x008, IPRB, PORT_MRW, 0x0000, "Interrupt priority register B" )
    WORD_PORT( 0x00C, IPRC, PORT_MRW, 0x0000, "Interrupt priority register C" )
    WORD_PORT( 0x010, IPRD, PORT_MRW, 0xDA74, "Interrupt priority register D" )
MMIO_REGION_END

/* Timer unit (Page 277 [317] of sh7750h manual) */
MMIO_REGION_BEGIN( 0xFFD80000, TMU, "Timer Unit" )
    BYTE_PORT( 0x000, TOCR, PORT_MRW, 0x00, "Timer output control register" )
    BYTE_PORT( 0x004, TSTR, PORT_MRW, 0x00, "Timer start register" )
    LONG_PORT( 0x008, TCOR0, PORT_MRW, 0xFFFFFFFF, "Timer constant 0" )
    LONG_PORT( 0x00C, TCNT0, PORT_MRW, 0xFFFFFFFF, "Timer counter 0" )
    WORD_PORT( 0x010, TCR0, PORT_MRW, 0x0000, "Timer control 0" )
    LONG_PORT( 0x014, TCOR1, PORT_MRW, 0xFFFFFFFF, "Timer constant 1" )
    LONG_PORT( 0x018, TCNT1, PORT_MRW, 0xFFFFFFFF, "Timer counter 1" )
    WORD_PORT( 0x01C, TCR1, PORT_MRW, 0x0000, "Timer control 1" )
    LONG_PORT( 0x020, TCOR2, PORT_MRW, 0xFFFFFFFF, "Timer constant 2" )
    LONG_PORT( 0x024, TCNT2, PORT_MRW, 0xFFFFFFFF, "Timer counter 2" )
    WORD_PORT( 0x028, TCR2, PORT_MRW, 0x0000, "Timer control 2" )
    LONG_PORT( 0x02C, TCPR2, PORT_R, UNDEFINED, "Input capture register" )
MMIO_REGION_END

/* Serial channel (page 541 [581] of sh7750h manual) */
MMIO_REGION_BEGIN( 0xFFE00000, SCI, "Serial Communication Interface" )
    BYTE_PORT( 0x000, SCSMR1, PORT_MRW, 0x00, "Serial mode register" )
    BYTE_PORT( 0x004, SCBRR1, PORT_MRW, 0xFF, "Bit rate register" )
    BYTE_PORT( 0x008, SCSCR1, PORT_MRW, 0x00, "Serial control register" )
    BYTE_PORT( 0x00C, SCTDR1, PORT_MRW, 0xFF, "Transmit data register" )
    BYTE_PORT( 0x010, SCSSR1, PORT_MRW, 0x84, "Serial status register" )
    BYTE_PORT( 0x014, SCRDR1, PORT_R, 0x00, "Receive data register" )
    BYTE_PORT( 0x01C, SCSPTR1, PORT_MRW, 0x00, "Serial port register" )
MMIO_REGION_END

MMIO_REGION_BEGIN( 0xFFE80000, SCIF, "Serial Controller (FIFO) Registers" )
    WORD_PORT( 0x000, SCSMR2, PORT_MRW, 0x0000, "Serial mode register (FIFO)" )
    BYTE_PORT( 0x004, SCBRR2, PORT_MRW, 0xFF, "Bit rate register (FIFO)" )
    WORD_PORT( 0x008, SCSCR2, PORT_MRW, 0x0000, "Serial control register" )
    BYTE_PORT( 0x00C, SCFTDR2, PORT_W, UNDEFINED, "Transmit FIFO data register" )
    WORD_PORT( 0x010, SCFSR2, PORT_MRW, 0x0060, "Serial status register (FIFO)" )
    BYTE_PORT( 0x014, SCFRDR2, PORT_R, UNDEFINED, "Receive FIFO data register" )
    WORD_PORT( 0x018, SCFCR2, PORT_MRW, 0x0000, "FIFO control register" )
    WORD_PORT( 0x01C, SCFDR2, PORT_MR, 0x0000, "FIFO data count register" )
    WORD_PORT( 0x020, SCSPTR2, PORT_MRW, 0x0000, "Serial port register (FIFO)" )
    WORD_PORT( 0x024, SCLSR2, PORT_MRW, 0x0000, "Line status register (FIFO)" )
MMIO_REGION_END

MMIO_REGION_LIST_BEGIN( sh4mmio )
    MMIO_REGION( MMU )
    MMIO_REGION( UBC )
    MMIO_REGION( BSC )
    MMIO_REGION( DMAC )
    MMIO_REGION( CPG )
    MMIO_REGION( RTC )
    MMIO_REGION( INTC )
    MMIO_REGION( TMU )
    MMIO_REGION( SCI )
    MMIO_REGION( SCIF )
    MMIO_REGION( PMM )
MMIO_REGION_LIST_END

/* mmucr register bits */
#define MMUCR_AT   0x00000001 /* Address Translation enabled */
#define MMUCR_TI   0x00000004 /* TLB invalidate (always read as 0) */
#define MMUCR_SV   0x00000100 /* Single Virtual mode=1 / multiple virtual=0 */
#define MMUCR_SQMD 0x00000200 /* Store queue mode bit (0=user, 1=priv only) */
#define MMUCR_URC  0x0000FC00 /* UTLB access counter */
#define MMUCR_URB  0x00FC0000 /* UTLB entry boundary */
#define MMUCR_LRUI 0xFC000000 /* Least recently used ITLB */
#define MMUCR_MASK 0xFCFCFF05
#define MMUCR_RMASK 0xFCFCFF01 /* Read mask */

#define IS_MMU_ENABLED() (MMIO_READ(MMU, MMUCR)&MMUCR_AT)

/* ccr register bits */
#define CCR_IIX    0x00008000 /* IC index enable */
#define CCR_ICI    0x00000800 /* IC invalidation (always read as 0) */
#define CCR_ICE    0x00000100 /* IC enable */
#define CCR_OIX    0x00000080 /* OC index enable */
#define CCR_ORA    0x00000020 /* OC RAM enable */
#define CCR_OCI    0x00000008 /* OC invalidation (always read as 0) */
#define CCR_CB     0x00000004 /* Copy-back (P1 area cache write mode) */
#define CCR_WT     0x00000002 /* Write-through (P0,U0,P3 write mode) */
#define CCR_OCE    0x00000001 /* OC enable */
#define CCR_MASK   0x000089AF
#define CCR_RMASK  0x000081A7 /* Read mask */

#define MEM_OC_INDEX0   (CCR_ORA|CCR_OCE)
#define MEM_OC_INDEX1   (CCR_ORA|CCR_OIX|CCR_OCE)

#define PMCR_CLKF  0x0100
#define PMCR_PMCLR 0x2000
#define PMCR_PMST  0x4000
#define PMCR_PMEN  0x8000
#define PMCR_RUNNING (PMCR_PMST|PMCR_PMEN)

/* MMU functions */
void mmu_init(void);
void mmu_set_cache_mode( int );
void mmu_ldtlb(void);

int32_t FASTCALL mmu_icache_addr_read( sh4addr_t addr );
int32_t FASTCALL mmu_icache_data_read( sh4addr_t addr );
int32_t FASTCALL mmu_itlb_addr_read( sh4addr_t addr );
int32_t FASTCALL mmu_itlb_data_read( sh4addr_t addr );
int32_t FASTCALL mmu_ocache_addr_read( sh4addr_t addr );
int32_t FASTCALL mmu_ocache_data_read( sh4addr_t addr );
int32_t FASTCALL mmu_utlb_addr_read( sh4addr_t addr );
int32_t FASTCALL mmu_utlb_data_read( sh4addr_t addr );
void FASTCALL mmu_icache_addr_write( sh4addr_t addr, uint32_t val );
void FASTCALL mmu_icache_data_write( sh4addr_t addr, uint32_t val );
void FASTCALL mmu_itlb_addr_write( sh4addr_t addr, uint32_t val );
void FASTCALL mmu_itlb_data_write( sh4addr_t addr, uint32_t val );
void FASTCALL mmu_ocache_addr_write( sh4addr_t addr, uint32_t val );
void FASTCALL mmu_ocache_data_write( sh4addr_t addr, uint32_t val );
void FASTCALL mmu_utlb_addr_write( sh4addr_t addr, uint32_t val );
void FASTCALL mmu_utlb_data_write( sh4addr_t addr, uint32_t val );


#ifdef __cplusplus
}
#endif

#endif
