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
#include <glib.h>
#include <elf.h>
#include "mem.h"
#include "bootstrap.h"
#include "dreamcast.h"
#include "config.h"
#include "loader.h"
#include "drivers/cdrom/cdrom.h"
#include "drivers/cdrom/isofs.h"
#include "gdrom/gdrom.h"

const char bootstrap_magic[32] = "SEGA SEGAKATANA SEGA ENTERPRISES";
const char iso_magic[6] = "\001CD001";
char *file_loader_extensions[][2] = { 
        { "sbi", "Self Boot Inducer" },
        { "bin", "SH4 Bin file" },
        { NULL, NULL } };

static cdrom_disc_t cdrom_wrap_elf( cdrom_disc_type_t type, const gchar *filename, int fd, ERROR *err );
static cdrom_disc_t cdrom_wrap_binary( cdrom_disc_type_t type, const gchar *filename, int fd, ERROR *err );
static gboolean file_load_binary( const gchar *filename, int fd, ERROR *err );
static gboolean file_load_elf( const gchar *filename, int fd, ERROR *err );



lxdream_file_type_t file_identify( const gchar *filename, int fd, ERROR *err )
{
    char buf[32];
    lxdream_file_type_t result = FILE_UNKNOWN;
    gboolean mustClose = FALSE;
    off_t posn;

    if( fd == -1 ) {
        fd = open( filename, O_RDONLY );
        if( fd == -1 ) {
            SET_ERROR( err, LX_ERR_FILE_NOOPEN, "Unable to open file '%s' (%s)" ,filename, strerror(errno) );
            return FILE_ERROR;
        }
        mustClose = TRUE;
    } else {
        /* Save current file position */
        posn = lseek(fd, 0, SEEK_CUR);
        if( posn == -1 ) {
            SET_ERROR( err, LX_ERR_FILE_IOERROR, "Unable to read from file '%s' (%s)", filename, strerror(errno) );
            return FILE_ERROR;
        }
    }

    int status = read(fd, buf, 32);
    if( status == -1 ) {
        SET_ERROR( err, LX_ERR_FILE_IOERROR, "Unable to read from file '%s' (%s)", filename, strerror(errno) );
        result = FILE_ERROR;
    } else if( status != 32 ) {
        result = FILE_UNKNOWN;
    } else if( buf[0] == 0x7F && buf[1] == 'E' &&
            buf[2] == 'L' && buf[3] == 'F' ) {
        result = FILE_ELF;
    } else if( memcmp( buf, "PK\x03\x04", 4 ) == 0 ) {
        result = FILE_ZIP;
    } else if( memcmp( buf, DREAMCAST_SAVE_MAGIC, 16 ) == 0 ) {
        result = FILE_SAVE_STATE;
    } else if( lseek( fd, 32768, SEEK_SET ) == 32768 &&
            read( fd, buf, 8 ) == 8 &&
            memcmp( buf, iso_magic, 6) == 0 ) {
        result = FILE_ISO;
    } else {
        /* Check the file extension - .bin = sh4 binary */
        int len = strlen(filename);
        struct stat st;

        if( len > 4 && strcasecmp(filename + (len-4), ".bin") == 0 &&
            fstat(fd, &st) != -1 && st.st_size <= BINARY_MAX_SIZE ) {
            result = FILE_BINARY;
        }
    }

    if( mustClose ) {
        close(fd);
    } else {
        lseek( fd, posn, SEEK_SET );
    }
    return result;
}


gboolean file_load_exec( const gchar *filename, ERROR *err )
{
    gboolean result = TRUE;

    int fd = open( filename, O_RDONLY );
    if( fd == -1 ) {
        SET_ERROR( err, LX_ERR_FILE_NOOPEN, "Unable to open file '%s' (%s)" ,filename, strerror(errno) );
        return FALSE;
    }

    lxdream_file_type_t type = file_identify(filename, fd, err);
    switch( type ) {
    case FILE_ERROR:
        result = FALSE;
        break;
    case FILE_ELF:
        result = file_load_elf( filename, fd, err );
        break;
    case FILE_BINARY:
        result = file_load_binary( filename, fd, err );
        break;
    default:
        SET_ERROR( err, LX_ERR_FILE_UNKNOWN, "File '%s' could not be recognized as an executable binary", filename );
        result = FALSE;
        break;
    }

    close(fd);
    return result;
}

lxdream_file_type_t file_load_magic( const gchar *filename, gboolean wrap_exec, ERROR *err )
{
    gboolean result;
    /* Try disc types first */
    cdrom_disc_t disc = cdrom_disc_open( filename, err );
    if( disc != NULL ) {
        gdrom_mount_disc(disc);
        return FILE_DISC;
    } else if( !IS_ERROR_CODE(err,LX_ERR_FILE_UNKNOWN) ) {
        return FILE_ERROR;
    }

    int fd = open( filename, O_RDONLY );
    if( fd == -1 ) {
        SET_ERROR( err, LX_ERR_FILE_NOOPEN, "Unable to open file '%s' (%s)" ,filename, strerror(errno) );
        return FILE_ERROR;
    }

    lxdream_file_type_t type = file_identify(filename, fd, err);
    switch( type ) {
    case FILE_ERROR:
        result = FALSE;
        break;
    case FILE_ELF:
        if( wrap_exec ) {
            disc = cdrom_wrap_elf( CDROM_DISC_XA, filename, fd, err );
            result = disc != NULL;
            if( disc != NULL ) {
                gdrom_mount_disc(disc);
            }
        } else {
            result = file_load_elf( filename, fd, err );
        }
        break;
    case FILE_BINARY:
        if( wrap_exec ) {
            disc = cdrom_wrap_binary( CDROM_DISC_XA, filename, fd, err );
            result = disc != NULL;
            if( disc != NULL ) {
                gdrom_mount_disc(disc);
            }
        } else {
            result = file_load_binary( filename, fd, err );
        }
        break;
    case FILE_SAVE_STATE:
        result = dreamcast_load_state( filename );
        break;
    case FILE_ZIP:
        SET_ERROR( err, LX_ERR_FILE_UNSUP, "ZIP/SBI not currently supported" );
        result = FALSE;
        break;
    case FILE_ISO:
        SET_ERROR( err, LX_ERR_FILE_UNSUP, "ISO files are not currently supported" );
        result = FALSE;
        break;
    default:
        SET_ERROR( err, LX_ERR_FILE_UNKNOWN, "File '%s' could not be recognized", filename );
        result = FALSE;
        break;
    }
    close(fd);
    if( result ) {
        CLEAR_ERROR(err);
        return type;
    }
    return FILE_ERROR;
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

static gboolean is_arch( Elf32_Ehdr *head, Elf32_Half mach ) {
    return ( head->e_ident[EI_CLASS] == ELFCLASS32 &&
            head->e_ident[EI_DATA] == ELFDATA2LSB &&
            head->e_ident[EI_VERSION] == 1 &&
            head->e_type == ET_EXEC &&
            head->e_machine == mach &&
            head->e_version == 1 );
}

#define is_sh4_elf(head) is_arch(head, EM_SH)
#define is_arm_elf(head) is_arch(head, EM_ARM)

static gboolean file_load_elf( const gchar *filename, int fd, ERROR *err )
{
    Elf32_Ehdr head;
    Elf32_Phdr phdr;
    int i;

    if( read( fd, &head, sizeof(head) ) != sizeof(head) )
        return FALSE;
    if( !is_sh4_elf(&head) ) {
        SET_ERROR( err, LX_ERR_FILE_INVALID, "File is not an SH4 ELF executable file" );
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
        }
    }

    file_load_postload( filename, head.e_entry );
    return TRUE;
}

static gboolean file_load_binary( const gchar *filename, int fd, ERROR *err )
{
    struct stat st;

    if( fstat( fd, &st ) == -1 ) {
        SET_ERROR( err, LX_ERR_FILE_IOERROR, "Error reading binary file '%s' (%s)", filename, strerror(errno) );
        return FALSE;
    }

    if( st.st_size > BINARY_MAX_SIZE ) {
        SET_ERROR( err, LX_ERR_FILE_INVALID, "Binary file '%s' is too large to fit in memory", filename );
        return FALSE;
    }

    sh4ptr_t target = mem_get_region( BINARY_LOAD_ADDR );
    if( read( fd, target, st.st_size ) != st.st_size ) {
        SET_ERROR( err, LX_ERR_FILE_IOERROR, "Error reading binary file '%s' (%s)", filename, strerror(errno) );
        return FALSE;
    }

    file_load_postload( filename, BINARY_LOAD_ADDR );
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

    /* 1. Load in the bootstrap: Note failures here are considered configuration errors */
    gchar *bootstrap_file = lxdream_get_global_config_path_value(CONFIG_BOOTSTRAP);
    if( bootstrap_file == NULL || bootstrap_file[0] == '\0' ) {
        g_free(data);
        SET_ERROR( err, LX_ERR_CONFIG, "Unable to create CD image: bootstrap file is not configured" );
        return NULL;
    }

    FILE *f = fopen( bootstrap_file, "ro" );
    if( f == NULL ) {
        g_free(data);
        SET_ERROR( err, LX_ERR_CONFIG, "Unable to create CD image: bootstrap file '%s' could not be opened", bootstrap_file );
        return FALSE;
    }
    size_t len = fread( bootstrap, 1, 32768, f );
    fclose(f);
    if( len != 32768 ) {
        g_free(data);
        SET_ERROR( err, LX_ERR_CONFIG, "Unable to create CD image: bootstrap file '%s' is invalid", bootstrap_file );
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
        SET_ERROR( err, LX_ERR_NOMEM, "Unable to create CD image: out of memory" );
        return NULL;
    }

    IsoStream *stream;
    if( iso_mem_stream_new(data, bin_size, &stream) != 1 ) {
        g_free(data);
        iso_image_unref(iso);
        SET_ERROR( err, LX_ERR_NOMEM, "Unable to create CD image: out of memory" );
        return NULL;
    }
    iso_tree_add_new_file(iso_image_get_root(iso), "1ST_READ.BIN", stream, NULL);
    sector_source_t track = iso_sector_source_new( iso, SECTOR_MODE2_FORM1, start_lba,
            bootstrap, err );
    if( track == NULL ) {
        iso_image_unref(iso);
        return NULL;
    }

    cdrom_disc_t disc = cdrom_disc_new_from_track( type, track, start_lba, err );
    iso_image_unref(iso);
    if( disc != NULL ) {
        disc->name = g_strdup(filename);
    } 
    return disc;
}

static cdrom_disc_t cdrom_wrap_elf( cdrom_disc_type_t type, const gchar *filename, int fd, ERROR *err )
{
    Elf32_Ehdr head;
    int i;

    /* Check the file header is actually an SH4 binary */
    if( read( fd, &head, sizeof(head) ) != sizeof(head) )
        return FALSE;

    if( !is_sh4_elf(&head) ) {
        SET_ERROR( err, LX_ERR_FILE_INVALID, "File is not an SH4 ELF executable file" );
        return FALSE;
    }
    if( head.e_entry != BINARY_LOAD_ADDR ) {
        SET_ERROR( err, LX_ERR_FILE_INVALID, "SH4 Binary has incorrect entry point (should be %08X but is %08X)", BINARY_LOAD_ADDR, head.e_entry );
        return FALSE;
    }

    /* Load the program headers */
    Elf32_Phdr phdr[head.e_phnum];
    lseek( fd, head.e_phoff, SEEK_SET );
    if( read( fd, phdr, sizeof(phdr) ) != sizeof(phdr) ) {
        SET_ERROR( err, LX_ERR_FILE_INVALID, "File is not a valid executable file" );
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
        SET_ERROR( err, LX_ERR_FILE_INVALID, "SH4 Binary has incorrect load address (should be %08X but is %08X)", BINARY_LOAD_ADDR, start );
        return FALSE;
    }
    if( end >= 0x8D000000 ) {
        SET_ERROR( err, LX_ERR_FILE_INVALID, "SH4 binary is too large to fit in memory (end address is %08X)", end );
        return FALSE;
    }

    /* Load the program into memory */
    unsigned char *program = g_malloc0( end-start );
    for( i=0; i<head.e_phnum; i++ ) {
        if( phdr[i].p_type == PT_LOAD ) {
            lseek( fd, phdr[i].p_offset, SEEK_SET );
            uint32_t size = MIN( phdr[i].p_filesz, phdr[i].p_memsz);
            int status = read( fd, program + phdr[i].p_vaddr - start, size );
            if( status == -1 ) {
                SET_ERROR( err, LX_ERR_FILE_IOERROR, "I/O error reading SH4 binary %s (%s)", filename, strerror(errno) );
            } else if( status != size ) {
                SET_ERROR( err, LX_ERR_FILE_IOERROR, "SH4 binary %s is corrupt", filename );
            }            
        }
    }

    /* And finally pass it over to the disc wrapper */
    return cdrom_disc_new_wrapped_binary(type, filename, program, end-start, err );
}

static cdrom_disc_t cdrom_wrap_binary( cdrom_disc_type_t type, const gchar *filename, int fd, ERROR *err )
{
    struct stat st;
    unsigned char *data;
    size_t len;

    if( fstat(fd, &st) == -1 ) {
        SET_ERROR( err, LX_ERR_FILE_IOERROR, "Error reading binary file '%s' (%s)", filename, strerror(errno) );
        return NULL;
    }

    data = g_malloc(st.st_size);
    len = read( fd, data, st.st_size );
    if( len != st.st_size ) {
        SET_ERROR( err, LX_ERR_FILE_IOERROR, "Error reading binary file '%s' (%s)", filename, strerror(errno) );
        free(data);
        return NULL;
    }

    return cdrom_disc_new_wrapped_binary( type, filename, data, st.st_size, err );
}

cdrom_disc_t cdrom_wrap_magic( cdrom_disc_type_t type, const gchar *filename, ERROR *err )
{
    cdrom_disc_t disc = NULL;

    int fd = open( filename, O_RDONLY );
    if( fd == -1 ) {
        SET_ERROR( err, LX_ERR_FILE_NOOPEN, "Unable to open file '%s'", filename );
        return NULL;
    }

    lxdream_file_type_t filetype = file_identify( filename, fd, err );
    switch( filetype ) {
    case FILE_ELF:
        disc = cdrom_wrap_elf(type, filename, fd, err);
        break;
    case FILE_BINARY:
        disc = cdrom_wrap_binary(type, filename, fd, err);
        break;
    default:
        SET_ERROR( err, LX_ERR_FILE_UNKNOWN, "File '%s' cannot be wrapped (not a recognized binary)", filename );
        break;
    }

    close(fd);
    return disc;

}
