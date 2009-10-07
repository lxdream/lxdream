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
#include "ioutil.h"

struct io_osx_cbinfo {
    int fd;
    io_callback_t callback;
    void * cbdata;
    void (*cbdealloc)(void *);
    void *fdRef;
    CFRunLoopSourceRef sourceRef;

    struct io_osx_cbinfo *next;
};

static struct io_osx_cbinfo *cbinfo_list = NULL;

void io_unregister_callback( struct io_osx_cbinfo *cbinfo )
{
    CFRunLoopRemoveSource( CFRunLoopGetCurrent(), cbinfo->sourceRef, kCFRunLoopCommonModes );
    CFRelease(cbinfo->sourceRef);
    cbinfo->sourceRef = NULL;
    CFRelease(cbinfo->fdRef); /* Note this implicitly releases the cbinfo itself as well */
}

static void io_osx_net_callback( CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *unused, void *data )
{
    struct io_osx_cbinfo *cbinfo = (struct io_osx_cbinfo *)data;
    if(!cbinfo->callback( CFSocketGetNative(s), cbinfo->cbdata) ) {
        io_unregister_callback(cbinfo);
    }
}

static void io_osx_fd_callback( CFFileDescriptorRef f, CFOptionFlags type, void *data )
{
    struct io_osx_cbinfo *cbinfo = (struct io_osx_cbinfo *)data;
    if(!cbinfo->callback( CFFileDescriptorGetNativeDescriptor(f), cbinfo->cbdata) ) {
        io_unregister_callback(cbinfo);
    }
}

static void io_osx_release( const void *data )
{
    struct io_osx_cbinfo *cbinfo = (struct io_osx_cbinfo *)data;
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
io_listener_t io_register_tcp_listener( int fd, io_callback_t callback, void *data, void (*dealloc)(void*) )
{
    CFSocketContext socketContext;
    struct io_osx_cbinfo *cbinfo = malloc( sizeof(struct io_osx_cbinfo) );
    assert(cbinfo != NULL);
    
    cbinfo->callback = callback;
    cbinfo->cbdata = data;
    cbinfo->cbdealloc = dealloc;
    socketContext.version = 0;
    socketContext.info = cbinfo;
    socketContext.retain = NULL;
    socketContext.release = io_osx_release;
    socketContext.copyDescription = NULL;
    
    CFSocketRef ref = CFSocketCreateWithNative( kCFAllocatorDefault, fd, kCFSocketReadCallBack,
            io_osx_net_callback, &socketContext );
    cbinfo->fdRef = ref;
    cbinfo->sourceRef = CFSocketCreateRunLoopSource( kCFAllocatorDefault, ref, 0 );
    CFRunLoopAddSource( CFRunLoopGetCurrent(), cbinfo->sourceRef, kCFRunLoopCommonModes );

    return cbinfo;
}

/**
 * Register a file descriptor listener on an already open (and listening)
 * file descriptor. The file descriptor must not have been previously registered.
 * @return TRUE on success, FALSE on failure.
 *
 */
io_listener_t io_register_listener( int fd, io_callback_t callback, void *data, void (*dealloc)(void *) )
{
    CFFileDescriptorContext fdContext;
    struct io_osx_cbinfo *cbinfo = malloc( sizeof(struct io_osx_cbinfo) );
    assert(cbinfo != NULL);

    cbinfo->callback = callback;
    cbinfo->cbdata = data;
    cbinfo->cbdealloc = dealloc;
    fdContext.version = 0;
    fdContext.retain = NULL;
    fdContext.info = cbinfo;
    fdContext.release = io_osx_release;
    fdContext.copyDescription = NULL;

    CFFileDescriptorRef ref = CFFileDescriptorCreate( kCFAllocatorDefault, fd, FALSE,
            io_osx_fd_callback, &fdContext);
    cbinfo->fdRef = ref;
    cbinfo->sourceRef = CFFileDescriptorCreateRunLoopSource( kCFAllocatorDefault, ref, 0 );
    CFRunLoopAddSource( CFRunLoopGetCurrent(), cbinfo->sourceRef, kCFRunLoopCommonModes );
    return cbinfo;
}

void io_unregister_listener( io_listener_t data )
{
    struct io_osx_cbinfo *cbinfo = (struct io_osx_cbinfo *)data;
    io_unregister_callback(cbinfo);
}
