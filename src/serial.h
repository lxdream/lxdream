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
#ifndef lxdream_serial_H
#define lxdream_serial_H 1

#include <stdint.h>

#include "lxdream.h"

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
    void (*attach)(struct serial_device *dev);
    void (*detach)(struct serial_device *dev);
    void (*destroy)(struct serial_device *dev);
    void (*set_line_speed)(struct serial_device *dev, uint32_t bps);
    void (*set_line_params)(struct serial_device *dev, int flags);
    void (*receive_data)(struct serial_device *dev, uint8_t value);
} *serial_device_t;

serial_device_t serial_attach_device( serial_device_t dev );
serial_device_t serial_detach_device( );
serial_device_t serial_get_device( );

/**
 * Destroy a serial device.
 */
void serial_destroy_device( serial_device_t dev );


void serial_transmit_data( char *data, int length );
void serial_transmit_break( void );

/**
 * Create a serial device on a host device identified by the given
 * file (ie /dev/tty). If filename identifies a regular file, it is opened
 * for output only.
 */
serial_device_t serial_fd_device_new_filename( const gchar *filename );

/**
 * Create a serial device on the host console (stdin/stdout).
 */
serial_device_t serial_fd_device_new_console();

/**
 * Create a serial device on a pair of file streams (in and out)
 */
serial_device_t serial_fd_device_new_file( FILE *in, FILE *out, gboolean closeOnDestroy );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_serial_H */
