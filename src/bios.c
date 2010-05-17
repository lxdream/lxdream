/**
 * $Id$
 * 
 * "Fake" BIOS functions, for operation without the actual BIOS.
 *
 * Copyright (c) 2005-2010 Nathan Keynes.
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

#include "dream.h"
#include "mem.h"
#include "syscall.h"
#include "asic.h"
#include "dreamcast.h"
#include "bootstrap.h"
#include "sh4/sh4.h"
#include "drivers/cdrom/cdrom.h"
#include "drivers/cdrom/isofs.h"
#include "gdrom/gdrom.h"

gboolean bios_boot_gdrom_disc( void );

/* Definitions from KOS */
#define COMMAND_QUEUE_LENGTH 16

#define GD_CMD_PIOREAD     16  /* readcd */
#define GD_CMD_DMAREAD     17  /* readcd */
#define GD_CMD_GETTOC      18
#define GD_CMD_GETTOC2     19  /* toc2 */
#define GD_CMD_PLAY        20  /* playcd */
#define GD_CMD_PLAY2       21  /* playcd */
#define GD_CMD_PAUSE       22  /* No params */
#define GD_CMD_RELEASE     23  /* No params */
#define GD_CMD_INIT        24  /* No params */
#define GD_CMD_SEEK        27
#define GD_CMD_READ        28
#define GD_CMD_STOP        33  /* No params */
#define GD_CMD_GETSCD      34
#define GD_CMD_GETSES      35

#define GD_CMD_STATUS_NONE   0
#define GD_CMD_STATUS_ACTIVE 1
#define GD_CMD_STATUS_DONE   2
#define GD_CMD_STATUS_ABORT  3
#define GD_CMD_STATUS_ERROR  4

#define GD_ERROR_OK          0
#define GD_ERROR_NO_DISC     2
#define GD_ERROR_DISC_CHANGE 6
#define GD_ERROR_SYSTEM      1

struct gdrom_toc2_params {
    uint32_t session;
    sh4addr_t buffer;
};

struct gdrom_readcd_params {
    cdrom_lba_t lba;
    cdrom_count_t count;
    sh4addr_t buffer;
    uint32_t unknown;
};

struct gdrom_playcd_params {
    cdrom_lba_t start;
    cdrom_lba_t end;
    uint32_t repeat;
};

typedef union gdrom_cmd_params {
    struct gdrom_toc2_params toc2;
    struct gdrom_readcd_params readcd;
    struct gdrom_playcd_params playcd;
} *gdrom_cmd_params_t;




typedef struct gdrom_queue_entry {
    int status;
    uint32_t cmd_code;
    union gdrom_cmd_params params;
    uint32_t result[4];
} *gdrom_queue_entry_t;

static struct gdrom_queue_entry gdrom_cmd_queue[COMMAND_QUEUE_LENGTH];

static struct bios_gdrom_status {
    uint32_t status;
    uint32_t disk_type;
} bios_gdrom_status;

void bios_gdrom_init( void )
{
    memset( &gdrom_cmd_queue, 0, sizeof(gdrom_cmd_queue) );
}

void bios_gdrom_run_command( gdrom_queue_entry_t cmd )
{
    DEBUG( "BIOS GD command %d", cmd->cmd_code );
    cdrom_error_t status = CDROM_ERROR_OK;
    sh4ptr_t ptr;
    switch( cmd->cmd_code ) {
    case GD_CMD_INIT:
        /* *shrug* */
        cmd->status = GD_CMD_STATUS_DONE;
        break;
    case GD_CMD_GETTOC2:
        ptr = mem_get_region( cmd->params.toc2.buffer );
        status = gdrom_read_toc( ptr );
        if( status == CDROM_ERROR_OK ) {
            /* Convert data to little-endian */
            struct gdrom_toc *toc = (struct gdrom_toc *)ptr;
            for( unsigned i=0; i<99; i++ ) {
                toc->track[i] = ntohl(toc->track[i]);
            }
            toc->first = ntohl(toc->first);
            toc->last = ntohl(toc->last);
            toc->leadout = ntohl(toc->leadout);
        }
        break;
    case GD_CMD_PIOREAD:
    case GD_CMD_DMAREAD:
        ptr = mem_get_region( cmd->params.readcd.buffer );
        status = gdrom_read_cd( cmd->params.readcd.lba,
                cmd->params.readcd.count, 0x28, ptr, NULL );
        break;
    default:
        WARN( "Unknown BIOS GD command %d\n", cmd->cmd_code );
        cmd->status = GD_CMD_STATUS_ERROR;
        cmd->result[0] = GD_ERROR_SYSTEM;
        return;
    }

    switch( status ) {
    case CDROM_ERROR_OK:
        cmd->status = GD_CMD_STATUS_DONE;
        cmd->result[0] = GD_ERROR_OK;
        break;
    case CDROM_ERROR_NODISC:
        cmd->status = GD_CMD_STATUS_ERROR;
        cmd->result[0] = GD_ERROR_NO_DISC;
        break;
    default:
        cmd->status = GD_CMD_STATUS_ERROR;
        cmd->result[0] = GD_ERROR_SYSTEM;
    }
}

uint32_t bios_gdrom_enqueue( uint32_t cmd, sh4addr_t data )
{
    int i;
    for( i=0; i<COMMAND_QUEUE_LENGTH; i++ ) {
        if( gdrom_cmd_queue[i].status != GD_CMD_STATUS_ACTIVE ) {
            gdrom_cmd_queue[i].status = GD_CMD_STATUS_ACTIVE;
            gdrom_cmd_queue[i].cmd_code = cmd;
            switch( cmd ) {
            case GD_CMD_PIOREAD:
            case GD_CMD_DMAREAD:
                mem_copy_from_sh4( (unsigned char *)&gdrom_cmd_queue[i].params.readcd, data, sizeof(struct gdrom_readcd_params) );
                break;
            case GD_CMD_GETTOC2:
                mem_copy_from_sh4( (unsigned char *)&gdrom_cmd_queue[i].params.toc2, data, sizeof(struct gdrom_toc2_params) );
                break;
            case GD_CMD_PLAY:
            case GD_CMD_PLAY2:
                mem_copy_from_sh4( (unsigned char *)&gdrom_cmd_queue[i].params.playcd, data, sizeof(struct gdrom_playcd_params) );
                break;
            }
            return i;
        }
    }
    return -1;
}

void bios_gdrom_run_queue( void ) 
{
    int i;
    for( i=0; i<COMMAND_QUEUE_LENGTH; i++ ) {
        if( gdrom_cmd_queue[i].status == GD_CMD_STATUS_ACTIVE ) {
            bios_gdrom_run_command( &gdrom_cmd_queue[i] );
        }
    }
}

gdrom_queue_entry_t bios_gdrom_get_command( uint32_t id )
{
    if( id >= COMMAND_QUEUE_LENGTH ||
            gdrom_cmd_queue[id].status == GD_CMD_STATUS_NONE )
        return NULL;
    return &gdrom_cmd_queue[id];
}

/**
 * Address of the system information block (in the flash rom). Also repeats
 * at FLASH_SYSINFO_SEGMENT+0xA0
 */
#define FLASH_SYSINFO_SEGMENT 0x0021a000
#define FLASH_CONFIG_SEGMENT  0x0021c000
#define FLASH_CONFIG_LENGTH   0x00004000
#define FLASH_PARTITION_MAGIC "KATANA_FLASH____"

/**
 * Locate the active config block. FIXME: This isn't completely correct, but it works
 * under at least some circumstances. 
 */
static char *bios_find_flash_config( sh4addr_t segment, uint32_t length )
{
    char *start = mem_get_region(segment);
    char *p = start + 0x80;
    char *end = p + length;
    char *result = NULL;

    if( memcmp( start, FLASH_PARTITION_MAGIC, 16 ) != 0 )
        return NULL; /* Missing magic */
    while( p < end ) {
        if( p[0] == 0x05 && p[1] == 0 ) {
            result = p;
        }
        p += 0x40;
    }
    return result;
}

/**
 * Syscall information courtesy of Marcus Comstedt
 */
static void bios_sysinfo_vector( uint32_t syscallid )
{
    char *flash_segment, *flash_config;
    char *dest;
    DEBUG( "BIOS SYSINFO: r4 = %08X, r5 = %08X, r6 = %08x, r7= %08X", sh4r.r[4], sh4r.r[5], sh4r.r[6], sh4r.r[7] );

    switch( sh4r.r[7] ) {
    case 0: /* SYSINFO_INIT */
        /* Initialize the region 8c000068 .. 8c00007f from the flash rom
         *   uint64_t system_id;
         *   char [5] system_props;
         *   char [3] zero_pad (?)
         *   char [8] settings;
         **/
        flash_segment = mem_get_region(FLASH_SYSINFO_SEGMENT);
        flash_config = bios_find_flash_config(FLASH_CONFIG_SEGMENT,FLASH_CONFIG_LENGTH);
        dest = mem_get_region( 0x8c000068 );
        memset( dest, 0, 24 );
        memcpy( dest, flash_segment + 0x56, 8 );
        memcpy( dest + 8, flash_segment, 5 );
        if( flash_config != NULL ) {
            memcpy( dest+16, flash_config+2, 8 );
        }
        break;
    case 2: /* SYSINFO_ICON */
        /* Not supported yet */
        break;
    case 3: /* SYSINFO_ID */
        sh4r.r[0] = 0x8c000068;
        break;
    }
}

static void bios_flashrom_vector( uint32_t syscallid )
{
    char *dest;
    DEBUG( "BIOS FLASHROM: r4 = %08X, r5 = %08X, r6 = %08x, r7= %08X", sh4r.r[4], sh4r.r[5], sh4r.r[6], sh4r.r[7] );

    switch( sh4r.r[7] ) {
    case 0: /* FLASHROM_INFO */
        break;
    case 1: /* FLASHROM_READ */

        break;
    case 2: /* FLASHROM_WRITE */
        break;
    case 3: /* FLASHROM_DELETE */
        break;
    }
}

static void bios_romfont_vector( uint32_t syscallid )
{
    DEBUG( "BIOS ROMFONT: r4 = %08X, r5 = %08X, r6 = %08x, r7= %08X", sh4r.r[4], sh4r.r[5], sh4r.r[6], sh4r.r[7] );
    /* Not implemented */
}

static void bios_gdrom_vector( uint32_t syscallid )
{
    gdrom_queue_entry_t cmd;

    DEBUG( "BIOS GDROM: r4 = %08X, r5 = %08X, r6 = %08x, r7= %08X", sh4r.r[4], sh4r.r[5], sh4r.r[6], sh4r.r[7] );

    switch( sh4r.r[6] ) {
    case 0: /* GD-Rom */
        switch( sh4r.r[7] ) {
        case 0: /* Send command */
            sh4r.r[0] = bios_gdrom_enqueue( sh4r.r[4], sh4r.r[5] );
            break;
        case 1:  /* Check command */
            cmd = bios_gdrom_get_command( sh4r.r[4] );
            if( cmd == NULL ) {
                sh4r.r[0] = GD_CMD_STATUS_NONE;
            } else {
                sh4r.r[0] = cmd->status;
                if( cmd->status == GD_CMD_STATUS_ERROR &&
                        sh4r.r[5] != 0 ) {
                    mem_copy_to_sh4( sh4r.r[5], (sh4ptr_t)&cmd->result, sizeof(cmd->result) );
                }
            }
            break;
        case 2: /* Mainloop */
            bios_gdrom_run_queue();
            break;
        case 3: /* Init */
            bios_gdrom_init();
            break;
        case 4: /* Drive status */
            if( sh4r.r[4] != 0 ) {
                mem_copy_to_sh4( sh4r.r[4], (sh4ptr_t)&bios_gdrom_status,
                        sizeof(bios_gdrom_status) );
            }
            sh4r.r[0] = 0;
            break;
        case 8: /* Abort command */
            cmd = bios_gdrom_get_command( sh4r.r[4] );
            if( cmd == NULL || cmd->status != GD_CMD_STATUS_ACTIVE ) {
                sh4r.r[0] = -1;
            } else {
                cmd->status = GD_CMD_STATUS_ABORT;
                sh4r.r[0] = 0;
            }
            break;
        case 9: /* Reset */
            break;
        case 10: /* Set mode */
            sh4r.r[0] = 0;
            break;
        }
        break;
        case -1: /* Misc */
        break;
        default: /* ??? */
            break;
    }
}

static void bios_menu_vector( uint32_t syscallid )
{
    DEBUG( "BIOS MENU: r4 = %08X, r5 = %08X, r6 = %08x, r7= %08X", sh4r.r[4], sh4r.r[5], sh4r.r[6], sh4r.r[7] );

    switch( sh4r.r[4] ) {
    case 0:
        WARN( "Entering main program" );
        break;
    case 1:
        WARN( "Program aborted to DC menu");
        dreamcast_stop();
        break;
    }
}

void bios_boot( uint32_t syscallid )
{
    /* Initialize hardware */
    /* Boot disc if present */
    if( !bios_boot_gdrom_disc() ) {
        dreamcast_stop();
    }
}

void bios_install( void ) 
{
    bios_gdrom_init();
    syscall_add_hook_vector( 0xB0, 0x8C0000B0, bios_sysinfo_vector );
    syscall_add_hook_vector( 0xB4, 0x8C0000B4, bios_romfont_vector );
    syscall_add_hook_vector( 0xB8, 0x8C0000B8, bios_flashrom_vector );
    syscall_add_hook_vector( 0xBC, 0x8C0000BC, bios_gdrom_vector );
    syscall_add_hook_vector( 0xE0, 0x8C0000E0, bios_menu_vector );
}

#define MIN_ISO_SECTORS 32

static gboolean bios_load_ipl( cdrom_disc_t disc, cdrom_track_t track, const char *program_name,
                               unsigned char *buffer, gboolean unscramble )
{
    gboolean rv = TRUE;

    IsoImageFilesystem *iso = iso_filesystem_new_from_track( disc, track, NULL );
    if( iso == NULL ) {
        ERROR( "Disc is not bootable (invalid ISO9660 filesystem)" );
        return FALSE;
    }
    IsoFileSource *file = NULL;
    int status = iso->get_by_path(iso, program_name, &file );
    if( status != 1 ) {
        ERROR( "Disc is not bootable (initial program '%s' not found)", program_name );
        iso_filesystem_unref(iso);
        return FALSE;
    }

    struct stat st;
    if( iso_file_source_stat(file, &st) == 1 ) {
        if( st.st_size > (0x8D000000 - BINARY_LOAD_ADDR) ) {
            ERROR( "Disc is not bootable (Initial program is too large to fit into memory)" );
            rv = FALSE;
        } else if( iso_file_source_open(file) == 1 ) {
            size_t len;
            if( unscramble ) {
                char *tmp = g_malloc(st.st_size);
                len = iso_file_source_read(file, tmp, st.st_size);
                bootprogram_unscramble(buffer, tmp, st.st_size);
                g_free(tmp);
            } else {
                len = iso_file_source_read(file, buffer, st.st_size);
            }

            if( len != st.st_size ) {
                ERROR( "Disc is not bootable (Unable to read initial program '%s')", program_name );
                rv = FALSE;
            }
            iso_file_source_close(file);
        }
    } else {
        ERROR( "Disc is not bootable (Unable to get size of initial program '%s')", program_name );
        rv = FALSE;
    }

    iso_file_source_unref(file);
    iso_filesystem_unref(iso);
    return rv;
}

gboolean bios_boot_gdrom_disc( void )
{
    cdrom_disc_t disc = gdrom_get_current_disc();

    int status = gdrom_get_drive_status();
    if( status == CDROM_DISC_NONE ) {
        ERROR( "No disc in drive" );
        return FALSE;
    }

    /* Find the bootable data track (if present) */
    cdrom_track_t track = gdrom_disc_get_boot_track(disc);
    if( track == NULL ) {
        ERROR( "Disc is not bootable" );
        return FALSE;
    }
    uint32_t lba = track->lba;
    uint32_t sectors = cdrom_disc_get_track_size(disc,track);
    if( sectors < MIN_ISO_SECTORS ) {
        ERROR( "Disc is not bootable" );
        return FALSE;
    }
    /* Load the initial bootstrap into DC ram at 8c008000 */
    size_t length = BOOTSTRAP_SIZE;
    unsigned char *bootstrap = mem_get_region(BOOTSTRAP_LOAD_ADDR);
    if( cdrom_disc_read_sectors( disc, track->lba, BOOTSTRAP_SIZE/2048,
            CDROM_READ_DATA|CDROM_READ_MODE2_FORM1, bootstrap, &length ) !=
            CDROM_ERROR_OK ) {
        ERROR( "Disc is not bootable" );
        return FALSE;
    }

    /* Check the magic just to be sure */
    dc_bootstrap_head_t metadata = (dc_bootstrap_head_t)bootstrap;
    if( memcmp( metadata->magic, BOOTSTRAP_MAGIC, BOOTSTRAP_MAGIC_SIZE ) != 0 ) {
        ERROR( "Disc is not bootable (missing dreamcast bootstrap)" );
        return FALSE;
    }

    /* Get the initial program from the bootstrap (usually 1ST_READ.BIN) */
    char program_name[18] = "/";
    memcpy(program_name+1, metadata->boot_file, 16);
    program_name[17] = '\0';
    for( int i=16; i >= 0 && program_name[i] == ' '; i-- ) {
        program_name[i] = '\0';
    }

    /* Bootstrap is good. Now find the program in the actual filesystem... */
    unsigned char *program = mem_get_region(BINARY_LOAD_ADDR);
    gboolean isGDROM = (disc->disc_type == CDROM_DISC_GDROM );
    if( !bios_load_ipl( disc, track, program_name, program, !isGDROM ) )
        return FALSE;
    asic_enable_ide_interface(isGDROM);
    dreamcast_program_loaded( "", BOOTSTRAP_ENTRY_ADDR );
    return TRUE;
}
