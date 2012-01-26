/**
 * $Id$
 *
 * SCSI/MMC device interface (depends on lower-level SCSI transport)
 *
 * Copyright (c) 2009 Nathan Keynes.
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

#include <assert.h>
#include <string.h>
#include <glib/gstrfuncs.h>
#include "lxdream.h"
#include "gettext.h"
#include "drivers/cdrom/cdimpl.h"

#define MAXTOCENTRIES 600  /* This is a fairly generous overestimate really */
#define MAXTOCSIZE 4 + (MAXTOCENTRIES*11)
#define MAX_SECTORS_PER_CALL 1

/**
 * Parse the TOC (format 2) into the cdrom_disc structure
 */
void mmc_parse_toc2( cdrom_disc_t disc, unsigned char *buf )
{
    int max_track = 0;
    int max_session = 0;
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
            if( session > max_session ) {
                max_session = session;
            }
            disc->track[trackno].trackno = point;
            disc->track[trackno].flags = (buf[i+1] & 0x0F) << 4;
            disc->track[trackno].sessionno = session;
            disc->track[trackno].lba = MSFTOLBA(buf[i+8],buf[i+9],buf[i+10]);
            last_track = trackno;
        } else switch( (adr << 8) | point ) {
        case 0x1A0: /* session info */
            if( buf[i+9] == 0x20 ) {
                session_type = CDROM_DISC_XA;
            } else {
                session_type = CDROM_DISC_NONXA;
            }
            disc->disc_type = session_type;
            break;
        case 0x1A2: /* leadout */
            disc->leadout = MSFTOLBA(buf[i+8], buf[i+9], buf[i+10]);
            break;
        }
    }
    disc->track_count = max_track;
    disc->session_count = max_session;
}


const char *mmc_parse_inquiry( unsigned char *buf )
{
    char vendorid[9];
    char productid[17];
    char productrev[5];
    memcpy( vendorid, buf+8, 8 ); vendorid[8] = 0;
    memcpy( productid, buf+16, 16 ); productid[16] = 0;
    memcpy( productrev, buf+32, 4 ); productrev[4] = 0;
    return g_strdup_printf( "%.8s %.16s %.4s", g_strstrip(vendorid),
              g_strstrip(productid), g_strstrip(productrev) );
}

/**
 * Construct a drive indentification string based on the response to the
 * INQUIRY command. On success, returns the disc identification as a newly
 * allocated string, otherwise NULL.
 */
const char *cdrom_disc_scsi_identify_drive( cdrom_disc_t disc )
{
    unsigned char ident[256];
    uint32_t identlen = 256;
    char cmd[12] = {0x12,0,0,0, 0xFF,0,0,0, 0,0,0,0};
    cdrom_error_t status = SCSI_TRANSPORT(disc)->packet_read( disc, cmd, ident, &identlen );
    if( status == CDROM_ERROR_OK ) {
        return mmc_parse_inquiry(ident);
    }
    return NULL;
}


static cdrom_error_t cdrom_disc_scsi_read_sectors( sector_source_t source, cdrom_lba_t lba,
        cdrom_count_t count, cdrom_read_mode_t mode, unsigned char *buf, size_t *length )
{
    assert( IS_SECTOR_SOURCE_TYPE(source,DISC_SECTOR_SOURCE) );
    cdrom_disc_t disc = (cdrom_disc_t)source;
    uint32_t sector_size = CDROM_MAX_SECTOR_SIZE;
    char cmd[12];

    cmd[0] = 0xBE;
    cmd[1] = CDROM_READ_TYPE(mode);
    cmd[2] = (lba >> 24) & 0xFF;
    cmd[3] = (lba >> 16) & 0xFF;
    cmd[4] = (lba >> 8) & 0xFF;
    cmd[5] = lba & 0xFF;
    cmd[6] = (count>>16) & 0xFF;
    cmd[7] = (count>>8) & 0xFF;
    cmd[8] = count & 0xFF;
    cmd[9] = CDROM_READ_FIELDS(mode)>>8;
    cmd[10]= 0;
    cmd[11]= 0;

    cdrom_error_t status = SCSI_TRANSPORT(disc)->packet_read( disc, cmd, buf, &sector_size );
    if( status != 0 ) {
        return status;
    }
    /* FIXME */
    *length = 2048;
    return 0;
}

/**
 * Read the full table of contents into the disc from the device.
 */
gboolean cdrom_disc_scsi_read_toc( cdrom_disc_t disc, ERROR *err )
{
    unsigned char buf[MAXTOCSIZE];
    uint32_t buflen = sizeof(buf);
    char cmd[12] = { 0x43, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    cmd[7] = (sizeof(buf))>>8;
    cmd[8] = (sizeof(buf))&0xFF;
    memset( buf, 0, sizeof(buf) );
    cdrom_error_t status = SCSI_TRANSPORT(disc)->packet_read(disc, cmd, buf, &buflen );
    if( status == CDROM_ERROR_OK ) {
	    mmc_parse_toc2( disc, buf );
	    return TRUE;
    } else {
    	if( (status & 0xFF) != 0x02 ) {
    	    /* Sense key 2 == Not Ready (ie temporary failure). Just ignore and
    	     * consider the drive empty for now, but warn about any other errors
    	     * we get. */
    	    WARN( _("Unable to read disc table of contents (error %04x)"), status );
    	}
    	cdrom_disc_clear_toc(disc);
    	return FALSE;
    }
}

static gboolean cdrom_disc_scsi_check_media( cdrom_disc_t disc )
{
	if( SCSI_TRANSPORT(disc)->media_changed(disc) ) {
		cdrom_disc_scsi_read_toc(disc, NULL);
		return TRUE;
	} else {
		return FALSE;
	}
}

static cdrom_error_t cdrom_disc_scsi_play_audio( cdrom_disc_t disc, cdrom_lba_t lba, cdrom_count_t length )
{
    char cmd[12] = { 0xA5, 0,0,0, 0,0,0,0, 0,0,0,0 };
    cmd[2] = (lba >> 24) & 0xFF;
    cmd[3] = (lba >> 16) & 0xFF;
    cmd[4] = (lba >> 8) & 0xFF;
    cmd[5] = lba & 0xFF;
    cmd[6] = (length >> 24) & 0xFF;
    cmd[7] = (length >> 16) & 0xFF;
    cmd[8] = (length >> 8) & 0xFF;
    cmd[9] = length & 0xFF;

    return SCSI_TRANSPORT(disc)->packet_cmd( disc, cmd );
}


static cdrom_error_t cdrom_disc_scsi_stop_audio( cdrom_disc_t disc )
{
    uint32_t buflen = 0;
    char cmd[12] = {0x4E,0,0,0, 0,0,0,0, 0,0,0,0};
    
    return SCSI_TRANSPORT(disc)->packet_cmd( disc, cmd );
}

void cdrom_disc_scsi_init( cdrom_disc_t disc, cdrom_scsi_transport_t scsi )
{
    disc->impl_data = scsi;
    disc->source.read_sectors = cdrom_disc_scsi_read_sectors;
    disc->read_toc = cdrom_disc_scsi_read_toc;
    disc->check_media = cdrom_disc_scsi_check_media;
    disc->play_audio = cdrom_disc_scsi_play_audio;
    disc->stop_audio = cdrom_disc_scsi_stop_audio;
}

cdrom_disc_t cdrom_disc_scsi_new( const char *filename, cdrom_scsi_transport_t scsi, ERROR *err )
{
    cdrom_disc_t disc = cdrom_disc_new(filename, err);
    if( disc != NULL ) {
        /* Initialize */
        cdrom_disc_scsi_init( disc, scsi );
        cdrom_disc_read_toc(disc, err);
    }
    return disc;
} 

cdrom_disc_t cdrom_disc_scsi_new_file( FILE *f, const char *filename, cdrom_scsi_transport_t scsi, ERROR *err )
{
    cdrom_disc_t disc = cdrom_disc_new(filename, err);
    if( disc != NULL ) {
        /* Initialize */
        disc->base_source = file_sector_source_new( f, SECTOR_UNKNOWN, 0, 0, TRUE );
        if( disc->base_source != NULL ) {
            sector_source_ref( disc->base_source );
            cdrom_disc_scsi_init( disc, scsi );
            cdrom_disc_read_toc(disc, err);
        } else {
            cdrom_disc_unref(disc);
            disc = NULL;
        }
    }
    return disc;
}
