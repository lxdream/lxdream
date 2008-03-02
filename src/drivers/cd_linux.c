/**
 * $Id$
 *
 * Linux cd-rom device driver. 
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
#include <linux/cdrom.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fstab.h>
#include <fcntl.h>

#include "gdrom/gdrom.h"
#include "gdrom/packet.h"
#include "dream.h"

#define MAXTOCENTRIES 600  /* This is a fairly generous overestimate really */
#define MAXTOCSIZE 4 + (MAXTOCENTRIES*11)
#define MAX_SECTORS_PER_CALL 1

#define MSFTOLBA( m,s,f ) (f + (s*CD_FRAMES) + (m*CD_FRAMES*CD_SECS))

static uint32_t inline lbatomsf( uint32_t lba ) {
    union cdrom_addr addr;
    lba = lba + CD_MSF_OFFSET;
    addr.msf.frame = lba % CD_FRAMES;
    int seconds = lba / CD_FRAMES;
    addr.msf.second = seconds % CD_SECS;
    addr.msf.minute = seconds / CD_SECS;
    return addr.lba;
}

#define LBATOMSF( lba ) lbatomsf(lba)


static gboolean linux_image_is_valid( FILE *f );
static gdrom_disc_t linux_open_device( const gchar *filename, FILE *f );
static gdrom_error_t linux_read_disc_toc( gdrom_image_t disc );
static gdrom_error_t linux_read_sector( gdrom_disc_t disc, uint32_t sector,
					int mode, unsigned char *buf, uint32_t *length );
static gdrom_error_t linux_send_command( int fd, char *cmd, unsigned char *buffer, size_t *buflen,
					 int direction );
static int linux_drive_status( gdrom_disc_t disc );

struct gdrom_image_class cdrom_device_class = { "Linux", NULL,
						linux_image_is_valid, linux_open_device };
GList *gdrom_get_native_devices(void)
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
		list = g_list_append( list, g_strdup(ent->fs_spec) );
	    }
	    close(fd);
	}
    }
    return list;
}

static gboolean linux_image_is_valid( FILE *f )
{
    struct stat st;
    struct cdrom_tochdr tochdr;

    if( fstat(fileno(f), &st) == -1 ) {
	return FALSE; /* can't stat device? */
    }
    if( !S_ISBLK(st.st_mode) ) {
	return FALSE; /* Not a block device */
    }

    if( ioctl(fileno(f), CDROMREADTOCHDR, &tochdr) == -1 ) {
	/* Quick check that this is really a CD */
	return FALSE;
    }

    return TRUE;
}

static gdrom_disc_t linux_open_device( const gchar *filename, FILE *f ) 
{
    gdrom_disc_t disc;

    disc = gdrom_image_new(filename, f);
    if( disc == NULL ) {
	ERROR("Unable to allocate memory!");
	return NULL;
    }

    gdrom_error_t status = linux_read_disc_toc( (gdrom_image_t)disc );
    if( status != 0 ) {
	gdrom_image_destroy_no_close(disc);
	if( status == 0xFFFF ) {
	    ERROR("Unable to load disc table of contents (%s)", strerror(errno));
	} else {
	    ERROR("Unable to load disc table of contents (sense %d,%d)",
		  status &0xFF, status >> 8 );
	}
	return NULL;
    }
    disc->read_sector = linux_read_sector;
    disc->drive_status = linux_drive_status;
    return disc;
}

static int linux_drive_status( gdrom_disc_t disc )
{
    int fd = fileno(((gdrom_image_t)disc)->file);
    int status = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    if( status == CDS_DISC_OK ) {
	status = ioctl(fd, CDROM_MEDIA_CHANGED, CDSL_CURRENT);
	if( status != 0 ) {
	    linux_read_disc_toc( (gdrom_image_t)disc);
	}
	return ((gdrom_image_t)disc)->disc_type | IDE_DISC_READY;
    } else {
	return IDE_DISC_NONE;
    }
}
/**
 * Read the full table of contents into the disc from the device.
 */
static gdrom_error_t linux_read_disc_toc( gdrom_image_t disc )
{
    int fd = fileno(disc->file);
    unsigned char buf[MAXTOCSIZE];
    size_t buflen = sizeof(buf);
    char cmd[12] = { 0x43, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    
    cmd[7] = (sizeof(buf))>>8;
    cmd[8] = (sizeof(buf))&0xFF;
    memset( buf, 0, sizeof(buf) );
    gdrom_error_t status = linux_send_command( fd, cmd, buf, &buflen, CGC_DATA_READ );
    if( status != 0 ) {
	return status;
    }

    int max_track = 0;
    int last_track = -1;
    int leadout = -1;
    int len = (buf[0] << 8) | buf[1];
    int session_type = -1;
    int i;
    for( i = 4; i<len; i+=11 ) {
	int session = buf[i];
	int adr = buf[i+1] >> 4;
	int point = buf[i+3];
	if( adr == 0x01 && point > 0 && point < 100 ) {
	    /* Track info */
	    int trackno = point-1;
	    if( point > max_track ) {
		max_track = point;
	    }
	    disc->track[trackno].flags = (buf[i+1] & 0x0F) << 4;
	    disc->track[trackno].session = session - 1;
	    disc->track[trackno].lba = MSFTOLBA(buf[i+8],buf[i+9],buf[i+10]);
	    if( disc->track[trackno].flags & TRACK_DATA ) {
		disc->track[trackno].mode = GDROM_MODE1;
	    } else {
		disc->track[trackno].mode = GDROM_CDDA;
	    }
	    if( last_track != -1 ) {
		disc->track[last_track].sector_count = disc->track[trackno].lba -
		    disc->track[last_track].lba;
	    }
	    last_track = trackno;
	} else switch( (adr << 8) | point ) {
	case 0x1A0: /* session info */
	    if( buf[i+9] == 0x20 ) {
		session_type = IDE_DISC_CDROMXA;
	    } else {
		session_type = IDE_DISC_CDROM;
	    }
	    disc->disc_type = session_type;
	    break;
	case 0x1A2: /* leadout */
	    leadout = MSFTOLBA(buf[i+8], buf[i+9], buf[i+10]);
	    break;
	}
    }
    disc->track_count = max_track;

    if( leadout != -1 && last_track != -1 ) {
	disc->track[last_track].sector_count = leadout - disc->track[last_track].lba;
    }
    return 0;
}

 gdrom_error_t linux_play_audio( gdrom_disc_t disc, uint32_t lba, uint32_t endlba )
{
    int fd = fileno( ((gdrom_image_t)disc)->file );
    uint32_t real_sector = lba - CD_MSF_OFFSET;
    uint32_t length = endlba - lba;
    uint32_t buflen = 0;
    char cmd[12] = { 0xA5, 0,0,0, 0,0,0,0, 0,0,0,0 };
    cmd[2] = (real_sector >> 24) & 0xFF;
    cmd[3] = (real_sector >> 16) & 0xFF;
    cmd[4] = (real_sector >> 8) & 0xFF;
    cmd[5] = real_sector & 0xFF;
    cmd[6] = (length >> 24) & 0xFF;
    cmd[7] = (length >> 16) & 0xFF;
    cmd[8] = (length >> 8) & 0xFF;
    cmd[9] = length & 0xFF;
    
    return linux_send_command( fd, cmd, NULL, &buflen, CGC_DATA_NONE );
}

gdrom_error_t linux_stop_audio( gdrom_disc_t disc )
{
    int fd = fileno( ((gdrom_image_t)disc)->file );
    uint32_t buflen = 0;
    char cmd[12] = {0x4E,0,0,0, 0,0,0,0, 0,0,0,0};
    return linux_send_command( fd, cmd, NULL, &buflen, CGC_DATA_NONE );
}

static gdrom_error_t linux_read_sector( gdrom_disc_t disc, uint32_t sector,
					int mode, unsigned char *buf, uint32_t *length )
{
    gdrom_image_t image = (gdrom_image_t)disc;
    int fd = fileno(image->file);
    uint32_t real_sector = sector - CD_MSF_OFFSET;
    uint32_t sector_size = MAX_SECTOR_SIZE;
    char cmd[12] = { 0xBE, 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    cmd[1] = (mode & 0x0E) << 1;
    cmd[2] = (real_sector >> 24) & 0xFF;
    cmd[3] = (real_sector >> 16) & 0xFF;
    cmd[4] = (real_sector >> 8) & 0xFF;
    cmd[5] = real_sector & 0xFF;
    cmd[6] = 0;
    cmd[7] = 0;
    cmd[8] = 1;

    if( READ_CD_RAW(mode) ) {
	cmd[9] = 0xF0;
    } else {
	if( READ_CD_HEADER(mode) ) {
	    cmd[9] = 0xA0;
	}
	if( READ_CD_SUBHEAD(mode) ) {
	    cmd[9] |= 0x40;
	}
	if( READ_CD_DATA(mode) ) {
	    cmd[9] |= 0x10;
	}
    }
    
    gdrom_error_t status = linux_send_command( fd, cmd, buf, &sector_size, CGC_DATA_READ );
    if( status != 0 ) {
	return status;
    }
    *length = 2048;
    return 0;
}

/**
 * Send a packet command to the device and wait for a response. 
 * @return 0 on success, -1 on an operating system error, or a sense error
 * code on a device error.
 */
static gdrom_error_t linux_send_command( int fd, char *cmd, unsigned char *buffer, size_t *buflen,
					 int direction )
{
    struct request_sense sense;
    struct cdrom_generic_command cgc;

    memset( &cgc, 0, sizeof(cgc) );
    memset( &sense, 0, sizeof(sense) );
    memcpy( cgc.cmd, cmd, 12 );
    cgc.buffer = buffer;
    cgc.buflen = *buflen;
    cgc.sense = &sense;
    cgc.data_direction = direction;
    
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
