/**
 * $Id$
 *
 * Glib-based networking support functions. Currently this is just for activity callbacks.
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

#include <assert.h>
#include <glib.h>
#include <stdlib.h>
#include "ioutil.h"

struct io_glib_cbinfo {
    io_callback_t callback;
    guint sourceid;
    void * cbdata;
    void (*cbdealloc)(void *);
};

static gboolean io_glib_callback( GIOChannel *source, GIOCondition cond, gpointer data )
{
    struct io_glib_cbinfo *cbinfo = (struct io_glib_cbinfo *)data;
    return cbinfo->callback( g_io_channel_unix_get_fd(source), cbinfo->cbdata);
}

static void io_glib_release( void *data )
{
    struct io_glib_cbinfo *cbinfo = (struct io_glib_cbinfo *)data;
    if( cbinfo->cbdealloc ) {
        cbinfo->cbdealloc( cbinfo->cbdata );
    }
    free(cbinfo);
}

/**
 * Register a TCP server socket listener on an already open (and listening) 
 * socket. The socket must not have been previously registered.
 * @return TRUE on success, FALSE on failure.
 * 
 * Defined in netutil.h
 */ 
io_listener_t io_register_tcp_listener( int fd, io_callback_t callback, void *data, void (*dealloc)(void*) )
{
    return io_register_listener( fd, callback, data, dealloc );
}

io_listener_t io_register_listener( int fd, io_callback_t callback, void *data, void (*dealloc)(void *) )
{
    struct io_glib_cbinfo *cbinfo = malloc( sizeof(struct io_glib_cbinfo) );
    assert(cbinfo != NULL);
    
    cbinfo->callback = callback;
    cbinfo->cbdata = data;
    cbinfo->cbdealloc = dealloc;

    /**
     * Note magic here: the watch creates an event source which holds a 
     * reference to the channel. We unref the channel so that the channel then
     * is automatically released when the event source goes away.
     */
    GIOChannel *chan = g_io_channel_unix_new(fd);
    g_io_channel_set_encoding( chan, NULL, NULL );
    g_io_channel_set_buffered(chan, FALSE);
    cbinfo->sourceid = g_io_add_watch_full( chan, 0, G_IO_IN, io_glib_callback, cbinfo, io_glib_release );
    g_io_channel_unref( chan );
    return cbinfo;
}


void io_unregister_listener( io_listener_t data )
{
    struct io_glib_cbinfo *cbinfo = (struct io_glib_cbinfo *)data;
    g_source_remove(cbinfo->sourceid);
}
