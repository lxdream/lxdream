/**
 * $Id$
 * 
 * DMA support code
 *
 * Copyright (c) 2006 Nathan Keynes.
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

#include "lib.h"

/**
 * Setup the DMAC for a transfer. Assumes 32-byte block transfer.
 * Caller is responsible for making sure no-one else is using the
 * channel already. 
 *
 * @param channel DMA channel to use, 0 to 3
 * @param source source address (if a memory source)
 * @param dest   destination address (if a memory destination)
 * @param length number of bytes to transfer (must be a multiple of
 *               32.
 * @param direction 0 = host to device, 1 = device to host
 */
void dmac_prepare_channel( int channel, uint32_t source, uint32_t dest,
			   uint32_t length, int direction );

/**
 * Transfer data to the PVR via DMA. Target address should be
 * 0x10000000 for the TA, and 0x11000000 + VRAM address for VRAM.
 *
 * @param target Target address
 * @param buf    Data to write (must be 32-byte aligned)
 * @param length Size of data to write, in bytes.
 * @param region Target region for VRAM writes, 0 for 64-byte region, 1 for 32-byte region.
 *
 * @return 0 on success, non-zero on failure.
 */
int pvr_dma_write( unsigned int target, char *buf, int length, int region );
