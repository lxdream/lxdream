/**
 * $Id$
 *
 * Global cdrom definitions.
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

#ifndef cdrom_defs_H
#define cdrom_defs_H 1

#include "lxdream.h"
#include <stdint.h>
#include <glib/gtypes.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t cdrom_lba_t;
typedef uint32_t cdrom_count_t;

/** Tracks are numbered 1..99, with 0 reserved for errors */
typedef uint8_t  cdrom_trackno_t;

typedef uint8_t  cdrom_sessionno_t;

typedef const struct cdrom_track *cdrom_track_t;
typedef struct cdrom_disc *cdrom_disc_t;

/** sector read mode - values are based on the MMC READ CD command. */
typedef uint16_t cdrom_read_mode_t;
#define CDROM_READ_ANY         (0<<2)
#define CDROM_READ_CDDA        (1<<2)
#define CDROM_READ_MODE1       (2<<2)
#define CDROM_READ_MODE2       (3<<2)
#define CDROM_READ_MODE2_FORM1 (4<<2)
#define CDROM_READ_MODE2_FORM2 (5<<2)

#define CDROM_READ_NONE      0x0000
#define CDROM_READ_SYNC      0x8000
#define CDROM_READ_DATA      0x1000
#define CDROM_READ_ECC       0x0800
#define CDROM_READ_HEADER    0x2000
#define CDROM_READ_SUBHEADER 0x4000
#define CDROM_READ_RAW       0xF800 /* Read full sector */
#define CDROM_READ_TYPE(x)   ((x) & 0x1C)
#define CDROM_READ_FIELDS(x) ((x) & 0xF800)

/** Actual sector mode */
typedef enum {
    SECTOR_UNKNOWN,        // Unknown sector mode
    SECTOR_CDDA,           // Standard audio track
    /* Data-only modes */
    SECTOR_MODE1,          // Standard CD-Rom Mode 1 data track
    SECTOR_MODE2_FORMLESS, // Mode 2 data track with no sub-structure (rare)
    SECTOR_MODE2_FORM1,    // Mode 2/Form 1 data track (standard for multisession)
    SECTOR_MODE2_FORM2,    // Mode 2/Form 2 data track (also fairly uncommon).

    /* 2336-byte Mode 2 XA sector with subheader and ecc data */
    SECTOR_SEMIRAW_MODE2,
    /* 2352-byte raw data sector in an XA session */
    SECTOR_RAW_XA,
    /* 2352-byte raw data sector in a non-XA session */
    SECTOR_RAW_NONXA,
    /* CDDA + subchannel data */
    SECTOR_CDDA_SUBCHANNEL
} sector_mode_t;


extern const uint32_t cdrom_sector_size[];
extern const uint32_t cdrom_sector_read_mode[];
#define CDROM_MAX_SECTOR_SIZE    2352
#define CDROM_MAX_TRACKS         99
#define CDROM_MSF_START          150 /* MSF numbering starts after the initial pregap */
#define CDROM_FRAMES_PER_SECOND  75
#define CDROM_SECONDS_PER_MINUTE 60
#define CDROM_FRAMES_PER_MINUTE  (CDROM_FRAMES_PER_SECOND*CDROM_SECONDS_PER_MINUTE)
#define CDROM_PREGAP             150  /* Standard pregap, in frames */
#define CDROM_SECTOR_SIZE(x)     cdrom_sector_size[x]
#define CDROM_SECTOR_READ_MODE(x) cdrom_sector_read_mode[x]
#define MSFTOLBA( m,s,f ) ((f) + ((s)*CDROM_FRAMES_PER_SECOND) + ((m)*CDROM_FRAMES_PER_MINUTE) - CDROM_MSF_START)

/**
 * Convert an 8-bit BCD number to integer form.
 * Eg, 0x79 => 79
 */
uint8_t static inline BCDTOU8( uint8_t bcd )
{
    return (bcd & 0x0F) + (((bcd & 0xF0)>>4)*10);
}

/**
 * Convert a 32 bit BCD-encoded MSF address to the
 * equivalent LBA form.
 * Eg, 0x
 */
cdrom_lba_t static inline BCD_MSFTOLBA( uint32_t msf )
{
    msf = GUINT32_FROM_BE(msf);
    int f = BCDTOU8(msf);
    int s = BCDTOU8(msf>>8);
    int m = BCDTOU8(msf>>16);
    return MSFTOLBA(m,s,f);
}

/* Disc types */
typedef uint8_t cdrom_type_t;
#define CDROM_TYPE_NONXA 0x00  /* Audio or straight mode-1 data */
#define CDROM_TYPE_CDI   0x10
#define CDROM_TYPE_XA    0x20
#define CDROM_TYPE_GD    0x80  /* SEGA only */


/* Error codes are defined as MMC sense data - low byte is the sense key,
 * next byte, is the ASC code, and third byte is the ASCQ (not currently used)
 */
typedef uint32_t cdrom_error_t;
#define CDROM_ERROR_OK        0x0000
#define CDROM_ERROR_NODISC    0x3A02
#define CDROM_ERROR_BADCMD    0x2005
#define CDROM_ERROR_BADFIELD  0x2405
#define CDROM_ERROR_BADREAD   0x3002
#define CDROM_ERROR_BADREADMODE 0x6405  /* Illegal mode for this track */
#define CDROM_ERROR_READERROR 0x1103    /* Read failed due to uncorrectable error */
#define CDROM_ERROR_RESET     0x2906

#ifdef __cplusplus
}
#endif

#endif /* !cdrom_defs_H */
