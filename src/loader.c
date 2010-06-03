/**
 * $Id$
 *
 * File loading routines, mostly for loading demos without going through the
 * whole procedure of manually making a CD image for them.
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
#include <glib/gstrfuncs.h>
#include <elf.h>
#include "mem.h"
#include "bootstrap.h"
#include "dreamcast.h"
#include "config.h"
#include "loader.h"
#include "drivers/cdrom/cdrom.h"
#include "drivers/cdrom/isofs.h"

const char bootstrap_magic[32] = "SEGA SEGAKATANA SEGA ENTERPRISES";
const char iso_magic[6] = "\001CD001";
char *file_loader_extensions[][2] = { 
        { "sbi", "Self Boot Inducer" },
        { "bin", "SH4 Bin file" },
        { NULL, NULL } };

gboolean file_load_elf_fd( const gchar *filename, int fd );

typedef enum {
    FILE_ERROR,
    FILE_BINARY,
    FILE_ELF,
    FILE_ISO,
    FILE_DISC,
    FILE_ZIP,
    FILE_SAVE_STATE,
} lxdream_file_type_t;

static lxdream_file_type_t file_magic( const gchar *filename, int fd, ERROR *err )
{
    char buf[32];

    /* begin magic */
    if( read( fd, buf, 32 ) != 32 ) {
        SET_ERROR( err, errno, "Unable to read from file '%s'", filename );
        return FILE_ERROR;

    }

    lseek( fd, 0, SEEK_SET );
    if( buf[0] == 0x7F && buf[1] == 'E' &&
            buf[2] == 'L' && buf[3] == 'F' ) {
        return FILE_ELF;
    } else if( memcmp( buf, "PK\x03\x04", 4 ) == 0 ) {
        return FILE_ZIP;
    } else if( memcmp( buf, DREAMCAST_SAVE_MAGIC, 16 ) == 0 ) {
        return FILE_SAVE_STATE;
    } else if( lseek( fd, 32768, SEEK_SET ) == 32768 &&
            read( fd, buf, 8 ) == 8 &&
            memcmp( buf, iso_magic, 6) == 0 ) {
        lseek( fd, 0, SEEK_SET );
        return FILE_ISO;
    }
    lseek( fd, 0, SEEK_SET );
    return FILE_BINARY;
}


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
                WARN( "ISO images not supported yet" );
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
    gchar *bootstrap_file = lxdream_get_global_config_path_value(CONFIG_BOOTSTRAP);
    if( bootstrap_file != NULL && bootstrap_file[0] != '\0' ) {
        /* Load in a bootstrap before the binary, to initialize everything
         * correctly
         */
        if( mem_load_block( bootstrap_file, BOOTSTRAP_LOAD_ADDR, BOOTSTRAP_SIZE ) == 0 ) {
            dreamcast_program_loaded( filename, BOOTSTRAP_ENTRY_ADDR );
            g_free(bootstrap_file);
            return;
        }
    }
    dreamcast_program_loaded( filename, pc );
    g_free(bootstrap_file);
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

gboolean is_sh4_elf( Elf32_Ehdr *head )
{
    return ( head->e_ident[EI_CLASS] == ELFCLASS32 &&
            head->e_ident[EI_DATA] == ELFDATA2LSB &&
            head->e_ident[EI_VERSION] == 1 &&
            head->e_type == ET_EXEC &&
            head->e_machine == EM_SH &&
            head->e_version == 1 );
}

gboolean is_arm_elf( Elf32_Ehdr *head )
{
    return ( head->e_ident[EI_CLASS] == ELFCLASS32 &&
            head->e_ident[EI_DATA] == ELFDATA2LSB &&
            head->e_ident[EI_VERSION] == 1 &&
            head->e_type == ET_EXEC &&
            head->e_machine == EM_ARM &&
            head->e_version == 1 );
}

gboolean file_load_elf_fd( const gchar *filename, int fd ) 
{
    Elf32_Ehdr head;
    Elf32_Phdr phdr;
    int i;

    if( read( fd, &head, sizeof(head) ) != sizeof(head) )
        return FALSE;
    if( !is_sh4_elf(&head) ) {
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

/**
 * Create a new CDROM disc containing a single 1ST_READ.BIN.
 * @param type The disc type - must be CDROM_DISC_GDROM or CDROM_DISC_XA
 * @param bin The binary data (takes ownership)
 * @param bin_size
 */
cdrom_disc_t cdrom_disc_new_wrapped_binary( cdrom_disc_type_t type, const gchar *filename, unsigned char *bin, size_t bin_size,
                                            ERROR *err )
{
    IsoImage *iso = NULL;
    unsigned char *data = bin;
    cdrom_lba_t start_lba = 45000; /* GDROM_START */
    char bootstrap[32768];

    /* 1. Load in the bootstrap */
    gchar *bootstrap_file = lxdream_get_global_config_path_value(CONFIG_BOOTSTRAP);
    if( bootstrap_file == NULL || bootstrap_file[0] == '\0' ) {
        g_free(data);
        SET_ERROR( err, ENOENT, "Unable to create CD image: bootstrap file is not configured" );
        return NULL;
    }

    FILE *f = fopen( bootstrap_file, "ro" );
    if( f == NULL ) {
        g_free(data);
        SET_ERROR( err, errno, "Unable to create CD image: bootstrap file '%s' could not be opened", bootstrap_file );
        return FALSE;
    }
    size_t len = fread( bootstrap, 1, 32768, f );
    fclose(f);
    if( len != 32768 ) {
        g_free(data);
        SET_ERROR( err, EINVAL, "Unable to create CD image: bootstrap file '%s' is invalid", bootstrap_file );
        return FALSE;
    }

    /* 2. Scramble the binary if necessary (and set type settings) */
    if( type != CDROM_DISC_GDROM ) {
        /* scramble the binary if we're going the MIL-CD route */
        unsigned char *scramblebin = g_malloc(bin_size);
        bootprogram_scramble( scramblebin, bin, bin_size );
        data = scramblebin;
        start_lba = 0x2DB6; /* CDROM_START (does it matter?) */
        g_free(bin);
    }

    /* 3. Frob the bootstrap data */
    dc_bootstrap_head_t boot_header = (dc_bootstrap_head_t)bootstrap;
    memcpy( boot_header->boot_file, "1ST_READ.BIN    ", 16 );
    char tmp[129];
    int name_len = snprintf( tmp, 129, "lxdream wrapped image: %s", filename );
    if( name_len < 128 )
        memset( tmp+name_len, ' ', 128-name_len );
    memcpy( boot_header->product_name, tmp, 128 );
//    bootstrap_update_crc(bootstrap);


    /* 4. Build the ISO image */
    int status = iso_image_new("autocd", &iso);
    if( status != 1 ) {
        g_free(data);
        return NULL;
    }

    IsoStream *stream;
    if( iso_memory_stream_new(data, bin_size, &stream) != 1 ) {
        g_free(data);
        iso_image_unref(iso);
        return NULL;
    }
    iso_tree_add_new_file(iso_image_get_root(iso), "1ST_READ.BIN", stream, NULL);
    sector_source_t track = iso_sector_source_new( iso, SECTOR_MODE2_FORM1, start_lba,
            bootstrap, err );
    if( track == NULL ) {
        iso_image_unref(iso);
        return NULL;
    }

    cdrom_disc_t disc = cdrom_disc_new_from_track( type, track, start_lba );
    iso_image_unref(iso);
    if( disc != NULL ) {
        disc->name = g_strdup(filename);
    }
    return disc;
}

cdrom_disc_t cdrom_wrap_elf_fd( cdrom_disc_type_t type, const gchar *filename, int fd, ERROR *err )
{
    Elf32_Ehdr head;
    int i;

    /* Check the file header is actually an SH4 binary */
    if( read( fd, &head, sizeof(head) ) != sizeof(head) )
        return FALSE;
    if( !is_sh4_elf(&head) ) {
        SET_ERROR( err, EINVAL, "File is not an SH4 ELF executable file" );
        return FALSE;
    }
    if( head.e_entry != BINARY_LOAD_ADDR ) {
        SET_ERROR( err, EINVAL, "SH4 Binary has incorrect entry point (should be %08X but is %08X)", BINARY_LOAD_ADDR, head.e_entry );
        return FALSE;
    }

    /* Load the program headers */
    Elf32_Phdr phdr[head.e_phnum];
    lseek( fd, head.e_phoff, SEEK_SET );
    if( read( fd, phdr, sizeof(phdr) ) != sizeof(phdr) ) {
        SET_ERROR( err, EINVAL, "File is not a valid executable file" );
        return FALSE;
    }

    sh4addr_t start = (sh4addr_t)-1, end=0;
    /* Scan program headers for memory range in use */
    for( i=0; i<head.e_phnum; i++ ) {
        if( phdr[i].p_type == PT_LOAD ) {
            if( phdr[i].p_vaddr < start )
                start = phdr[i].p_vaddr;
            if( phdr[i].p_vaddr + phdr[i].p_memsz > end )
                end = phdr[i].p_vaddr + phdr[i].p_memsz;
        }
    }

    if( start != BINARY_LOAD_ADDR ) {
        SET_ERROR( err, EINVAL, "SH4 Binary has incorrect load address (should be %08X but is %08X)", BINARY_LOAD_ADDR, start );
        return FALSE;
    }
    if( end >= 0x8D000000 ) {
        SET_ERROR( err, EINVAL, "SH4 binary is too large to fit in memory (end address is %08X)", end );
        return FALSE;
    }

    /* Load the program into memory */
    char *program = g_malloc0( end-start );
    for( i=0; i<head.e_phnum; i++ ) {
        if( phdr[i].p_type == PT_LOAD ) {
            lseek( fd, phdr[i].p_offset, SEEK_SET );
            uint32_t size = MIN( phdr[i].p_filesz, phdr[i].p_memsz);
            read( fd, program + phdr[i].p_vaddr, size );
        }
    }

    /* And finally pass it over to the disc wrapper */
    return cdrom_disc_new_wrapped_binary(type, filename, program, end-start, err );
}

cdrom_disc_t cdrom_wrap_magic( cdrom_disc_type_t type, const gchar *filename, ERROR *err )
{
    cdrom_disc_t disc;
    char *data;
    int len;
    struct stat st;
    int fd = open( filename, O_RDONLY );
    if( fd == -1 ) {
        SET_ERROR( err, errno, "Unable to open file '%s'", filename );
        return NULL;
    }


    lxdream_file_type_t filetype = file_magic( filename, fd, err );
    switch( filetype ) {
    case FILE_BINARY:
        fstat( fd, &st );
        data = g_malloc(st.st_size);
        len = read( fd, data, st.st_size );
        close(fd);
        if( len != st.st_size ) {
            SET_ERROR( err, errno, "Error reading binary file '%s'", filename );
            return NULL;
        }
        return cdrom_disc_new_wrapped_binary( type, filename, data, st.st_size, err );
    case FILE_ELF:
        disc = cdrom_wrap_elf_fd(type, filename, fd, err);
        close(fd);
        return disc;
    default:
        close(fd);
        SET_ERROR( err, EINVAL, "File '%s' cannot be wrapped (not a binary)", filename );
        return NULL;
    }

}
