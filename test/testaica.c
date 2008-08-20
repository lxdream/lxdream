/**
 * $Id: testdata.c 602 2008-01-15 20:50:23Z nkeynes $
 * 
 * AICA test loader
 *
 * Copyright (c) 2006 Nathan Keynes.
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
#include <assert.h>

#include "lib.h"
#include "dma.h"

#define AICA_RAM_BASE 0xA0800000

#define AICA_SYSCALL (AICA_RAM_BASE+0x30)
#define AICA_SYSCALL_ARG1 (AICA_SYSCALL+4)
#define AICA_SYSCALL_ARG2 (AICA_SYSCALL+8)
#define AICA_SYSCALL_ARG3 (AICA_SYSCALL+12)
#define AICA_SYSCALL_RETURN (AICA_SYSCALL+4)

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

uint32_t do_syscall( uint32_t syscall, uint32_t arg1, uint32_t arg2, uint32_t arg3 )
{
    uint32_t fd, len;
    char *data;
    char *tmp;
    int rv;

    printf( "Got syscall: %d\n", syscall );
    
    switch( syscall ) {
    case SYS_READ:
        fd = arg1;
        data = (char *)(AICA_RAM_BASE + (arg2 & 0x001FFFFF));
        len = arg3;
        tmp = malloc(len);
        rv = read( fd, data, len );
        if( rv >= 0 ) 
            memcpy_to_aica( data, tmp, rv );
        free(tmp);
        return rv;
    case SYS_WRITE:
        fd = arg1;
        data = (char *)(AICA_RAM_BASE + (arg2 & 0x001FFFFF));
        len = arg3;
        tmp = malloc(len);
        memcpy(tmp, data, len);
        rv = write( fd, tmp, len );
        free(tmp);
        return rv;
    }
    return 0;
}
    

int main( int argc, char *argv[] ) 
{
    char buf[65536] __attribute__((aligned(32)));
    uint32_t aica_addr = AICA_RAM_BASE;
    int len;
    int totallen = 0;
    
    aica_disable();
    /* Load ARM program from stdin and copy to ARM memory */
    while( (len = read(0, buf, sizeof(buf))) > 0 ) {
        if(memcpy_to_aica( aica_addr, buf, len ) != 0 ) {
            printf( "Failed to load program!\n" );
            return 1;
        }
        aica_addr += len;
        totallen += len;
    }
    printf( "Program loaded (%d bytes)\n", totallen);
    
    /* Main loop waiting for IO commands */
    aica_enable();
    do {
        g2_fifo_wait();
        irq_disable();
        int syscall = long_read(AICA_SYSCALL);
        irq_enable();
        if( syscall != -1 ) {
            if( syscall == -2 ) {
                fprintf( stderr, "ARM aborted with general exception\n" );
                return -2;
            } else if( syscall == SYS_EXIT ) {
                printf( "Exiting at ARM request\n" );
                aica_disable();
                return long_read(AICA_SYSCALL_ARG1);
            } else {
                uint32_t result = do_syscall( syscall, long_read(AICA_SYSCALL_ARG1), 
                        long_read(AICA_SYSCALL_ARG2), long_read(AICA_SYSCALL_ARG3) );
                g2_fifo_wait();
                irq_disable();
                long_write( AICA_SYSCALL_RETURN, result );
                long_write( AICA_SYSCALL, -1 );
                irq_enable();
            }
        }
    } while( 1 );
}
