/**
 * $Id: serial.h,v 1.1 2005-12-22 07:38:06 nkeynes Exp $
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
#ifndef dream_serial_H
#define dream_serial_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SERIAL_8BIT        0x00
#define SERIAL_7BIT        0x40
#define SERIAL_PARITY_OFF  0x00
#define SERIAL_PARITY_EVEN 0x20
#define SERIAL_PARITY_ODD  0x30
#define SERIAL_1STOPBIT    0x00
#define SERIAL_2STOPBITS   0x08

typedef struct serial_device {
    void (*set_line_speed)(uint32_t bps);
    void (*set_line_params)(int flags);
    void (*receive_data)(uint8_t value);
} *serial_device_t;

void serial_attach_device( serial_device_t dev );
void serial_detach_device( );

void serial_transmit_data( char *data, int length );
void serial_transmit_break( void );

#ifdef __cplusplus
}
#endif

#endif
