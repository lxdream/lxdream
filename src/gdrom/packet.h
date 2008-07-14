/**
 * $Id$
 *
 * This file defines the command codes and any other flags used by the 
 * GD-Rom ATAPI packet commands.
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

#ifndef lxdream_packet_H
#define lxdream_packet_H 1

/**
 * Valid command codes (hex):
 * 00  Test 
 * 10
 * 11  Inquiry
 * 12
 * 13  Request Sense
 * 14  Read TOC
 * 15  Read session info
 * 16
 * 20
 * 21
 * 22
 * 30  Read CD
 * 31
 * 40  Read Status ?
 * 50
 * 51
 * 52
 * 53
 * 54
 * 55
 * 70  
 * 71
 * 72
 * 73
 * FE
 */

#define PKT_CMD_TEST_READY 0x00
#define PKT_CMD_MODE_SENSE 0x11
#define PKT_CMD_MODE_SELECT 0x12
#define PKT_CMD_SENSE    0x13
#define PKT_CMD_READ_TOC 0x14
#define PKT_CMD_SESSION_INFO 0x15
#define PKT_CMD_READ_SECTOR 0x30
#define PKT_CMD_PLAY_AUDIO  0x20 /* ? */
#define PKT_CMD_STATUS  0x40
#define PKT_CMD_SPIN_UP 0x70 /* ??? */
#define PKT_CMD_71      0x71 /* ??? seems to return garbage */

#define PKT_ERR_OK        0x0000
#define PKT_ERR_NODISC    0x3A02
#define PKT_ERR_BADCMD    0x2005
#define PKT_ERR_BADFIELD  0x2405
#define PKT_ERR_BADREAD   0x3002
#define PKT_ERR_BADREADMODE 0x6405  /* Illegal mode for this track */
#define PKT_ERR_RESET     0x2906

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

#endif /* !lxdream_packet_H */
