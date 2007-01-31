/**
 * $Id: linux.c,v 1.3 2007-01-31 10:58:42 nkeynes Exp $
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <linux/cdrom.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

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
					int mode, char *buf, uint32_t *length );
static gdrom_error_t linux_send_command( int fd, char *cmd, char *buffer, size_t *buflen,
					 int direction );


struct gdrom_image_class linux_device_class = { "Linux", NULL,
					     linux_image_is_valid, linux_open_device };

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
    int fd = fileno(f);

    disc = gdrom_image_new(f);
    if( disc == NULL ) {
	ERROR("Unable to allocate memory!");
	return NULL;
    }

    gdrom_error_t status = linux_read_disc_toc( (gdrom_image_t)disc );
    if( status != 0 ) {
	disc->close(disc);
	if( status == 0xFFFF ) {
	    ERROR("Unable to load disc table of contents (%s)", strerror(errno));
	} else {
	    ERROR("Unable to load disc table of contents (sense %d,%d)",
		  status &0xFF, status >> 8 );
	}
	return NULL;
    }
    disc->read_sector = linux_read_sector;
    ((gdrom_image_t)disc)->disc_type = IDE_DISC_CDROM;
    return disc;
}

/**
 * Read the full table of contents into the disc from the device.
 */
static gdrom_error_t linux_read_disc_toc( gdrom_image_t disc )
{
    int fd = fileno(disc->file);
    unsigned char buf[MAXTOCSIZE];
    int buflen = sizeof(buf);
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
    int session_type = GDROM_MODE1;
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
		session_type = GDROM_MODE2;
	    } else {
		session_type = GDROM_MODE1;
	    }
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

static gdrom_error_t linux_read_sector( gdrom_disc_t disc, uint32_t sector,
					int mode, char *buf, uint32_t *length )
{
    gdrom_image_t image = (gdrom_image_t)disc;
    int fd = fileno(image->file);
    uint32_t real_sector = sector - CD_MSF_OFFSET;
    uint32_t sector_size = MAX_SECTOR_SIZE;
    int i;
    char cmd[12] = { 0xBE, 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    cmd[1] = (mode & 0x0E) << 1;
    cmd[2] = (real_sector >> 24) & 0xFF;
    cmd[3] = (real_sector >> 16) & 0xFF;
    cmd[4] = (real_sector >> 8) & 0xFF;
    cmd[5] = real_sector & 0xFF;
    cmd[6] = 0;
    cmd[7] = 0;
    cmd[8] = 1;
    cmd[9] = 0x10;
    
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
static gdrom_error_t linux_send_command( int fd, char *cmd, char *buffer, size_t *buflen,
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
    
    if( ioctl(fd, CDROM_SEND_PACKET, &cgc) == -1 ) {
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
