/**
 * $Id: dcload.c,v 1.3 2006-03-20 11:59:57 nkeynes Exp $
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
#include "dream.h"
#include "mem.h"
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

void dcload_syscall( uint32_t syscall_id ) 
{
    uint32_t syscall = sh4r.r[4];
    switch( sh4r.r[4] ) {
    case SYS_READ:
	if( sh4r.r[5] == 0 ) {
	    char *buf = mem_get_region( sh4r.r[6] );
	    int length = sh4r.r[7];
	    sh4r.r[0] = read( 0, buf, length );
	} else {
	    sh4r.r[0] = -1;
	}
	break;
    case SYS_WRITE:
	if( sh4r.r[5] == 1 || sh4r.r[5] == 2 ) {
	    char *buf = mem_get_region( sh4r.r[6] );
	    int length = sh4r.r[7];
	    sh4r.r[0] = write( sh4r.r[5], buf, length );
	} else {
	    sh4r.r[0] = -1;
	}
	break;
    case SYS_EXIT:
	/* exit( sh4r.r[4] ); */
	dreamcast_stop();
    default:
	sh4r.r[0] = -1;
    }

}

void dcload_install() 
{
    syscall_add_hook_vector( 0xF0, SYSCALL_ADDR, dcload_syscall );
    sh4_write_long( SYS_MAGIC_ADDR, SYS_MAGIC );
}
