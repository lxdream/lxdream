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

#include "gdrom/gddriver.h"
#include "gdrom/packet.h"
#include "dream.h"

static gboolean linux_is_cdrom_device( FILE *f );
static gdrom_disc_t linux_open_device( const gchar *filename, FILE *f );
static gdrom_error_t linux_packet_read( gdrom_disc_t disc, char *cmd, 
                                        unsigned char *buf, uint32_t *buflen );
static gdrom_error_t linux_packet_cmd( gdrom_disc_t disc, char *cmd ); 
static gboolean linux_media_changed( gdrom_disc_t disc );


struct gdrom_image_class cdrom_device_class = { "Linux", NULL,
        linux_is_cdrom_device, linux_open_device };
        
static struct gdrom_scsi_transport linux_scsi_transport = {
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

GList *cdrom_get_native_devices(void)
{
    GList *list = NULL;
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
                FILE *f = fdopen(fd,"r");
                gdrom_disc_t disc = gdrom_scsi_disc_new(ent->fs_spec, f, &linux_scsi_transport);
                if( disc != NULL ) {
                	list = g_list_append( list, gdrom_device_new(disc->name, disc->display_name) );
                	disc->destroy(disc,TRUE);
                }  else {
			close(fd);
                }
            } else {
            	close(fd);
            }
        }
    }
    return list;
}

gdrom_disc_t cdrom_open_device( const gchar *method, const gchar *path )
{
    return NULL;
}

static gdrom_disc_t linux_open_device( const gchar *filename, FILE *f ) 
{
	if( !linux_is_cdrom_device(f) ) {
		return NULL;
	}

    return gdrom_scsi_disc_new(filename, f, &linux_scsi_transport);
}

static gboolean linux_media_changed( gdrom_disc_t disc )
{
    int fd = fileno(disc->file);
    int status = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    if( status == CDS_DISC_OK ) {
        status = ioctl(fd, CDROM_MEDIA_CHANGED, CDSL_CURRENT);
        return status == 0 ? FALSE : TRUE;
    } else {
    	return disc->disc_type == IDE_DISC_NONE ? FALSE : TRUE;
    }
}

/**
 * Send a packet command to the device and wait for a response. 
 * @return 0 on success, -1 on an operating system error, or a sense error
 * code on a device error.
 */
static gdrom_error_t linux_packet_read( gdrom_disc_t disc, char *cmd, 
                                        unsigned char *buffer, uint32_t *buflen )
{
    int fd = fileno(disc->file);
    struct request_sense sense;
    struct cdrom_generic_command cgc;

    memset( &cgc, 0, sizeof(cgc) );
    memset( &sense, 0, sizeof(sense) );
    memcpy( cgc.cmd, cmd, 12 );
    cgc.buffer = buffer;
    cgc.buflen = *buflen;
    cgc.sense = &sense;
    cgc.data_direction = CGC_DATA_READ;

    if( ioctl(fd, CDROM_SEND_PACKET, &cgc) < 0 ) {
        if( sense.sense_key == 0 ) {
            return -1; 
        } else {
            /* TODO: Map newer codes back to the ones used by the gd-rom. */
            return sense.sense_key | (sense.asc<<8);
        }
    } else {
        *buflen = cgc.buflen;
        return 0;
    }
}

static gdrom_error_t linux_packet_cmd( gdrom_disc_t disc, char *cmd )
{
    int fd = fileno(disc->file);
    struct request_sense sense;
    struct cdrom_generic_command cgc;

    memset( &cgc, 0, sizeof(cgc) );
    memset( &sense, 0, sizeof(sense) );
    memcpy( cgc.cmd, cmd, 12 );
    cgc.buffer = 0;
    cgc.buflen = 0;
    cgc.sense = &sense;
    cgc.data_direction = CGC_DATA_NONE;

    if( ioctl(fd, CDROM_SEND_PACKET, &cgc) < 0 ) {
        if( sense.sense_key == 0 ) {
            return -1; 
        } else {
            /* TODO: Map newer codes back to the ones used by the gd-rom. */
            return sense.sense_key | (sense.asc<<8);
        }
    } else {
        return 0;
    }
}
