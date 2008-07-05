/**
 * $Id: mmc.c 662 2008-03-02 11:38:08Z nkeynes $
 *
 * MMC host-side support functions.
 *
 * Copyright (c) 2008 Nathan Keynes.
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

#include "gdrom/gddriver.h"
#include "gdrom/packet.h"

#define MSFTOLBA( m,s,f ) (f + (s*CD_FRAMES_PER_SECOND) + (m*CD_FRAMES_PER_MINUTE))

void mmc_make_read_cd_cmd( char *cmd, uint32_t real_sector, int mode )
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

void mmc_parse_toc2( gdrom_image_t disc, unsigned char *buf )
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