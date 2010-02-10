/**
 * $Id$
 * 
 * "Fake" BIOS functions, for operation without the actual BIOS.
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

#include "dream.h"
#include "mem.h"
#include "syscall.h"
#include "dreamcast.h"
#include "bootstrap.h"
#include "sh4/sh4.h"
#include "drivers/cdrom/cdrom.h"
#include "drivers/cdrom/isoread.h"
#include "gdrom/gdrom.h"

#define COMMAND_QUEUE_LENGTH 16

/* TODO: Check if these are the real ATAPI command codes or not */
#define GD_CMD_PIOREAD     16
#define GD_CMD_DMAREAD     17
#define GD_CMD_GETTOC      18
#define GD_CMD_GETTOC2     19
#define GD_CMD_PLAY        20
#define GD_CMD_PLAY2       21
#define GD_CMD_PAUSE       22
#define GD_CMD_RELEASE     23
#define GD_CMD_INIT        24
#define GD_CMD_SEEK        27
#define GD_CMD_READ        28
#define GD_CMD_STOP        33
#define GD_CMD_GETSCD      34
#define GD_CMD_GETSES      35

#define GD_CMD_STATUS_NONE 0
#define GD_CMD_STATUS_ACTIVE 1
#define GD_CMD_STATUS_DONE 2
#define GD_CMD_STATUS_ABORT 3
#define GD_CMD_STATUS_ERROR 4

#define GD_ERROR_OK          0
#define GD_ERROR_NO_DISC     2
#define GD_ERROR_DISC_CHANGE 6
#define GD_ERROR_SYSTEM      1


typedef struct gdrom_command {
    int status;
    uint32_t cmd_code;
    sh4ptr_t data;
    uint32_t result[4];
} *gdrom_command_t;

static struct gdrom_command gdrom_cmd_queue[COMMAND_QUEUE_LENGTH];

static struct bios_gdrom_status {
    uint32_t status;
    uint32_t disk_type;
} bios_gdrom_status;

void bios_gdrom_run_command( gdrom_command_t cmd )
{
    DEBUG( "BIOS GD command %d", cmd->cmd_code );
    switch( cmd->cmd_code ) {
    case GD_CMD_INIT:
        /* *shrug* */
        cmd->status = GD_CMD_STATUS_DONE;
        break;
    default:
        cmd->status = GD_CMD_STATUS_ERROR;
        cmd->result[0] = GD_ERROR_SYSTEM;
        break;
    }
}

void bios_gdrom_init( void )
{
    memset( &gdrom_cmd_queue, 0, sizeof(gdrom_cmd_queue) );
}

uint32_t bios_gdrom_enqueue( uint32_t cmd, sh4ptr_t ptr )
{
    int i;
    for( i=0; i<COMMAND_QUEUE_LENGTH; i++ ) {
        if( gdrom_cmd_queue[i].status != GD_CMD_STATUS_ACTIVE ) {
            gdrom_cmd_queue[i].status = GD_CMD_STATUS_ACTIVE;
            gdrom_cmd_queue[i].cmd_code = cmd;
            gdrom_cmd_queue[i].data = ptr;
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

gdrom_command_t bios_gdrom_get_command( uint32_t id )
{
    if( id >= COMMAND_QUEUE_LENGTH ||
            gdrom_cmd_queue[id].status == GD_CMD_STATUS_NONE )
        return NULL;
    return &gdrom_cmd_queue[id];
}

/**
 * Syscall list courtesy of Marcus Comstedt
 */

void bios_syscall( uint32_t syscallid )
{
    gdrom_command_t cmd;

    switch( syscallid ) {
    case 0xB0: /* sysinfo */
        break;
    case 0xB4: /* Font */
        break;
    case 0xB8: /* Flash */
        break;
    case 0xBC: /* Misc/GD-Rom */
        switch( sh4r.r[6] ) {
        case 0: /* GD-Rom */
            switch( sh4r.r[7] ) {
            case 0: /* Send command */
                if( sh4r.r[5] == 0 )
                    sh4r.r[0] = bios_gdrom_enqueue( sh4r.r[4], NULL );
                else
                    sh4r.r[0] = bios_gdrom_enqueue( sh4r.r[4], mem_get_region(sh4r.r[5]) );
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
        break;
        case 0xE0: /* Menu */
            switch( sh4r.r[7] ) {
            case 0:
                WARN( "Entering main program" );
                break;
            case 1:
                WARN( "Program aborted to DC menu");
                dreamcast_stop();
                break;
            }
    }
}

void bios_boot( uint32_t syscallid )
{
    /* Initialize hardware */
    /* Boot disc if present */
    bios_boot_gdrom_disc();
}

void bios_install( void ) 
{
    bios_gdrom_init();
    syscall_add_hook_vector( 0xB0, 0x8C0000B0, bios_syscall );
    syscall_add_hook_vector( 0xB4, 0x8C0000B4, bios_syscall );
    syscall_add_hook_vector( 0xB8, 0x8C0000B8, bios_syscall );
    syscall_add_hook_vector( 0xBC, 0x8C0000BC, bios_syscall );
    syscall_add_hook_vector( 0xE0, 0x8C0000E0, bios_syscall );
}

#define MIN_ISO_SECTORS 32

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
    char program_name[17];
    memcpy(program_name, metadata->boot_file, 16);
    program_name[16] = '\0';
    for( int i=15; i >= 0 && program_name[i] == ' '; i-- ) {
        program_name[i] = '\0';
    }

    /* Bootstrap is good. Now find the program in the actual filesystem... */
    isofs_reader_t iso = isofs_reader_new_from_track( disc, track, NULL );
    if( iso == NULL ) {
        ERROR( "Disc is not bootable" );
        return FALSE;
    }
    isofs_reader_dirent_t ent = isofs_reader_get_file( iso, program_name );
    if( ent == NULL ) {
        ERROR( "Disc is not bootable (initial program '%s' not found)", program_name );
        isofs_reader_destroy(iso);
        return FALSE;
    }

    if( ent->size > (0x8D000000 - BINARY_LOAD_ADDR) ) {
        /* Bootstrap isn't going to fit in memory. Complain and abort */
        ERROR( "Disc is not bootable (initial program too large)" );
        isofs_reader_destroy(iso);
        return FALSE;
    }
    unsigned char *program = mem_get_region(BINARY_LOAD_ADDR);
    int program_sectors = (ent->size+2047)/2048;
    if( disc->disc_type == CDROM_DISC_GDROM ) {
        /* Load the binary directly into RAM */
        if( isofs_reader_read_file( iso, ent, 0, ent->size, program ) !=
                CDROM_ERROR_OK ) {
            ERROR( "Disc is not bootable (failed to read initial program)\n" );
            isofs_reader_destroy(iso);
            return FALSE;
        }
    } else {
        /* Load the binary into a temp buffer */
        unsigned char tmp[program_sectors*2048];
        if( isofs_reader_read_file( iso, ent, 0, ent->size, tmp ) !=
                CDROM_ERROR_OK ) {
            ERROR( "Disc is not bootable (failed to read initial program)\n" );
            isofs_reader_destroy(iso);
            return FALSE;
        }
        bootprogram_unscramble(program, tmp, ent->size);
    }
    isofs_reader_destroy(iso);
    dreamcast_program_loaded( "", BOOTSTRAP_LOAD_ADDR );
}
