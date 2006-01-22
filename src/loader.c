/**
 * $Id: loader.c,v 1.9 2006-01-22 22:41:40 nkeynes Exp $
 *
 * File loading routines, mostly for loading demos without going through the
 * whole procedure of making a CD image for them.
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
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include "gui/gui.h"
#include "mem.h"
#include "sh4core.h"
#include "bootstrap.h"

char *bootstrap_file = DEFAULT_BOOTSTRAP_FILE;
char bootstrap_magic[32] = "SEGA SEGAKATANA SEGA ENTERPRISES";
char iso_magic[6] = "\001CD001";
char *file_loader_extensions[][2] = { 
    { "sbi", "Self Boot Inducer" },
    { "bin", "SH4 Bin file" },
    { NULL, NULL } };

#define BOOTSTRAP_LOAD_ADDR 0x8C008000
#define BOOTSTRAP_SIZE 32768

#define BINARY_LOAD_ADDR 0x8C010000

#define CDI_V2 0x80000004
#define CDI_V3 0x80000005

gboolean file_load_magic( const gchar *filename )
{
    char buf[32];
    uint32_t tmpa[2];
    struct stat st;
    
    int fd = open( filename, O_RDONLY );
    if( fd == -1 ) {
        ERROR( "Unable to open file: '%s' (%s)", filename,
               strerror(errno) );
        return FALSE;
    }
    
    fstat( fd, &st );
    /*
    if( st.st_size < 32768 ) {
        ERROR( "File '%s' too small to be a dreamcast image", filename );
        close(fd);
        return FALSE;
    }
    */
    
    /* begin magic */
    if( read( fd, buf, 32 ) != 32 ) {
        ERROR( "Unable to read from file '%s'", filename );
        close(fd);
        return FALSE;
    }
    if( memcmp( buf, bootstrap_magic, 32 ) == 0 ) {
        /* we have a DC bootstrap */
        if( st.st_size == BOOTSTRAP_SIZE ) {
            char *load = mem_get_region( BOOTSTRAP_LOAD_ADDR );
            lseek( fd, 0, SEEK_SET );
            read( fd, load, BOOTSTRAP_SIZE );
            bootstrap_dump( load );
            sh4_set_pc( BOOTSTRAP_LOAD_ADDR + 0x300 );
            gtk_gui_update();
        } else {
            /* look for a valid ISO9660 header */
            lseek( fd, 32768, SEEK_SET );
            read( fd, buf, 8 );
            if( memcmp( buf, iso_magic, 6 ) == 0 ) {
                /* Alright, got it */
                INFO( "Loading ISO9660 filesystem from '%s'",
                      filename );
            }
        }
    } else if( memcmp( buf, "PK\x03\x04", 4 ) == 0 ) {
	/* ZIP file, aka SBI file */
	WARN( "SBI files not supported yet" );
    } else if( memcpy( buf, "\x7fELF", 4 ) == 0 ) {
	/* ELF binary */
	WARN( "ELF files not supported yet" );
    } else {
	/* Assume raw binary */
	file_load_binary( filename );
    } 
    close(fd);
    return TRUE;
}

int file_load_binary( const gchar *filename ) {
    /* Load the binary itself */
    mem_load_block( filename, BINARY_LOAD_ADDR, -1 );
    if( bootstrap_file != NULL ) {
	/* Load in a bootstrap before the binary, to initialize everything
	 * correctly
	 */
	mem_load_block( bootstrap_file, BOOTSTRAP_LOAD_ADDR, BOOTSTRAP_SIZE );
	sh4_set_pc( BOOTSTRAP_LOAD_ADDR + 0x300 );
    } else {
	sh4_set_pc( BINARY_LOAD_ADDR );
    }
    gtk_gui_update();
}
