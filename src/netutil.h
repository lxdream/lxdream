/**
 * $Id$
 * 
 * GDB RDP server stub - SH4 + ARM
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

#ifndef lxdream_netutil_H
#define lxdream_netutil_H 1

#include <netinet/in.h>
#include "lxdream.h"

/**
 * Construct a server socket listening on the given interface and port. If port
 * is 0, a dynamic port will be bound instead. 
 * This method does not register a listener.
 * @return newly created socket fd, or -1 on failure.
 */ 
int net_create_server_socket(const char *interface, int port );

/**
 * Callback invoked when data is available from the remote peer, or when the peer
 * connects/disconnects.
 * 
 * @param fd     file descriptor of the connected socket
 * @param data   data supplied when the callback was registered
 * @return TRUE to maintain the connection, FALSE to immediately disconnected + close.
 */
typedef gboolean (*net_callback_t)( int fd, void *data );

/**
 * Register a TCP server socket listener on an already open (and listening) 
 * socket. The socket must not have been previously registered.
 * @return TRUE on success, FALSE on failure.
 * 
 * Note: Implementation is platform specific
 */ 
gboolean net_register_tcp_listener( int fd, net_callback_t callback, void *data, void (*dealloc)(void*) );

/**
 * Unregister a socket that was previously registered with the system. This
 * does not close the socket, but will remove any callbacks associated with the socket.
 */ 
gboolean net_unregister_listener( int fd );

#endif /* !lxdream_netutil_H */
