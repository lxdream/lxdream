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

#include <string.h>
#include "lxdream.h"
#include "gettext.h"
#include "gdrom/gddriver.h"
#include "gdrom/packet.h"

#define MAXTOCENTRIES 600  /* This is a fairly generous overestimate really */
#define MAXTOCSIZE 4 + (MAXTOCENTRIES*11)
#define MAX_SECTORS_PER_CALL 1

#define MSFTOLBA( m,s,f ) (f + (s*CD_FRAMES_PER_SECOND) + (m*CD_FRAMES_PER_MINUTE))

static void mmc_make_read_cd_cmd( char *cmd, uint32_t real_sector, int mode )
{
    cmd[0] = 0xBE;
    cmd[1] = (mode & 0x0E) << 1;
    cmd[2] = (real_sector >> 24) & 0xFF;
    cmd[3] = (real_sector >> 16) & 0xFF;
    cmd[4] = (real_sector >> 8) & 0xFF;
    cmd[5] = real_sector & 0xFF;
    cmd[6] = 0;
    cmd[7] = 0;
    cmd[8] = 1;
    cmd[9] = 0;
    cmd[10]= 0;
    cmd[11]= 0;

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
}

/**
 * Parse the TOC (format 2) into the gdrom_disc structure
 */
void mmc_parse_toc2( gdrom_disc_t disc, unsigned char *buf )
{
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
}


/**
 * Construct a drive indentification string based on the response to the
 * INQUIRY command. On success, the disc display_name is updated with the
 * drive name, otherwise the display_name is unchanged.
 * @return PKT_ERR_OK on success, otherwise the host failure code.
 */
static gdrom_error_t gdrom_scsi_identify_drive( gdrom_disc_t disc )
{
    unsigned char ident[256];
    uint32_t identlen = 256;
    char cmd[12] = {0x12,0,0,0, 0xFF,0,0,0, 0,0,0,0};
    gdrom_error_t status = SCSI_TRANSPORT(disc)->packet_read( disc, cmd, ident, &identlen );
    if( status == PKT_ERR_OK ) {
        char vendorid[9];
        char productid[17];
        char productrev[5];
        memcpy( vendorid, ident+8, 8 ); vendorid[8] = 0;
        memcpy( productid, ident+16, 16 ); productid[16] = 0;
        memcpy( productrev, ident+32, 4 ); productrev[4] = 0;
       	g_free( (char *)disc->display_name );
        disc->display_name = g_strdup_printf( "%.8s %.16s %.4s", g_strstrip(vendorid), 
                  g_strstrip(productid), g_strstrip(productrev) );
    }
    return status;
}


static gdrom_error_t gdrom_scsi_read_sector( gdrom_disc_t disc, uint32_t sector,
                                        int mode, unsigned char *buf, uint32_t *length )
{
    uint32_t real_sector = sector - GDROM_PREGAP;
    uint32_t sector_size = MAX_SECTOR_SIZE;
    char cmd[12];

    mmc_make_read_cd_cmd( cmd, real_sector, mode );

    gdrom_error_t status = SCSI_TRANSPORT(disc)->packet_read( disc, cmd, buf, &sector_size );
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
static gdrom_error_t gdrom_scsi_read_toc( gdrom_disc_t disc )
{
    unsigned char buf[MAXTOCSIZE];
    uint32_t buflen = sizeof(buf);
    char cmd[12] = { 0x43, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    cmd[7] = (sizeof(buf))>>8;
    cmd[8] = (sizeof(buf))&0xFF;
    memset( buf, 0, sizeof(buf) );
    gdrom_error_t status = SCSI_TRANSPORT(disc)->packet_read(disc, cmd, buf, &buflen );
    if( status == PKT_ERR_OK ) {
	    mmc_parse_toc2( disc, buf );	
    } else {
    	if( status & 0xFF != 0x02 ) {
    	    /* Sense key 2 == Not Ready (ie temporary failure). Just ignore and
    	     * consider the drive empty for now, but warn about any other errors
    	     * we get. */
    	    WARN( _("Unable to read disc table of contents (error %04x)"), status );
    	}
    	disc->disc_type = IDE_DISC_NONE;
    }
    return status;
}

static gboolean gdrom_scsi_check_status( gdrom_disc_t disc )
{
	if( SCSI_TRANSPORT(disc)->media_changed(disc) ) {
		gdrom_scsi_read_toc(disc);
		return TRUE;
	} else {
		return FALSE;
	}
}

static gdrom_error_t gdrom_scsi_play_audio( gdrom_disc_t disc, uint32_t lba, uint32_t endlba )
{
    uint32_t real_sector = lba - GDROM_PREGAP;
    uint32_t length = endlba - lba;
 
    char cmd[12] = { 0xA5, 0,0,0, 0,0,0,0, 0,0,0,0 };
    cmd[2] = (real_sector >> 24) & 0xFF;
    cmd[3] = (real_sector >> 16) & 0xFF;
    cmd[4] = (real_sector >> 8) & 0xFF;
    cmd[5] = real_sector & 0xFF;
    cmd[6] = (length >> 24) & 0xFF;
    cmd[7] = (length >> 16) & 0xFF;
    cmd[8] = (length >> 8) & 0xFF;
    cmd[9] = length & 0xFF;

    return SCSI_TRANSPORT(disc)->packet_cmd( disc, cmd );
}


gdrom_error_t gdrom_scsi_stop_audio( gdrom_disc_t disc )
{
    int fd = fileno(disc->file);
    uint32_t buflen = 0;
    char cmd[12] = {0x4E,0,0,0, 0,0,0,0, 0,0,0,0};
    
    return SCSI_TRANSPORT(disc)->packet_cmd( disc, cmd );
}


gdrom_disc_t gdrom_scsi_disc_new( const gchar *filename, FILE *f, gdrom_scsi_transport_t scsi )
{
	gdrom_disc_t disc = gdrom_disc_new(filename,f);
    if( disc != NULL ) {
	    /* Initialize */
    	disc->impl_data = scsi;
    	disc->check_status = gdrom_scsi_check_status;
    	disc->read_sector = gdrom_scsi_read_sector;
    	disc->play_audio = gdrom_scsi_play_audio;
    	disc->run_time_slice = NULL;
    	gdrom_scsi_identify_drive(disc);
    	gdrom_scsi_read_toc(disc);
    }
    return disc;
} 