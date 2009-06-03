/**
 * $Id$
 * 
 * Network support functions
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

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "netutil.h"

int net_create_server_socket(const char *interface, int port )
{
    struct sockaddr_in sin;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if( fd == -1 ) {
        ERROR( "Failed to create TCP socket!" );
        return -1;
    }
    
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(port);
    
    if( interface != NULL ) {
        if( !inet_aton(interface, &sin.sin_addr) ) {
            /* TODO: hostname lookup */
        }
    }

    if( bind( fd, (struct sockaddr *)&sin, sizeof(sin) ) != 0 ||
            listen(fd, 5) != 0 ) {
        close(fd);
        ERROR( "Failed to bind port %d (%s)", port, strerror(errno) );
        return -1;
    }
    return fd;
}

