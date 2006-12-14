/**
 * $Id: gdrom.h,v 1.8 2006-12-14 12:31:38 nkeynes Exp $
 *
 * This file defines the structures and functions used by the GD-Rom
 * disc driver. (ie, the modules that supply a CD image to be used by the
 * system).
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

#ifndef dream_gdrom_H
#define dream_gdrom_H 1

#include "dream.h"

typedef uint16_t gdrom_error_t;

struct gdrom_toc {
    uint32_t track[99];
    uint32_t first, last, leadout;
};

#define GDROM_PREGAP 150  /* Sectors */

extern uint32_t gdrom_sector_size[];
#define GDROM_SECTOR_SIZE(x) gdrom_sector_size[x]
typedef enum {
    GDROM_MODE1,
    GDROM_MODE2,
    GDROM_MODE2_XA1,
    GDROM_MODE2_XA2,
    GDROM_CDDA,
    GDROM_GD,
    GDROM_RAW
} gdrom_track_mode_t;

/* The disc register indicates the current contents of the drive. When open
 * contains 0x06.
 */
#define IDE_DISC_AUDIO 0x00
#define IDE_DISC_NONE  0x06
#define IDE_DISC_CDROM 0x20
#define IDE_DISC_GDROM 0x80
#define IDE_DISC_READY 0x01 /* ored with above */
#define IDE_DISC_IDLE  0x02 /* ie spun-down */

#define TRACK_PRE_EMPHASIS   0x10
#define TRACK_COPY_PERMITTED 0x20
#define TRACK_DATA           0x40
#define TRACK_FOUR_CHANNEL   0x80

struct gdrom_track {
    gdrom_track_mode_t mode;
    uint8_t flags;        /* Track flags */
    int      session;     /* session # containing this track */
    uint32_t lba;         /* start sector address */
    uint32_t sector_size; /* For convenience, determined by mode */
    uint32_t sector_count;
    uint32_t offset; /* File offset of start of track - image files only */
};


typedef struct gdrom_disc {
    int disc_type;
    int track_count;
    struct gdrom_track track[99];
    gchar mcn[14]; /* Media catalogue number */
    const gchar *filename; /* Image filename */
    FILE *file; /* Stream, for image files */
    gdrom_error_t (*read_sectors)( struct gdrom_disc *disc,
			      uint32_t lba, uint32_t sector_count,
			      int mode, char *buf, uint32_t *length );
    void (*close)( struct gdrom_disc *disc );
} *gdrom_disc_t;

/**
 *
 */
typedef struct gdrom_image_class {
    const gchar *name;
    const gchar *extension;
    gboolean (*is_valid_file)(FILE *f);
    gdrom_disc_t (*open_image_file)(const gchar *filename, FILE *f);
} *gdrom_image_class_t;

extern struct gdrom_image_class nrg_image_class;
extern struct gdrom_image_class cdi_image_class;
extern struct gdrom_image_class linux_device_class;

/**
 * Construct a new image file using the default methods.
 */
gdrom_disc_t gdrom_image_new( FILE *file );

/**
 * Open an image file
 */
gdrom_disc_t gdrom_image_open( const gchar *filename );

/**
 * Retrieve the disc table of contents, and write it into the buffer in the 
 * format expected by the DC.
 * @return 0 on success, error code on failure (eg no disc mounted)
 */
gdrom_error_t gdrom_get_toc( char *buf );

/**
 * Retrieve the short (6-byte) session info, and write it into the buffer.
 * @return 0 on success, error code on failure.
 */
gdrom_error_t gdrom_get_info( char *buf, int session );

/**
 * Shortcut to open and mount an image file
 */
gdrom_disc_t gdrom_mount_image( const gchar *filename );

void gdrom_mount_disc( gdrom_disc_t disc );

void gdrom_unmount_disc( void );

gboolean gdrom_is_mounted( void );

uint32_t gdrom_read_sectors( uint32_t sector, uint32_t sector_count,
			     int mode, char *buf, uint32_t *length );

#endif
