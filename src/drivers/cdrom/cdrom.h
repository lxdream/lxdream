/**
 * $Id$
 *
 * Copyright (c) 2005-2009 Nathan Keynes.
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

#ifndef cdrom_cdrom_H
#define cdrom_cdrom_H 1

#include <stdio.h>
#include <glib/glist.h>
#include "drivers/cdrom/defs.h"
#include "drivers/cdrom/sector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CDROM_DISC_NONE =  0x06,
    CDROM_DISC_AUDIO = 0x00,
    CDROM_DISC_NONXA = 0x10,
    CDROM_DISC_XA    = 0x20,
    CDROM_DISC_GDROM = 0x80
} cdrom_disc_type_t;

#define TRACK_FLAG_PREEMPH   0x10 /* Pre-emphasis (audio only) */
#define TRACK_FLAG_COPYPERM  0x20 /* Copy permitted */
#define TRACK_FLAG_DATA      0x40 /* Data track */
#define TRACK_FLAG_FOURCHAN  0x80 /* 4-channel audio */

struct cdrom_track {
    cdrom_trackno_t trackno;
    cdrom_sessionno_t sessionno;  /* session # containing this track */
    cdrom_lba_t lba;            /* start sector address */
    uint8_t flags;              /* Track flags */
    sector_source_t source;
};

/**
 * A CDROM disc, either an image file, or an open physical host device.
 */
struct cdrom_disc {
    struct sector_source source;
    const char *name; /* Filename or identifier used to open the disc */
    cdrom_disc_type_t disc_type;
    gchar mcn[14]; /* Media catalogue number, null terminated. */
    cdrom_trackno_t track_count;
    cdrom_sessionno_t session_count;
    cdrom_lba_t leadout; /* LBA of the disc leadout */
    struct cdrom_track track[99];

    /* Reference to an underlying source, if any. */
    sector_source_t base_source;

    /* Private implementation-specific data */
    void *impl_data;

    /** Check for media change. If the media cannot change (ie image file)
     * or is notified asynchonously, this should be a no-op. In the event of
     * a change, this function should update the structure according to the
     * new media (including TOC), and return TRUE.
     * @return TRUE if the media has changed since the last check, otherwise
     * FALSE.
     */
    gboolean (*check_media)(cdrom_disc_t disc);

    /**
     * Read the table of contents from the given file
     */
    gboolean (*read_toc)(cdrom_disc_t disc, ERROR *err);

    /**
     * Begin playing audio from the given lba address on the disc.
     */
    cdrom_error_t (*play_audio)(cdrom_disc_t disc, cdrom_lba_t lba, cdrom_count_t length);

    cdrom_error_t (*scan_audio)(cdrom_disc_t disc, cdrom_lba_t lba, gboolean direction);

    cdrom_error_t (*stop_audio)(cdrom_disc_t disc);

};

/**
 * Open an image file or device
 */
cdrom_disc_t cdrom_disc_open( const char *filename, ERROR *err );

/**
 * Construct a disc around a source track.
 * @param type Disc type, which must be compatible with the track mode
 * @param track The source of data for the main track
 * @param lba The position on disc of the main track. If non-zero,
 * a filler track is added before it, in 2 separate sessions.
 */
cdrom_disc_t cdrom_disc_new_from_track( cdrom_disc_type_t type, sector_source_t track, cdrom_lba_t lba );

/**
 * Get the track information for the given track. If there is no such track,
 * return NULL;
 */
cdrom_track_t cdrom_disc_get_track( cdrom_disc_t disc, cdrom_trackno_t track );

/**
 * Get the track information for the first track of the given session. If there
 * is no such session, return NULL;
 */
cdrom_track_t cdrom_disc_get_session( cdrom_disc_t disc, cdrom_sessionno_t session );

cdrom_track_t cdrom_disc_get_last_track( cdrom_disc_t disc );

/**
 * Get the track information for the last data track  on the disc
 */
cdrom_track_t cdrom_disc_get_last_data_track( cdrom_disc_t disc );

cdrom_track_t cdrom_disc_prev_track( cdrom_disc_t disc, cdrom_track_t track );
cdrom_track_t cdrom_disc_next_track( cdrom_disc_t disc, cdrom_track_t track );

/**
 * Return the size of the track in sectors, including inter-track gap
 */
cdrom_count_t cdrom_disc_get_track_size( cdrom_disc_t disc, cdrom_track_t track );

/**
 * Find the track containing the sector specified by LBA.
 * Note: this function does not check for media change.
 * @return The track, or NULL if no track contains the sector.
 */
cdrom_track_t cdrom_disc_get_track_by_lba( cdrom_disc_t disc, cdrom_lba_t lba );

/** 
 * Check if the disc contains valid media.
 * @return CDROM_ERROR_OK if disc is present, otherwise CDROM_ERROR_NODISC
 */
cdrom_error_t cdrom_disc_check_media( cdrom_disc_t disc );

/**
 * Read sectors from the disc.
 * @return status code
 */
cdrom_error_t cdrom_disc_read_sectors( cdrom_disc_t disc, cdrom_lba_t lba, cdrom_count_t count, cdrom_read_mode_t mode,
                                       unsigned char *buf, size_t *length );

/**
 * Print the disc's table of contents to the given output stream.
 */
void cdrom_disc_print_toc( FILE *f, cdrom_disc_t disc );

#define cdrom_disc_ref(disc) sector_source_ref((sector_source_t)disc)
#define cdrom_disc_unref(disc) sector_source_unref((sector_source_t)disc)

#ifdef __cplusplus
}
#endif

#endif /* !cdrom_cdrom_H */
