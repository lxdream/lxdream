/**
 * $Id$
 *
 * Host driver for a serial port attachment, that can be hooked to a character
 * device, fifo or named pipe.
 *
 * Copyright (c) 2009 Nathan Keynes.
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "lxdream.h"
#include "config.h"
#include "ioutil.h"
#include "serial.h"


typedef struct serial_fd_device {
    struct serial_device dev;
    FILE *in;
    FILE *out;
    gboolean closeOnDestroy;
    io_listener_t listener;
} *serial_fd_device_t;

static void serial_fd_device_attach(serial_device_t dev);
static void serial_fd_device_detach(serial_device_t dev);
static void serial_fd_device_destroy(serial_device_t dev);
static void serial_fd_device_set_line_speed(struct serial_device *dev, uint32_t bps);
static void serial_fd_device_set_line_params(struct serial_device *dev, int flags);
static void serial_fd_device_receive_data(struct serial_device *dev, uint8_t value);
static gboolean serial_fd_device_transmit_data( int fd, void *dev);

static gboolean serial_config_changed(void *data, struct lxdream_config_group *group, unsigned item,
                                      const gchar *oldval, const gchar *newval);

struct lxdream_config_group serial_group =
    { "serial", serial_config_changed, NULL, NULL,
       {{ "device", N_("Serial device"), CONFIG_TYPE_FILE, "/dev/tty" },
        { NULL, CONFIG_TYPE_NONE }} };

void serial_init()
{
    const gchar *name = serial_group.params[0].value;
    if( name != NULL ) {
        serial_device_t dev = serial_fd_device_new_filename(name);
        if( dev != NULL ) {
            serial_attach_device( dev );
        }
    }
}

static gboolean serial_config_changed(void *data, struct lxdream_config_group *group, unsigned item,
                                      const gchar *oldval, const gchar *newval)
{
    if( item == 0 ) {
        serial_destroy_device(serial_detach_device());
        serial_device_t dev = serial_fd_device_new_filename(newval);
        if( dev != NULL ) {
            serial_attach_device( dev );
        }
    }
    return TRUE;
}



serial_device_t serial_fd_device_new_filename( const gchar *filename )
{
    FILE *out = fopen( filename, "w+" );
    FILE *in;
    struct stat st;

    if( out == NULL ) {
        return NULL;
    }

    if( fstat( fileno(out), &st ) != 0 ) {
        fclose(out);
        return NULL;
    }

    if( S_ISCHR(st.st_mode) || S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode) ) {
        in = out;
    } else {
        in = NULL;
    }

    return (serial_device_t)serial_fd_device_new_file(in, out, TRUE);
}

serial_device_t serial_fd_device_new_console()
{
    return serial_fd_device_new_file( stdin, stdout, FALSE );
}

serial_device_t serial_fd_device_new_file( FILE *in, FILE *out, gboolean closeOnDestroy )
{
    if( in != NULL )
        fcntl( fileno(in), F_SETFL, O_NONBLOCK );

    serial_fd_device_t dev = (serial_fd_device_t)malloc(sizeof(struct serial_fd_device));
    if( dev == NULL ) {
        if( closeOnDestroy ) {
            if( in != NULL )
                fclose(in);
            if( out != NULL && out != in )
                fclose(out);
        }
        return NULL;
    }

    dev->dev.attach = serial_fd_device_attach;
    dev->dev.detach = serial_fd_device_detach;
    dev->dev.destroy = serial_fd_device_destroy;
    dev->dev.set_line_speed = serial_fd_device_set_line_speed;
    dev->dev.set_line_params = serial_fd_device_set_line_params;
    dev->dev.receive_data = serial_fd_device_receive_data;
    dev->in = in;
    dev->out = out;
    dev->closeOnDestroy = closeOnDestroy;
    return (serial_device_t)dev;
}

static void serial_fd_device_attach(serial_device_t dev)
{
    serial_fd_device_t fddev = (serial_fd_device_t)dev;
    if( fddev->in != NULL )
        fddev->listener = io_register_listener( fileno(fddev->in), serial_fd_device_transmit_data, fddev, NULL );
}

static void serial_fd_device_detach(serial_device_t dev)
{
    serial_fd_device_t fddev = (serial_fd_device_t)dev;
    if( fddev->in != NULL )
        io_unregister_listener( fddev->listener );
}

static void serial_fd_device_destroy(serial_device_t dev)
{
    serial_fd_device_t fddev = (serial_fd_device_t)dev;
    if( fddev->closeOnDestroy ) {
        if( fddev->in != NULL )
            fclose(fddev->in);
        if( fddev->out != NULL && fddev->out != fddev->in )
            fclose(fddev->out);
    }
    fddev->in = NULL;
    fddev->out = NULL;
    free(fddev);
}
static void serial_fd_device_set_line_speed(struct serial_device *dev, uint32_t bps)
{
    /* Do nothing for now */
}
static void serial_fd_device_set_line_params(struct serial_device *dev, int flags)
{
    /* Do nothing for now */
}
static void serial_fd_device_receive_data(struct serial_device *dev, uint8_t value)
{
    serial_fd_device_t fddev = (serial_fd_device_t)dev;
    if( fddev->out != NULL )
        fputc( value, fddev->out );
}

static gboolean serial_fd_device_transmit_data( int fd, void *dev )
{
    serial_fd_device_t fddev = (serial_fd_device_t)dev;
    char buf[4096];
    size_t len = fread(buf, 1, sizeof(buf), fddev->in);
    if( len > 0 ) {
        serial_transmit_data(buf, len);
    }
    return TRUE;
}
