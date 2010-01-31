/**
 * $Id$
 *
 * GD-Rom  access functions.
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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <glib/gutils.h>
#include <netinet/in.h>
#include "gdrom/ide.h"
#include "gdrom/gdrom.h"
#include "gdrom/packet.h"
#include "bootstrap.h"
#include "drivers/cdrom/cdrom.h"

#define GDROM_LBA_OFFSET 150

DEFINE_HOOK( gdrom_disc_change_hook, gdrom_disc_change_hook_t )

static void gdrom_fire_disc_changed( cdrom_disc_t disc )
{
    CALL_HOOKS( gdrom_disc_change_hook, disc, disc == NULL ? NULL : disc->name );
}

gboolean gdrom_disc_read_title( cdrom_disc_t disc, char *title, size_t titlelen );

struct gdrom_drive {
    cdrom_disc_t disc;
    int boot_track;
    char title[129];
} gdrom_drive;

void gdrom_mount_disc( cdrom_disc_t disc )
{
    if( disc != gdrom_drive.disc ) {
        cdrom_disc_unref(gdrom_drive.disc);
        gdrom_drive.disc = disc;
        cdrom_disc_ref(disc);
        gdrom_disc_read_title( disc, gdrom_drive.title, sizeof(gdrom_drive.title) );
        gdrom_fire_disc_changed( disc );
    }
}

gboolean gdrom_mount_image( const gchar *filename )
{
    cdrom_disc_t disc = cdrom_disc_open(filename, NULL);
    if( disc != NULL ) {         
        gdrom_mount_disc( disc );
        return TRUE;
    }
    return FALSE;
}

void gdrom_unmount_disc( ) 
{
    if( gdrom_drive.disc != NULL ) {
        cdrom_disc_unref(gdrom_drive.disc);
        gdrom_fire_disc_changed(NULL);
        gdrom_drive.disc = NULL;
    }
}

cdrom_disc_t gdrom_get_current_disc()
{
    return gdrom_drive.disc;
}

const gchar *gdrom_get_current_disc_name()
{
    if( gdrom_drive.disc == NULL ) {
        return NULL;
    } else {
        return gdrom_drive.disc->name;
    }
}

const gchar *gdrom_get_current_disc_title()
{
    if( gdrom_drive.disc == NULL || gdrom_drive.title[0] == '\0' ) {
        return NULL;
    } else {
        return gdrom_drive.title;
    }
}

#define CHECK_DISC() do { \
	    if( gdrom_drive.disc == NULL || gdrom_drive.disc->disc_type == CDROM_DISC_NONE ) { return CDROM_ERROR_NODISC; } \
    } while(0)

cdrom_error_t gdrom_check_media( )
{
	CHECK_DISC();
	return CDROM_ERROR_OK;
}

cdrom_error_t gdrom_read_toc( unsigned char *buf )
{
	struct gdrom_toc {
	    uint32_t track[99];
	    uint32_t first, last, leadout;
	};
	
    struct gdrom_toc *toc = (struct gdrom_toc *)buf;
    int i;
    
    CHECK_DISC();
    cdrom_disc_t disc = gdrom_drive.disc;

    for( i=0; i<disc->track_count; i++ ) {
        toc->track[i] = htonl( disc->track[i].lba+GDROM_LBA_OFFSET ) | disc->track[i].flags;
    }
    toc->first = 0x0100 | disc->track[0].flags;
    toc->last = (disc->track_count<<8) | disc->track[i-1].flags;
    toc->leadout = htonl(disc->leadout+GDROM_LBA_OFFSET) |
    disc->track[i-1].flags;
    for( ;i<99; i++ )
        toc->track[i] = 0xFFFFFFFF;
    return PKT_ERR_OK;
}

cdrom_error_t gdrom_read_session( int session, unsigned char *buf )
{
    cdrom_lba_t lba;
	CHECK_DISC();
	
    buf[0] = 0x01; /* Disc status? */
    buf[1] = 0;


    if( session == 0 ) {
        buf[2] = gdrom_drive.disc->session_count;
        lba = gdrom_drive.disc->leadout + GDROM_LBA_OFFSET;
    } else {
        cdrom_track_t track = cdrom_disc_get_session( gdrom_drive.disc, session );
        if( track == NULL )
            return CDROM_ERROR_BADFIELD;

        buf[2] = track->trackno;
        lba = track->lba + GDROM_LBA_OFFSET;
    }
    buf[3] = (lba >> 16) & 0xFF;
    buf[4] = (lba >> 8) & 0xFF;
    buf[5] = lba & 0xFF;
    return CDROM_ERROR_OK;
}

cdrom_error_t gdrom_read_short_status( cdrom_lba_t lba, unsigned char *buf )
{
    cdrom_lba_t real_lba = lba - GDROM_LBA_OFFSET;
	CHECK_DISC();
	
    cdrom_track_t track = cdrom_disc_get_track_by_lba( gdrom_drive.disc, real_lba );
    if( track == NULL ) {
        track = cdrom_disc_get_track( gdrom_drive.disc, 1 );
        if( track == NULL )
            return CDROM_ERROR_NODISC;
        lba = 150;
    }
    uint32_t offset = real_lba - track->lba;
	buf[0] = 0x00;
	buf[1] = 0x15; /* audio status ? */
    buf[2] = 0x00;
    buf[3] = 0x0E;
    buf[4] = track->flags;
    buf[5] = track->trackno;
    buf[6] = 0x01; /* ?? */
    buf[7] = (offset >> 16) & 0xFF;
    buf[8] = (offset >> 8) & 0xFF;
    buf[9] = offset & 0xFF;
    buf[10] = 0;
    buf[11] = (lba >> 16) & 0xFF;
    buf[12] = (lba >> 8) & 0xFF;
    buf[13] = lba & 0xFF;
    return PKT_ERR_OK;
}

int gdrom_get_drive_status( )
{
	if( gdrom_drive.disc == NULL ) {
		return CDROM_DISC_NONE;
	}
	
    if( cdrom_disc_check_media(gdrom_drive.disc) == CDROM_DISC_NONE ) {
        return CDROM_DISC_NONE;
    } else {
        return gdrom_drive.disc->disc_type | IDE_DISC_READY;
    }
}

cdrom_error_t gdrom_play_audio( cdrom_lba_t lba, cdrom_count_t count )
{
    CHECK_DISC();
    if( gdrom_drive.disc->play_audio ) {
        return gdrom_drive.disc->play_audio( gdrom_drive.disc, lba - GDROM_LBA_OFFSET, count );
    }
    return CDROM_ERROR_BADFIELD;
}

/* Parse CD read */
#define READ_CD_MODE(x)    ((x)&0x0E)
#define READ_CD_MODE_ANY   0x00
#define READ_CD_MODE_CDDA  0x02
#define READ_CD_MODE_1     0x04
#define READ_CD_MODE_2     0x06
#define READ_CD_MODE_2_FORM_1 0x08
#define READ_CD_MODE_2_FORM_2 0x0A

#define READ_CD_CHANNELS(x) ((x)&0xF0)
#define READ_CD_HEADER(x)  ((x)&0x80)
#define READ_CD_SUBHEAD(x) ((x)&0x40)
#define READ_CD_DATA(x)    ((x)&0x20)
#define READ_CD_RAW(x)     ((x)&0x10)


cdrom_error_t gdrom_read_cd( cdrom_lba_t lba, cdrom_count_t count,
                             unsigned mode, unsigned char *buf, size_t *length )
{
    cdrom_lba_t real_lba = lba - 150;
    cdrom_read_mode_t real_mode = 0;

    CHECK_DISC();

    /* Translate GDROM read mode into standard MMC read mode */
    if( READ_CD_RAW(mode) ) {
        real_mode = CDROM_READ_RAW;
    } else {
        if( READ_CD_HEADER(mode) ) {
            real_mode = CDROM_READ_HEADER|CDROM_READ_SYNC;
        }
        if( READ_CD_SUBHEAD(mode) ) {
            real_mode |= CDROM_READ_SUBHEADER;
        }
        if( READ_CD_DATA(mode) ) {
            real_mode |= CDROM_READ_DATA;
        }
    }

    if( READ_CD_MODE(mode) == 0x0C )
        real_mode |= CDROM_READ_MODE2;
    else
        real_mode |= (READ_CD_MODE(mode)<<1);

    return cdrom_disc_read_sectors( gdrom_drive.disc, real_lba, count, real_mode, buf, length );
}

void gdrom_run_slice( uint32_t nanosecs )
{

}


cdrom_track_t gdrom_disc_get_boot_track( cdrom_disc_t disc ) {
    int i, boot_track = -1;
    if( disc != NULL && disc->track_count > 0 ) {
        int last_session = disc->track[disc->track_count-1].sessionno;
        if( last_session == 1 )
            return NULL;
        for( i=disc->track_count-1; i>=0 && disc->track[i].sessionno == last_session; i-- ) {
            if( disc->track[i].flags & TRACK_FLAG_DATA ) {
                boot_track = i;
            }
        }
    }
    return &disc->track[boot_track];
}

/**
 * Check the disc for a useable DC bootstrap, and update the disc
 * with the title accordingly. Otherwise set the title to the empty string.
 * @return TRUE if we found a bootstrap, otherwise FALSE.
 */
gboolean gdrom_disc_read_title( cdrom_disc_t disc, char *title, size_t titlelen ) {
    cdrom_track_t boot_track = gdrom_disc_get_boot_track(disc);
    int i;
    if( boot_track != NULL ) {
        unsigned char boot_sector[CDROM_MAX_SECTOR_SIZE];
        size_t length = sizeof(boot_sector);
        if( cdrom_disc_read_sectors( disc, boot_track->lba, 1, CDROM_READ_DATA|CDROM_READ_MODE2_FORM1,
                boot_sector, &length ) == CDROM_ERROR_OK ) {
            if( memcmp( boot_sector, "SEGA SEGAKATANA SEGA ENTERPRISES", 32) == 0 ) {
                /* Got magic */
                dc_bootstrap_head_t bootstrap = (dc_bootstrap_head_t)boot_sector;
                for( i=128; i>0; i-- ) {
                    if( !isspace(bootstrap->product_name[i-1]) )
                        break;
                }
                if( i >= titlelen )
                    i = (titlelen-1);
                memcpy( title, bootstrap->product_name, i );
                title[i] = '\0';
            }
            bootstrap_dump(boot_sector, FALSE);
            return TRUE;
        }
    }
    title[0] = '\0';
    return FALSE;
}

