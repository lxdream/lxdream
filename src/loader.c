/**
 * $Id$
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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <elf.h>
#include "mem.h"
#include "bootstrap.h"
#include "dreamcast.h"
#include "config.h"
#include "loader.h"

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

gboolean file_load_elf_fd( const gchar *filename, int fd );


gboolean file_load_magic( const gchar *filename )
{
    char buf[32];
    struct stat st;
    gboolean result = TRUE;

    int fd = open( filename, O_RDONLY );
    if( fd == -1 ) {
        return FALSE;
    }

    fstat( fd, &st );

    /* begin magic */
    if( read( fd, buf, 32 ) != 32 ) {
        ERROR( "Unable to read from file '%s'", filename );
        close(fd);
        return FALSE;
    }
    if( memcmp( buf, bootstrap_magic, 32 ) == 0 ) {
        /* we have a DC bootstrap */
        if( st.st_size == BOOTSTRAP_SIZE ) {
            sh4ptr_t load = mem_get_region( BOOTSTRAP_LOAD_ADDR );
            lseek( fd, 0, SEEK_SET );
            read( fd, load, BOOTSTRAP_SIZE );
            bootstrap_dump( load, TRUE );
            dreamcast_program_loaded( filename, BOOTSTRAP_LOAD_ADDR + 0x300 );
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
        result = FALSE;
    } else if( memcmp( buf, DREAMCAST_SAVE_MAGIC, 16 ) == 0 ) {
        /* Save state */
        result = (dreamcast_load_state( filename )==0);
    } else if( buf[0] == 0x7F && buf[1] == 'E' && 
            buf[2] == 'L' && buf[3] == 'F' ) {
        /* ELF binary */
        lseek( fd, 0, SEEK_SET );
        result = file_load_elf_fd( filename, fd );
    } else {
        result = FALSE;
    }
    close(fd);
    return result;
}

void file_load_postload( const gchar *filename, int pc )
{
    const gchar *bootstrap_file = lxdream_get_config_value(CONFIG_BOOTSTRAP);
    if( bootstrap_file != NULL ) {
        /* Load in a bootstrap before the binary, to initialize everything
         * correctly
         */
        if( mem_load_block( bootstrap_file, BOOTSTRAP_LOAD_ADDR, BOOTSTRAP_SIZE ) == 0 ) {
            dreamcast_program_loaded( filename, BOOTSTRAP_LOAD_ADDR+0x300 );
            return;
        }
    }
    dreamcast_program_loaded( filename, pc );
}    


gboolean file_load_binary( const gchar *filename )
{
    /* Load the binary itself */
    if(  mem_load_block( filename, BINARY_LOAD_ADDR, -1 ) == 0 ) {
        file_load_postload( filename, BINARY_LOAD_ADDR );
        return TRUE;
    } else {
        return FALSE;
    }
}

gboolean file_load_elf_fd( const gchar *filename, int fd ) 
{
    Elf32_Ehdr head;
    Elf32_Phdr phdr;
    int i;

    if( read( fd, &head, sizeof(head) ) != sizeof(head) )
        return FALSE;
    if( head.e_ident[EI_CLASS] != ELFCLASS32 ||
            head.e_ident[EI_DATA] != ELFDATA2LSB ||
            head.e_ident[EI_VERSION] != 1 ||
            head.e_type != ET_EXEC ||
            head.e_machine != EM_SH ||
            head.e_version != 1 ) {
        ERROR( "File is not an SH4 ELF executable file" );
        return FALSE;
    }

    /* Program headers */
    for( i=0; i<head.e_phnum; i++ ) {
        lseek( fd, head.e_phoff + i*head.e_phentsize, SEEK_SET );
        read( fd, &phdr, sizeof(phdr) );
        if( phdr.p_type == PT_LOAD ) {
            lseek( fd, phdr.p_offset, SEEK_SET );
            sh4ptr_t target = mem_get_region( phdr.p_vaddr );
            read( fd, target, phdr.p_filesz );
            if( phdr.p_memsz > phdr.p_filesz ) {
                memset( target + phdr.p_filesz, 0, phdr.p_memsz - phdr.p_filesz );
            }
            INFO( "Loaded %d bytes to %08X", phdr.p_filesz, phdr.p_vaddr );
        }
    }

    file_load_postload( filename, head.e_entry );
    return TRUE;
}
