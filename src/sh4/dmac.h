/**
 * $Id$
 *
 * SH4 onboard DMA controller (DMAC) definitions.
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

#ifndef dream_sh4dmac_H
#define dream_sh4dmac_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* External DREQ. Note only used for DMA memory-to-memory - for single 
 * address transfers the device uses DMAC_get_buffer/DMAC_put_buffer.
 */
#define DMAC_EXTERNAL 0 
#define DMAC_SCI_TDE 1  /* SCI Transmit data empty */
#define DMAC_SCI_RDF 2  /* SCI Receive data full */
#define DMAC_SCIF_TDE 3 /* SCIF Transmit data empty */
#define DMAC_SCIF_RDF 4 /* SCIF Receive data full */
#define DMAC_TMU_ICI 5  /* TMU Input capture interrupt (not used?) */

/**
 * Trigger a DMAC transfer by asserting one of the above DMA request lines
 * (from the onboard peripherals). Actual transfer is dependent on the 
 * relevant channel configuration.
 */
void DMAC_trigger( int dmac_trigger );

/**
 * Execute a memory-to-external-device transfer. Copies data into the supplied
 * buffer up to a maximum of bytecount bytes. 
 * @return Actual number of bytes copied.
 */
uint32_t DMAC_get_buffer( int channel, unsigned char *buf, uint32_t bytecount );

/**
 * execute an external-device-to-memory transfer. Copies data from the 
 * supplied buffer into memory up to a maximum of bytecount bytes. 
 * @return Actual number of bytes copied.
 */
uint32_t DMAC_put_buffer( int channel, unsigned char *buf, uint32_t bytecount );

#ifdef __cplusplus
}
#endif
#endif
