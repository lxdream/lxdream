/**
 * $Id$
 *
 * OS X networking support functions. Currently this is just for activity callbacks.
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

#include <CoreFoundation/CoreFoundation.h>
#include "netutil.h"

struct net_osx_cbinfo {
    net_callback_t callback;
    void * cbdata;
    void (*cbdealloc)(void *);
    CFSocketRef sockRef;
    CFRunLoopSourceRef sourceRef;
};

static void net_osx_callback( CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *unused, void *data )
{
    struct net_osx_cbinfo *cbinfo = (struct net_osx_cbinfo *)data;
    if(!cbinfo->callback( CFSocketGetNative(s), cbinfo->cbdata) ) {
        CFRunLoopRemoveSource( CFRunLoopGetCurrent(), cbinfo->sourceRef, kCFRunLoopCommonModes );
        CFRelease(cbinfo->sourceRef);
        cbinfo->sourceRef = NULL;
        CFRelease(cbinfo->sockRef);
    }
}

static void net_osx_release( const void *data )
{
    struct net_osx_cbinfo *cbinfo = (struct net_osx_cbinfo *)data;
    if( cbinfo->cbdealloc != NULL ) {
        cbinfo->cbdealloc(cbinfo->cbdata);
    }
    free( cbinfo );
}

/**
 * Register a TCP server socket listener on an already open (and listening) 
 * socket. The socket must not have been previously registered.
 * @return TRUE on success, FALSE on failure.
 * 
 * Defined in netutil.h
 */ 
gboolean net_register_tcp_listener( int fd, net_callback_t callback, void *data, void (*dealloc)(void*) )
{
    CFSocketContext socketContext;
    struct net_osx_cbinfo *cbinfo = malloc( sizeof(struct net_osx_cbinfo) );
    assert(cbinfo != NULL);
    
    cbinfo->callback = callback;
    cbinfo->cbdata = data;
    cbinfo->cbdealloc = dealloc;
    socketContext.version = 0;
    socketContext.info = cbinfo;
    socketContext.retain = NULL;
    socketContext.release = net_osx_release;
    socketContext.copyDescription = NULL;
    
    cbinfo->sockRef = CFSocketCreateWithNative( kCFAllocatorDefault, fd, kCFSocketReadCallBack, 
            net_osx_callback, &socketContext );
    cbinfo->sourceRef = CFSocketCreateRunLoopSource( kCFAllocatorDefault, cbinfo->sockRef, 0 );
    CFRunLoopAddSource( CFRunLoopGetCurrent(), cbinfo->sourceRef, kCFRunLoopCommonModes );

    return TRUE;
}
