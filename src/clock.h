/**
 * $Id$
 * External interface to the dreamcast serial port, implemented by 
 * sh4/scif.c
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
#ifndef dream_clock_H
#define dream_clock_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MHZ
#define KHZ
#define SH4_BASE_RATE 200 MHZ
#define ARM_BASE_RATE 33 MHZ
#define PVR2_DOT_CLOCK 27068 KHZ

extern uint32_t sh4_freq;
extern uint32_t sh4_peripheral_freq;
extern uint32_t sh4_bus_freq;
extern uint32_t sh4_cpu_period;
extern uint32_t sh4_peripheral_period;
extern uint32_t sh4_bus_period;
extern uint32_t arm_freq;

#ifdef __cplusplus
}
#endif

#endif
