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
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <glib/gutils.h>
#include "gdrom/ide.h"
#include "gdrom/gdrom.h"
#include "gdrom/gddriver.h"
#include "gdrom/packet.h"
#include "dream.h"
#include "bootstrap.h"

extern gdrom_disc_t gdrom_disc;

DEFINE_HOOK( gdrom_disc_change_hook, gdrom_disc_change_hook_t )

static void gdrom_fire_disc_changed( gdrom_disc_t disc )
{
    CALL_HOOKS( gdrom_disc_change_hook, disc, disc == NULL ? NULL : disc->name );
}

char *gdrom_mode_names[] = { "Mode 0", "Mode 1", "Mode 2", "Mode 2 Form 1", "Mode 2 Form 2", "Audio", 
        "Mode 2 semiraw", "XA Raw", "Non-XA Raw" };
uint32_t gdrom_sector_size[] = { 0, 2048, 2336, 2048, 2324, 2352, 2336, 2352, 2352 };


gdrom_device_t gdrom_device_new( const gchar *name, const gchar *dev_name )
{
    struct gdrom_device *dev = g_malloc0( sizeof(struct gdrom_device) );
    dev->name = g_strdup(name);
    dev->device_name = g_strdup(dev_name);
    return dev;
}

void gdrom_device_destroy( gdrom_device_t dev )
{
    if( dev->name != NULL ) {
        g_free( dev->name );
        dev->name = NULL;
    }
    if( dev->device_name != NULL ) {
        g_free( dev->device_name );
        dev->device_name = NULL;
    }
    g_free( dev );
}

void gdrom_mount_disc( gdrom_disc_t disc ) 
{
    if( disc != gdrom_disc ) {
        if( gdrom_disc != NULL ) {
            gdrom_disc->destroy(gdrom_disc,TRUE);
        }
        gdrom_disc = disc;
        gdrom_disc_read_title( disc );
        gdrom_fire_disc_changed( disc );
    }
}

gboolean gdrom_mount_image( const gchar *filename )
{
    gdrom_disc_t disc = gdrom_image_open(filename);
    if( disc != NULL ) {         
        gdrom_mount_disc( disc );
        return TRUE;
    }
    return FALSE;
}

void gdrom_unmount_disc( ) 
{
    if( gdrom_disc != NULL ) {
        gdrom_disc->destroy(gdrom_disc, TRUE);
        gdrom_fire_disc_changed(NULL);
        gdrom_disc = NULL;
    }
}

gdrom_disc_t gdrom_get_current_disc()
{
    return gdrom_disc;
}

const gchar *gdrom_get_current_disc_name()
{
    if( gdrom_disc == NULL ) {
        return NULL;
    } else {
        return gdrom_disc->name;
    }
}

const gchar *gdrom_get_current_disc_title()
{
    if( gdrom_disc == NULL || gdrom_disc->title[0] == '\0' ) {
        return NULL;
    } else {
        return gdrom_disc->title;
    }
}

int gdrom_disc_get_track_by_lba( gdrom_disc_t disc, uint32_t lba )
{
    int i;	
    for( i=0; i<disc->track_count; i++ ) {
        if( disc->track[i].lba <= lba && 
                lba < (disc->track[i].lba + disc->track[i].sector_count) ) {
            return i+1;
        }
    }
    return -1;
}

#define CHECK_DISC(disc) do { \
	    if( disc == NULL ) { return PKT_ERR_NODISC; } \
	    disc->check_status(disc); \
	    if( disc->disc_type == IDE_DISC_NONE ) { return PKT_ERR_NODISC; } \
    } while(0)

gdrom_error_t gdrom_disc_check_media( gdrom_disc_t disc )
{
	CHECK_DISC(disc);
	return PKT_ERR_OK;
}

gdrom_error_t gdrom_disc_get_toc( gdrom_disc_t disc, unsigned char *buf ) 
{
	struct gdrom_toc {
	    uint32_t track[99];
	    uint32_t first, last, leadout;
	};
	
    struct gdrom_toc *toc = (struct gdrom_toc *)buf;
    int i;
    
    CHECK_DISC(disc);

    for( i=0; i<disc->track_count; i++ ) {
        toc->track[i] = htonl( disc->track[i].lba ) | disc->track[i].flags;
    }
    toc->first = 0x0100 | disc->track[0].flags;
    toc->last = (disc->track_count<<8) | disc->track[i-1].flags;
    toc->leadout = htonl(disc->track[i-1].lba + disc->track[i-1].sector_count) |
    disc->track[i-1].flags;
    for( ;i<99; i++ )
        toc->track[i] = 0xFFFFFFFF;
    return PKT_ERR_OK;
}

gdrom_error_t gdrom_disc_get_session_info( gdrom_disc_t disc, int session, unsigned char *buf )
{
	CHECK_DISC(disc);
	
    struct gdrom_track *last_track = &disc->track[disc->track_count-1];
    unsigned int end_of_disc = last_track->lba + last_track->sector_count;
    int i;
    buf[0] = 0x01; /* Disc status? */
    buf[1] = 0;

    if( session == 0 ) {
        buf[2] = last_track->session+1; /* last session */
        buf[3] = (end_of_disc >> 16) & 0xFF;
        buf[4] = (end_of_disc >> 8) & 0xFF;
        buf[5] = end_of_disc & 0xFF;
        return PKT_ERR_OK;
    } else {
        session--;
        for( i=0; i<disc->track_count; i++ ) {
            if( disc->track[i].session == session ) {
                buf[2] = i+1; /* first track of session */
                buf[3] = (disc->track[i].lba >> 16) & 0xFF;
                buf[4] = (disc->track[i].lba >> 8) & 0xFF;
                buf[5] = disc->track[i].lba & 0xFF;
                return PKT_ERR_OK;
            }
        }
        return PKT_ERR_BADFIELD; /* No such session */
    }
}

gdrom_error_t gdrom_disc_get_short_status( gdrom_disc_t disc, uint32_t lba, unsigned char *buf )
{
	CHECK_DISC(disc);
	
    int track_no = gdrom_disc_get_track_by_lba( disc, lba );
    if( track_no == -1 ) {
        track_no = 1;
        lba = 150;
    }
    struct gdrom_track *track = &disc->track[track_no-1];
    uint32_t offset = lba - track->lba;
	buf[0] = 0x00;
	buf[1] = 0x15; /* audio status ? */
    buf[2] = 0x00;
    buf[3] = 0x0E;
    buf[4] = track->flags;
    buf[5] = track_no;
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

int gdrom_disc_get_drive_status( gdrom_disc_t disc ) 
{
	if( disc == NULL ) {
		return IDE_DISC_NONE;
	}
	
	disc->check_status(disc);
    if( disc->disc_type == IDE_DISC_NONE ) {
        return IDE_DISC_NONE;
    } else {
        return disc->disc_type | IDE_DISC_READY;
    }
}

/**
 * Check the disc for a useable DC bootstrap, and update the disc
 * with the title accordingly.
 * @return TRUE if we found a bootstrap, otherwise FALSE.
 */
gboolean gdrom_disc_read_title( gdrom_disc_t disc ) {
    if( disc->track_count > 0 ) {
        /* Find the first data track of the last session */
        int last_session = disc->track[disc->track_count-1].session;
        int i, boot_track = -1;
        for( i=disc->track_count-1; i>=0 && disc->track[i].session == last_session; i-- ) {
            if( disc->track[i].flags & TRACK_DATA ) {
                boot_track = i;
            }
        }
        if( boot_track != -1 ) {
            unsigned char boot_sector[MAX_SECTOR_SIZE];
            uint32_t length = sizeof(boot_sector);
            if( disc->read_sector( disc, disc->track[boot_track].lba, 0x28,
                    boot_sector, &length ) == PKT_ERR_OK ) {
                if( memcmp( boot_sector, "SEGA SEGAKATANA SEGA ENTERPRISES", 32) == 0 ) {
                    /* Got magic */
                    memcpy( disc->title, boot_sector+128, 128 );
                    for( i=127; i>=0; i-- ) {
                        if( !isspace(disc->title[i]) ) 
                            break;
                    }
                    disc->title[i+1] = '\0';
                }
                bootstrap_dump(boot_sector, FALSE);
                return TRUE;
            }
        }
    }
    return FALSE;
}
