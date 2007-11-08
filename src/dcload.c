/**
 * $Id: dcload.c,v 1.8 2007-11-08 11:54:16 nkeynes Exp $
 * 
 * DC-load syscall implementation.
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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "dream.h"
#include "mem.h"
#include "dreamcast.h"
#include "syscall.h"
#include "sh4/sh4core.h"

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_CREAT 4
#define SYS_LINK 5
#define SYS_UNLINK 6
#define SYS_CHDIR 7
#define SYS_CHMOD 8
#define SYS_LSEEK 9
#define SYS_FSTAT 10
#define SYS_TIME 11
#define SYS_STAT 12
#define SYS_UTIME 13
#define SYS_ASSIGNWRKMEM 14
#define SYS_EXIT 15
#define SYS_OPENDIR 16
#define SYS_CLOSEDIR 17
#define SYS_READDIR 18
#define SYS_GETHOSTINFO 19

#define SYS_MAGIC 0xDEADBEEF
#define SYS_MAGIC_ADDR 0x8c004004
#define SYSCALL_ADDR 0x8c004008

static gboolean dcload_allow_unsafe = FALSE;

void dcload_set_allow_unsafe( gboolean allow )
{
    dcload_allow_unsafe = allow;
}

#define MAX_OPEN_FDS 16
/**
 * Mapping from emulator fd to real fd (so we can limit read/write
 * to only fds we've explicitly granted access to).
 */
int open_fds[MAX_OPEN_FDS];

int dcload_alloc_fd() 
{
    int i;
    for( i=0; i<MAX_OPEN_FDS; i++ ) {
	if( open_fds[i] == -1 ) {
	    return i;
	}
    }
    return -1;
}

void dcload_syscall( uint32_t syscall_id ) 
{
    // uint32_t syscall = sh4r.r[4];
    int fd;
    switch( sh4r.r[4] ) {
    case SYS_READ:
	fd = sh4r.r[5];
	if( fd < 0 || fd >= MAX_OPEN_FDS || open_fds[fd] == -1 ) {
	    sh4r.r[0] = -1;
	} else {
	    sh4ptr_t buf = mem_get_region( sh4r.r[6] );
	    int length = sh4r.r[7];
	    sh4r.r[0] = read( open_fds[fd], buf, length );
	}
	break;
    case SYS_WRITE:
	fd = sh4r.r[5];
	if( fd < 0 || fd >= MAX_OPEN_FDS || open_fds[fd] == -1 ) {
	    sh4r.r[0] = -1;
	} else {
	    sh4ptr_t buf = mem_get_region( sh4r.r[6] );
	    int length = sh4r.r[7];
	    sh4r.r[0] = write( open_fds[fd], buf, length );
	}
	break;
    case SYS_LSEEK:
	fd = sh4r.r[5];
	if( fd < 0 || fd >= MAX_OPEN_FDS || open_fds[fd] == -1 ) {
	    sh4r.r[0] = -1;
	} else {
	    sh4r.r[0] = lseek( open_fds[fd], sh4r.r[6], sh4r.r[7] );
	}
	break;

/* Secure access only */
    case SYS_OPEN:
	if( dcload_allow_unsafe ) {
	    fd = dcload_alloc_fd();
	    if( fd == -1 ) {
		sh4r.r[0] = -1;
	    } else {
		char *filename = (char *)mem_get_region( sh4r.r[5] );
		int realfd = open( filename, sh4r.r[6] );
		open_fds[fd] = realfd;
		sh4r.r[0] = realfd;
	    }
	} else {
	    ERROR( "Denying access to local filesystem" );
	    sh4r.r[0] = -1;
	}
	break;
    case SYS_CLOSE:
	if( dcload_allow_unsafe ) {
	    fd = sh4r.r[5];
	    if( fd < 0 || fd >= MAX_OPEN_FDS || open_fds[fd] == -1 ) {
		sh4r.r[0] = -1;
	    } else {
		if( open_fds[fd] > 2 ) {
		    sh4r.r[0] = close( open_fds[fd] );
		} else {
		    /* Don't actually close real fds 0-2 */
		    sh4r.r[0] = 0;
		}
		open_fds[fd] = -1;
	    }
	}
	break;
    case SYS_EXIT:
	if( dcload_allow_unsafe ) {
	    exit( sh4r.r[5] );
	} else {
	    dreamcast_stop();
	}
    default:
	sh4r.r[0] = -1;
    }

}

void dcload_install() 
{
    memset( &open_fds, -1, sizeof(open_fds) );
    open_fds[0] = 0;
    open_fds[1] = 1;
    open_fds[2] = 2;
    syscall_add_hook_vector( 0xF0, SYSCALL_ADDR, dcload_syscall );
    sh4_write_long( SYS_MAGIC_ADDR, SYS_MAGIC );
}
