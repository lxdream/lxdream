/**
 * $Id$
 *
 * Linux cd-rom device driver. Implemented using the SCSI transport.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <linux/cdrom.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fstab.h>
#include <fcntl.h>

#include "drivers/cdrom/cdimpl.h"

static gboolean linux_is_cdrom_device( FILE *f );
static gboolean linux_cdrom_disc_init( cdrom_disc_t disc, ERROR *err );
static cdrom_disc_t linux_cdrom_drive_open( cdrom_drive_t drive, ERROR *err );
static cdrom_error_t linux_cdrom_do_cmd( int fd, char *cmd,
        unsigned char *buf, unsigned int *buflen, unsigned char direction );
static cdrom_error_t linux_packet_read( cdrom_disc_t disc, char *cmd,
                                        unsigned char *buf, uint32_t *buflen );
static cdrom_error_t linux_packet_cmd( cdrom_disc_t disc, char *cmd );
static gboolean linux_media_changed( cdrom_disc_t disc );


struct cdrom_disc_factory linux_cdrom_drive_factory = { "Linux", NULL,
        linux_is_cdrom_device, linux_cdrom_disc_init, cdrom_disc_scsi_read_toc };
        
static struct cdrom_scsi_transport linux_scsi_transport = {
	linux_packet_read, linux_packet_cmd, linux_media_changed };
        
static gboolean linux_is_cdrom_device( FILE *f )
{
    int caps = ioctl(fileno(f), CDROM_GET_CAPABILITY);
    if( caps == -1 ) {
        /* Quick check that this is really a CD device */
        return FALSE;
    } else {
    	return TRUE;
    }
}		

void cdrom_drive_scan(void)
{
    unsigned char ident[256];
    uint32_t identlen;
    char cmd[12] = {0x12,0,0,0, 0xFF,0,0,0, 0,0,0,0};
    
    struct fstab *ent;
    struct stat st;
    setfsent();
    while( (ent = getfsent()) != NULL ) {
        if( (stat(ent->fs_spec, &st) != -1) && 
                S_ISBLK(st.st_mode) ) {
            /* Got a valid block device - is it a CDROM? */
            int fd = open(ent->fs_spec, O_RDONLY|O_NONBLOCK);
            if( fd == -1 )
                continue;
            int caps = ioctl(fd, CDROM_GET_CAPABILITY);
            if( caps != -1 ) {
                /* Appears to support CDROM functions */
                identlen = sizeof(ident);
                if( linux_cdrom_do_cmd( fd, cmd, ident, &identlen, CGC_DATA_READ ) ==
                        CDROM_ERROR_OK ) {
                    const char *drive_name = mmc_parse_inquiry( ident );
                    cdrom_drive_add( ent->fs_spec, drive_name, linux_cdrom_drive_open );
                }
            }
            close(fd);
        }
    }
}

gboolean linux_cdrom_disc_init( cdrom_disc_t disc, ERROR *err )
{
    if( linux_is_cdrom_device( cdrom_disc_get_base_file(disc) ) ) {
        cdrom_disc_scsi_init(disc, &linux_scsi_transport);
        return TRUE;
    } else {
        return FALSE;
    }
}

cdrom_disc_t linux_cdrom_drive_open( cdrom_drive_t drive, ERROR *err )
{
    
    int fd = open(drive->name, O_RDONLY|O_NONBLOCK);
    if( fd == -1 ) {
        SET_ERROR(err, errno, "Unable to open device '%s': %s", drive->name, strerror(errno) );
        return NULL;
    } else {
        FILE *f = fdopen(fd,"ro");
        if( !linux_is_cdrom_device(f) ) {
            SET_ERROR(err, EINVAL, "Device '%s' is not a CDROM drive", drive->name );
            return NULL;
        }
        return cdrom_disc_scsi_new_file(f, drive->name, &linux_scsi_transport, err);
    }
}

static gboolean linux_media_changed( cdrom_disc_t disc )
{
    int fd = cdrom_disc_get_base_fd(disc);
    int status = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    if( status == CDS_DISC_OK ) {
        status = ioctl(fd, CDROM_MEDIA_CHANGED, CDSL_CURRENT);
        return status == 0 ? FALSE : TRUE;
    } else {
    	return disc->disc_type == CDROM_DISC_NONE ? FALSE : TRUE;
    }
}

static cdrom_error_t linux_cdrom_do_cmd( int fd, char *cmd,
        unsigned char *buffer, unsigned int *buflen, 
        unsigned char direction )
{
    struct request_sense sense;
    struct cdrom_generic_command cgc;

    memset( &cgc, 0, sizeof(cgc) );
    memset( &sense, 0, sizeof(sense) );
    memcpy( cgc.cmd, cmd, 12 );
    cgc.buffer = buffer;
    if( buflen == NULL )
        cgc.buflen = 0;
    else
        cgc.buflen = *buflen;
    cgc.sense = &sense;
    cgc.data_direction = direction;

    if( ioctl(fd, CDROM_SEND_PACKET, &cgc) < 0 ) {
        if( sense.sense_key == 0 ) {
            return -1; 
        } else {
            return sense.sense_key | (sense.asc<<8);
        }
    } else {
        if( buflen != NULL )
            *buflen = cgc.buflen;
        return CDROM_ERROR_OK;
    }
    
}

/**
 * Send a packet command to the device and wait for a response. 
 * @return 0 on success, -1 on an operating system error, or a sense error
 * code on a device error.
 */
static cdrom_error_t linux_packet_read( cdrom_disc_t disc, char *cmd,
                                        unsigned char *buffer, unsigned int *buflen )
{
    int fd = cdrom_disc_get_base_fd(disc);
    return linux_cdrom_do_cmd( fd, cmd, buffer, buflen, CGC_DATA_READ );
}

static cdrom_error_t linux_packet_cmd( cdrom_disc_t disc, char *cmd )
{
    int fd = cdrom_disc_get_base_fd(disc);
    return linux_cdrom_do_cmd( fd, cmd, NULL, NULL, CGC_DATA_NONE );
}

