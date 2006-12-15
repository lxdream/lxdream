/**
 * $Id: ide.h,v 1.9 2006-12-15 10:18:39 nkeynes Exp $
 *
 * This file defines the interface and structures of the dreamcast's IDE 
 * port. Note that the register definitions are in asic.h, as the registers
 * fall into the general ASIC ranges (and I don't want to use smaller pages
 * at this stage). The registers here are exactly as per the ATA 
 * specifications, which makes things a little easier.
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

#ifndef dream_ide_H
#define dream_ide_H 1

#include "dream.h"

struct ide_registers {
    /* IDE interface registers */
    uint8_t status;  /* A05F709C + A05F7018 Read-only */
    uint8_t control; /* A05F7018 Write-only 01110 */
    uint8_t error;   /* A05F7084 Read-only  10001 */
    uint8_t feature; /* A05F7084 Write-only 10001 */
    uint8_t count;   /* A05F7088 Read/Write 10010 */
    uint8_t disc;    /* A05F708C Read-only 10011 */
    uint8_t lba0;    /* A05F708C Write-only 10011 (NB: Presumed, TBV */
    uint8_t lba1;    /* A05F7090 Read/Write 10100 */
    uint8_t lba2;    /* A05F7094 Read/Write 10101 */
    uint8_t device;  /* A05F7098 Read/Write 10110 */
    uint8_t command; /* A05F709C Write-only 10111 */

    /* Internal IDE state */
    uint8_t intrq_pending; /* Flag to indicate if the INTRQ line is active */
    int state;

    /* Sense response for the last executed packet command */
    unsigned char gdrom_sense[10];


    /* offset in the buffer of the next word to read/write, or -1
     * if inactive.
     */ 
    int data_offset;
    int data_length;
   
    int block_length; /* Used to determine the transfer unit size */
    int block_left; /* Bytes remaining in the current block */
};

#define IDE_STATE_IDLE      0 
#define IDE_STATE_CMD_WRITE 1
#define IDE_STATE_PIO_READ  2
#define IDE_STATE_PIO_WRITE 3
#define IDE_STATE_DMA_READ  4
#define IDE_STATE_DMA_WRITE 5
#define IDE_STATE_BUSY      6

/* Flag bits */
#define IDE_STATUS_BSY  0x80    /* Busy */
#define IDE_STATUS_DRDY 0x40    /* Device ready */
#define IDE_STATUS_DMRD 0x20    /* DMA Request */
#define IDE_STATUS_SERV 0x10   
#define IDE_STATUS_DRQ  0x08
#define IDE_STATUS_CHK  0x01    /* Check condition (ie error) */

#define IDE_FEAT_DMA 0x01
#define IDE_FEAT_OVL 0x02

#define IDE_COUNT_CD 0x01       /* Command (1)/Data (0) */
#define IDE_COUNT_IO 0x02       /* Input (0)/Output (1) */
#define IDE_COUNT_REL 0x04      /* Release device */


#define IDE_CTL_RESET 0x04
#define IDE_CTL_IRQEN 0x02 /* IRQ enabled when == 0 */

#define IDE_CMD_NOP 0x00
#define IDE_CMD_RESET_DEVICE 0x08
#define IDE_CMD_PACKET 0xA0
#define IDE_CMD_IDENTIFY_PACKET_DEVICE 0xA1
#define IDE_CMD_SERVICE 0xA2
#define IDE_CMD_SET_FEATURE 0xEF

#define IDE_FEAT_SET_TRANSFER_MODE 0x03
#define IDE_XFER_PIO        0x00
#define IDE_XFER_PIO_FLOW   0x08
#define IDE_XFER_MULTI_DMA  0x20
#define IDE_XFER_ULTRA_DMA  0x40

extern struct ide_registers idereg;

/* Note: control can be written at any time - all other registers are writable
 * only when ide_can_write_regs() is true
 */
#define ide_can_write_regs() ((idereg.status&0x88)==0)
#define IS_IDE_IRQ_ENABLED() ((idereg.control&0x02)==0)


uint16_t ide_read_data_pio(void);
void ide_write_data_pio( uint16_t value );
uint32_t ide_read_data_dma( uint32_t addr, uint32_t length );
uint8_t ide_read_status(void);
void ide_write_buffer( unsigned char *data, int length ); 

void ide_write_command( uint8_t command );
void ide_write_control( uint8_t value );

void ide_dma_read_req( uint32_t addr, uint32_t length );
#endif
