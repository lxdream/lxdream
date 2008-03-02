/**
 * $Id$
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
#include <glib.h>

#define MAX_SECTOR_SIZE 2352

typedef uint16_t gdrom_error_t;

struct gdrom_toc {
    uint32_t track[99];
    uint32_t first, last, leadout;
};

#define GDROM_PREGAP 150  /* Sectors */

extern uint32_t gdrom_sector_size[];
#define GDROM_SECTOR_SIZE(x) gdrom_sector_size[x]
/**
 * Track data type enumeration for cd images and devices. This somewhat
 * conflates the real track mode with the format of the image file, but
 * it manages to make sense so far.
 */
typedef enum {
    GDROM_MODE0,          // Mode 0 - should never actually see this
/* Data-only modes (image file contains only the user data) */
    GDROM_MODE1,          // Standard CD-Rom Mode 1 data track
    GDROM_MODE2_FORMLESS, // Mode 2 data track with no sub-structure (rare)
    GDROM_MODE2_FORM1,    // Mode 2/Form 1 data track (standard for multisession)
    GDROM_MODE2_FORM2,    // Mode 2/Form 2 data track (also fairly uncommon).
    GDROM_CDDA,           // Standard audio track

/* This one is somewhat special - the image file contains the 2336 bytes of
 * "extended user data", which in turn contains either a form 1 or form 2
 * sector. In other words it's a raw mode2 XA sector without the 16-byte header.
 */
    GDROM_SEMIRAW_MODE2,
/* Raw modes (image contains the full 2352-byte sector). Split into XA/Non-XA
 * here for convenience, although it's really a session level flag. */
    GDROM_RAW_XA,
    GDROM_RAW_NONXA,
} gdrom_track_mode_t;

/* The disc register indicates the current contents of the drive. When open
 * contains 0x06.
 */
#define IDE_DISC_READY 0x01 /* ored with above */
#define IDE_DISC_IDLE  0x02 /* ie spun-down */
#define IDE_DISC_NONE  0x06

#define IDE_DISC_AUDIO   0x00
#define IDE_DISC_CDROM   0x10
#define IDE_DISC_CDROMXA 0x20
#define IDE_DISC_GDROM   0x80

#define TRACK_PRE_EMPHASIS   0x10
#define TRACK_COPY_PERMITTED 0x20
#define TRACK_DATA           0x40
#define TRACK_FOUR_CHANNEL   0x80

typedef struct gdrom_track {
    gdrom_track_mode_t mode;
    uint8_t flags;        /* Track flags */
    int      session;     /* session # containing this track */
    uint32_t lba;         /* start sector address */
    uint32_t sector_size; /* For convenience, determined by mode */
    uint32_t sector_count;
    uint32_t offset; /* File offset of start of track - image files only */
    FILE *file;
} *gdrom_track_t;

typedef struct gdrom_disc {
    /**
     * Read a single sector from the disc at the specified logical address.
     * @param disc pointer to the disc structure
     * @param lba logical address to read from
     * @param mode mode field from the read command
     * @param buf buffer to receive data (at least MAX_SECTOR_SIZE bytes)
     * @param length unsigned int to receive the number of bytes actually read.
     * @return PKT_ERR_OK on success, or another PKT_ERR_* code on failure.
     */
    gdrom_error_t (*read_sector)( struct gdrom_disc *disc,
				  uint32_t lba, int mode, 
				  unsigned char *buf, uint32_t *length );
    
    /**
     * Read the TOC from the disc and write it into the specified buffer.
     * The method is responsible for returning the data in gd-rom
     * format.
     * @param disc pointer to the disc structure
     * @param buf buffer to receive data (0x198 bytes long)
     */
    gdrom_error_t (*read_toc)(struct gdrom_disc *disc, unsigned char *buf);

    /**
     * Read the information for the specified sector and return it in the
     * supplied buffer. 
     * @param disc pointer to the disc structure
     * @param session of interest. If 0, return end of disc information.
     * @param buf buffer to receive data (6 bytes)
     */
    gdrom_error_t (*read_session)(struct gdrom_disc *disc, int session, unsigned char *buf);

    /**
     * Read the position information (subchannel) for the specified sector
     * and return it in the supplied buffer. This method does not need to
     * write the first 4 bytes of the buffer.
     * @param disc pointer to the disc structure
     * @param lba sector to get position information for
     * @param buf buffer to receive data (14 bytes)
     */
    gdrom_error_t (*read_position)(struct gdrom_disc *disc, uint32_t lba, unsigned char *buf);

    /**
     * Return the current disc status, expressed as a combination of the 
     * IDE_DISC_* flags above.
     * @param disc pointer to the disc structure
     * @return an integer status value.
     */
    int (*drive_status)(struct gdrom_disc *disc);

    /**
     * Begin playing audio from the given lba address on the disc.
     */
    gdrom_error_t (*play_audio)(struct gdrom_disc *disc, uint32_t lba, uint32_t endlba);

    /**
     * Executed once per time slice to perform house-keeping operations 
     * (checking disc status, media changed, etc).
     */
    uint32_t (*run_time_slice)( struct gdrom_disc *disc, uint32_t nanosecs );

    /**
     * Close the disc and release any storage or resources allocated including
     * the disc structure itself.
     */
    void (*close)( struct gdrom_disc *disc );
    const gchar *name; /* Device name / Image filename */
} *gdrom_disc_t;


typedef struct gdrom_image {
    struct gdrom_disc disc;
    int disc_type;
    int track_count;
    struct gdrom_track track[99];
    gchar mcn[14]; /* Media catalogue number */
    FILE *file; /* Open file stream */
} *gdrom_image_t;

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
extern struct gdrom_image_class gdi_image_class;
extern struct gdrom_image_class cdrom_device_class;

/**
 * Construct a new image file using the default methods.
 */
gdrom_disc_t gdrom_image_new( const gchar *filename, FILE *f );

/**
 * Open an image file
 */
gdrom_disc_t gdrom_image_open( const gchar *filename );

/**
 * Dump image info
 */
void gdrom_image_dump_info( gdrom_disc_t d );

/**
 * Destroy an image data structure without closing the file
 * (Intended for use from image loaders only)
 */
void gdrom_image_destroy_no_close( gdrom_disc_t d );

/**
 * Retrieve the disc table of contents, and write it into the buffer in the 
 * format expected by the DC.
 * @return 0 on success, error code on failure (eg no disc mounted)
 */
gdrom_error_t gdrom_get_toc( unsigned char *buf );

/**
 * Retrieve the short (6-byte) session info, and write it into the buffer.
 * @return 0 on success, error code on failure.
 */
gdrom_error_t gdrom_get_info( unsigned char *buf, int session );

gdrom_track_t gdrom_get_track( int track_no );

uint8_t gdrom_get_track_no_by_lba( uint32_t lba );

/**
 * Shortcut to open and mount an image file
 * @return true on success
 */
gboolean gdrom_mount_image( const gchar *filename );

void gdrom_mount_disc( gdrom_disc_t disc );

void gdrom_unmount_disc( void );

gboolean gdrom_is_mounted( void );

gdrom_disc_t gdrom_get_current_disc();

GList *gdrom_get_native_devices();

uint32_t gdrom_read_sectors( uint32_t sector, uint32_t sector_count,
			     int mode, unsigned char *buf, uint32_t *length );

/**
 * Given a base filename (eg for a .cue file), generate the path for the given
 * find_name relative to the original file. 
 * @return a newly allocated string.
 */
gchar *gdrom_get_relative_filename( const gchar *base_name, const gchar *find_name );

#endif
