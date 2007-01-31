/**
 * $Id: packet.h,v 1.7 2007-01-31 10:58:42 nkeynes Exp $
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
#define PKT_CMD_IDENTIFY 0x11
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

#define IDE_READ_MODE1 0x20
#define IDE_READ_RAW   0x30
