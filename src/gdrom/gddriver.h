/**
 * $Id$
 *
 * This file defines the structures and functions used by the GD-Rom
 * disc drivers. (ie, the modules that supply a CD image to be used by the
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

#ifndef lxdream_gddriver_H
#define lxdream_gddriver_H 1

#include <stdio.h>
#include "lxdream.h"
#include "gdrom/gdrom.h"
#include <glib/gstrfuncs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SECTOR_SIZE 2352

#define CD_MSF_START 150 /* MSF numbering starts after the initial pregap */
#define CD_FRAMES_PER_SECOND 75
#define CD_SECONDS_PER_MINUTE 60
#define CD_FRAMES_PER_MINUTE (CD_FRAMES_PER_SECOND*CD_SECONDS_PER_MINUTE)

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

#define IDE_DISC_NONE    0x06
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
    uint32_t offset;      /* File offset of start of track (image files only) */
    FILE *file;           /* Per-track file handle (if any) */
} *gdrom_track_t;

struct gdrom_disc {
    int disc_type;     /* One of the IDE_DISC_* flags */
    const gchar *name; /* Device name / Image filename (owned) */
    const gchar *display_name; /* User-friendly device name, if any (owned) */
    gchar mcn[14]; /* Media catalogue number */
    char title[129]; /* Disc title (if any) from bootstrap */
    int track_count;
    struct gdrom_track track[99];
    FILE *file;       /* Image file / device handle */
    void *impl_data; /* Implementation private data */

	/* Check for media change. If the media cannot change (ie image file)
	 * or is notified asynchonously, this should be a no-op. In the event of
	 * a change, this function should update the structure according to the
	 * new media (including TOC), and return TRUE.
	 * @return TRUE if the media has changed since the last check, otherwise
	 * FALSE.
	 */
	gboolean (*check_status)( struct gdrom_disc *disc );

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
     * Begin playing audio from the given lba address on the disc.
     */
    gdrom_error_t (*play_audio)(struct gdrom_disc *disc, uint32_t lba, uint32_t endlba);

	/**
	 * Stop audio playback
	 */
	gdrom_error_t (*stop_audio)(struct gdrom_disc *disc);

    /**
     * Executed once per time slice to perform house-keeping operations 
     * (checking disc status, media changed, etc).
     */
    uint32_t (*run_time_slice)( struct gdrom_disc *disc, uint32_t nanosecs );

	/**
	 * Release all memory and system resources, including the gdrom_disc itself.
	 * (implicitly calls close() if not already closed. 
	 * @param disc The disc to destroy
	 * @param close_fh if TRUE, close the main file/device, otherwise leave open.
	 * This is mainly used when the handle will be immediately reused.
	 */
    void (*destroy)( struct gdrom_disc *disc, gboolean close_fh );
};

/**
 * Low-level SCSI transport provided to the main SCSI/MMC driver. When used
 * this will be set as the disc->impl_data field.
 * Note: For symmetry there should be a packet_write variant, but we don't
 * currently need it for anything. YAGNI, etc.
 */
typedef struct gdrom_scsi_transport {
	/* Execute a read command (ie a command that returns a block of data in
	 * response, not necessarily a CD read). 
	 * @param scsi The disc to execute the command
	 * @param cmd  The 12-byte command packet
	 * @param buf  The buffer to receive the read results
	 * @param length On entry, the size of buf. Modified on exit to the number
	 *        of bytes actually read.
	 * @return PKT_ERR_OK on success, otherwise the host error code.
	 */
	gdrom_error_t (*packet_read)( struct gdrom_disc *disc,
	                              char *cmd, unsigned char *buf,
	                              unsigned int *length );
	                              
	/* Execute a generic command that does not write or return any data.
	 * (eg play audio).
	 * @param scsi The disc to execute the command
	 * @param cmd  The 12-byte command packet
	 * @return PKT_ERR_OK on success, otherwise the host error code.
	 */
	gdrom_error_t (*packet_cmd)( struct gdrom_disc *disc,
	                             char *cmd );
	
	/* Return TRUE if the media has changed since the last call, otherwise
	 * FALSE. This method is used to implement the disc-level check_status
	 * and should have no side-effects.
	 */
	gboolean (*media_changed)( struct gdrom_disc *disc );
} *gdrom_scsi_transport_t;

/**
 * Allocate a new gdrom_disc_t and initialize the filename and file fields.
 * The disc is otherwise uninitialized - this is an internal method for use 
 * by the concrete implementations.
 */
gdrom_disc_t gdrom_disc_new(const gchar *filename, FILE *f);

/**
 * Construct a new SCSI/MMC disc using the supplied transport implementation.
 */
gdrom_disc_t gdrom_scsi_disc_new(const gchar *filename, FILE *f, gdrom_scsi_transport_t transport);

/**
 * Construct a new image file using the default methods.
 */
gdrom_disc_t gdrom_image_new( const gchar *filename, FILE *f );

#define SCSI_TRANSPORT(disc)  ((gdrom_scsi_transport_t)disc->impl_data)

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
 * Determine the track number containing the specified sector by lba.
 */
int gdrom_disc_get_track_by_lba( gdrom_disc_t image, uint32_t lba );

/**
 * Default disc destroy method, for chaining from subclasses
 */
void gdrom_disc_destroy( gdrom_disc_t disc, gboolean close_fh );

gdrom_device_t gdrom_device_new( const gchar *name, const gchar *dev_name );

void gdrom_device_destroy( gdrom_device_t dev );

/************* Host-native support functions ***************/

/**
 * Given a raw (2352 byte) data sector, extract the requested components into the 
 * target buffer. length will also be updated with the length of the copied
 * data
 */
void gdrom_extract_raw_data_sector( char *sector_data, int mode, unsigned char *buf, uint32_t *length );

/**
 * Check the disc for a useable DC bootstrap, and update the disc
 * with the title accordingly.
 * @return TRUE if we found a bootstrap, otherwise FALSE.
 */
gboolean gdrom_disc_read_title( gdrom_disc_t disc ); 

/** 
 * Parse a TOC mode-2 result buffer into the gdrom_disc_t data structure
 */
void mmc_parse_toc2( gdrom_disc_t disc, unsigned char *buf );

/**
 * Set the disc type flag based on the track contents
 */
void gdrom_set_disc_type( gdrom_disc_t disc );

#endif /* !lxdream_gddriver_H */
