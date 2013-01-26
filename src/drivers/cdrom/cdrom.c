/**
 * $Id$
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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "lxdream.h"
#include "drivers/cdrom/cdrom.h"
#include "drivers/cdrom/cdimpl.h"
#include "drivers/cdrom/isofs.h"

extern struct cdrom_disc_factory linux_cdrom_drive_factory;
extern struct cdrom_disc_factory nrg_disc_factory;
extern struct cdrom_disc_factory cdi_disc_factory;
extern struct cdrom_disc_factory gdi_disc_factory;

cdrom_disc_factory_t cdrom_disc_factories[] = {
#ifdef HAVE_LINUX_CDROM
        &linux_cdrom_drive_factory,
#endif
        &nrg_disc_factory,
        &cdi_disc_factory,
        &gdi_disc_factory,
        NULL };

/********************* Implementation Support functions ************************/

cdrom_error_t default_image_read_blocks( sector_source_t source, cdrom_lba_t lba, cdrom_count_t count,
                                         unsigned char *buf )
{
    assert( 0 && "read_blocks called on a cdrom disc" );
    return CDROM_ERROR_BADREAD;
}

cdrom_error_t default_image_read_sectors( sector_source_t source, cdrom_lba_t lba, cdrom_count_t count,
                                          cdrom_read_mode_t mode, unsigned char *buf, size_t *length )
{
    assert( IS_SECTOR_SOURCE_TYPE(source,DISC_SECTOR_SOURCE) );
    cdrom_disc_t disc = (cdrom_disc_t)source;
    size_t len = 0, tmplen;
    cdrom_count_t current = 0;

    while( current < count ) {
        cdrom_track_t track = cdrom_disc_get_track_by_lba( disc, lba + current );
        if( track == NULL )
            return CDROM_ERROR_BADREAD;
        uint32_t track_size = cdrom_disc_get_track_size( disc, track );
        cdrom_lba_t track_offset = lba + current - track->lba;
        cdrom_count_t sub_count = count - current;
        if( track_size - track_offset < sub_count )
            /* Read breaks across track boundaries. This will probably fail (due
             * to inter-track gaps), but try it just in case
             */
            sub_count = track_size - track_offset;
        cdrom_error_t err = track->source->read_sectors( track->source, track_offset, sub_count, mode, &buf[len], &tmplen );
        if( err != CDROM_ERROR_OK )
            return err;
        len += tmplen;
        current += sub_count;
    }
    if( length != NULL )
        *length = len;
    return CDROM_ERROR_OK;
}

void default_cdrom_disc_destroy( sector_source_t source )
{
    assert( IS_SECTOR_SOURCE_TYPE(source,DISC_SECTOR_SOURCE) );
    cdrom_disc_t disc = (cdrom_disc_t)source;
    int i;

    for( i=0; i<disc->track_count; i++ ) {
        sector_source_unref( disc->track[i].source );
    }
    sector_source_unref( disc->base_source );
    g_free( (char *)disc->name );

    default_sector_source_destroy( source );
}

cdrom_disc_t cdrom_disc_init( cdrom_disc_t disc, const char *filename )
{
    sector_source_init( &disc->source, DISC_SECTOR_SOURCE, SECTOR_UNKNOWN, 0, default_image_read_blocks,
            default_cdrom_disc_destroy );
    disc->source.read_sectors = default_image_read_sectors;
    disc->disc_type = CDROM_DISC_NONE;
    disc->track_count = disc->session_count = 0;
    for( int i=0; i<99; i++ ) {
        disc->track[i].trackno = i+1;
    }
    if( filename != NULL )
        disc->name = g_strdup(filename);
    return disc;
}

cdrom_disc_t cdrom_disc_new( const char *name, ERROR *err )
{
    cdrom_disc_t disc = g_malloc0( sizeof(struct cdrom_disc) );
    if( disc != NULL ) {
        cdrom_disc_init( disc, name );
    } else {
        SET_ERROR(err, LX_ERR_NOMEM, "Unable to allocate memory for cdrom disc");
    }
    return disc;
}

/**
 * Construct a new image-based disc using the given filename as the base source.
 * TOC is initialized to the empty values.
 */
static cdrom_disc_t cdrom_disc_image_new( const char *filename, ERROR *err )
{
    cdrom_disc_t disc = cdrom_disc_new( filename, err );
    if( disc != NULL && filename != NULL ) {
        disc->base_source = file_sector_source_new_filename( filename, SECTOR_UNKNOWN, 0, FILE_SECTOR_FULL_FILE );
        if( disc->base_source == NULL ) {
            SET_ERROR( err, LX_ERR_FILE_NOOPEN, "Unable to open cdrom file '%s': %s", filename, strerror(errno) );
            cdrom_disc_unref(disc);
            disc = NULL;
        } else {
            sector_source_ref(disc->base_source);
        }

    }
    return disc;
}

cdrom_lba_t cdrom_disc_compute_leadout( cdrom_disc_t disc )
{
    if( disc->track_count == 0 ) {
        disc->leadout = 0;
    } else {
        cdrom_track_t last_track = &disc->track[disc->track_count-1];
        if( last_track->source != NULL ) {
            cdrom_lba_t leadout = last_track->lba + last_track->source->size;
            if( leadout > disc->leadout )
                disc->leadout = leadout;
        }
    }
    return disc->leadout;
}


void cdrom_disc_set_default_disc_type( cdrom_disc_t disc )
{
    int type = CDROM_DISC_NONE, i;
    for( i=0; i<disc->track_count; i++ ) {
        if( (disc->track[i].flags & TRACK_FLAG_DATA == 0) ) {
            if( type == CDROM_DISC_NONE )
                type = CDROM_DISC_AUDIO;
        } else if( disc->track[i].source != NULL &&
                   (disc->track[i].source->mode == SECTOR_MODE1 ||
                    disc->track[i].source->mode == SECTOR_RAW_NONXA) ) {
            if( type != CDROM_DISC_XA )
                type = CDROM_DISC_NONXA;
        } else {
            type = CDROM_DISC_XA;
            break;
        }
    }
    disc->disc_type = type;
}

void cdrom_disc_clear_toc( cdrom_disc_t disc )
{
    disc->disc_type = CDROM_DISC_NONE;
    disc->leadout = 0;
    disc->track_count = 0;
    disc->session_count = 0;
    for( unsigned i=0; i< CDROM_MAX_TRACKS; i++ ) {
        if( disc->track[i].source != NULL ) {
            sector_source_unref( disc->track[i].source );
            disc->track[i].source = NULL;
        }
    }
}

gboolean cdrom_disc_read_toc( cdrom_disc_t disc, ERROR *err )
{
    if( disc->read_toc != NULL ) {
        /* First set the defaults for an empty disc */
        cdrom_disc_clear_toc(disc);

        if( disc->read_toc(disc, err ) ) {
            /* Success - update disc type and leadout if the TOC read didn't set them */
            if( disc->disc_type == CDROM_DISC_NONE )
                cdrom_disc_set_default_disc_type(disc);
            cdrom_disc_compute_leadout(disc);
            return TRUE;
        } else {
            /* Reset to an empty disc in case the reader left things in an
             * inconsistent state */
            cdrom_disc_clear_toc(disc);
            return FALSE;
        }
    } else {
        return TRUE;
    }
}

FILE *cdrom_disc_get_base_file( cdrom_disc_t disc )
{
    return file_sector_source_get_file(disc->base_source);
}

/*************************** Public functions ***************************/

cdrom_disc_t cdrom_disc_open( const char *inFilename, ERROR *err )
{
    const gchar *filename = inFilename;
    const gchar *ext = strrchr(filename, '.');
    int i;
    cdrom_disc_factory_t extclz = NULL;

    /* Ask the drive list if it recognizes the name first */
    cdrom_drive_t drive = cdrom_drive_find(inFilename);
    if( drive != NULL ) {
        return cdrom_drive_open(drive, err);
    }

    cdrom_disc_t disc = cdrom_disc_image_new( filename, err );
    if( disc == NULL )
        return NULL;

    /* check file extensions first */
    FILE *f = file_sector_source_get_file(disc->base_source);
    if( ext != NULL ) {
        ext++; /* Skip the '.' */
        for( i=0; cdrom_disc_factories[i] != NULL; i++ ) {
            if( cdrom_disc_factories[i]->extension != NULL &&
                    strcasecmp( cdrom_disc_factories[i]->extension, ext ) == 0 ) {
                extclz = cdrom_disc_factories[i];
                if( extclz->is_valid_file(f) ) {
                    disc->read_toc = extclz->read_toc;
                }
                break;
            }
        }
    }

    if( disc->read_toc == NULL ) {
        /* Okay, fall back to magic */
        for( i=0; cdrom_disc_factories[i] != NULL; i++ ) {
            if( cdrom_disc_factories[i] != extclz &&
                cdrom_disc_factories[i]->is_valid_file(f) ) {
                disc->read_toc = cdrom_disc_factories[i]->read_toc;
                break;
            }
        }
    }

    if( disc->read_toc == NULL ) {
        /* No handler found for file */
        cdrom_disc_unref( disc );
        SET_ERROR( err, LX_ERR_FILE_UNKNOWN, "File '%s' could not be recognized as any known image file or device type", filename );
        return NULL;
    } else if( !cdrom_disc_read_toc( disc, err ) ) {
        cdrom_disc_unref( disc );
        assert( err == NULL || err->code != LX_ERR_NONE ); /* Read-toc should have set an error code in this case */
        return NULL;
    } else {
        /* All good */
        return disc;
    }
}

/**
 * Construct a disc around a source track.
 * @param type Disc type, which must be compatible with the track mode
 * @param track The source of data for the main track
 * @param lba The position on disc of the main track. If non-zero,
 * a filler track is added before it, in 2 separate sessions.
 */
cdrom_disc_t cdrom_disc_new_from_track( cdrom_disc_type_t type, sector_source_t track, cdrom_lba_t lba, ERROR *err )
{
    cdrom_disc_t disc = cdrom_disc_new( NULL, NULL );
    if( disc != NULL ) {
        disc->disc_type = type;
        int trackno = 0;
        if( lba != 0 ) {
            cdrom_count_t size = lba - 150;
            if( lba < 150 )
                size = lba;
            disc->track[0].trackno = 1;
            disc->track[0].sessionno = 1;
            disc->track[0].lba = 0;
            disc->track[0].flags = 0;
            disc->track[0].source = null_sector_source_new( SECTOR_CDDA, size );
            sector_source_ref( disc->track[0].source );
            trackno++;
        }
        disc->track[trackno].trackno = trackno+1;
        disc->track[trackno].sessionno = trackno+1;
        disc->track[trackno].lba = lba;
        disc->track[trackno].flags = (track->mode == SECTOR_CDDA ? 0 : TRACK_FLAG_DATA);
        disc->track[trackno].source = track;
        sector_source_ref(track);

        disc->track_count = trackno+1;
        disc->session_count = trackno+1;
        cdrom_disc_compute_leadout(disc);
    } else {
        SET_ERROR(err, LX_ERR_NOMEM, "Unable to allocate memory for cdrom disc");
    }
    return disc;
}

/**
 * Construct a disc around an IsoImage track (convenience function)
 */
cdrom_disc_t cdrom_disc_new_from_iso_image( cdrom_disc_type_t type, IsoImage *iso, cdrom_lba_t lba,
                                            const char *bootstrap, ERROR *err )
{
    sector_mode_t mode = (type == CDROM_DISC_NONXA ? SECTOR_MODE1 : SECTOR_MODE2_FORM1 );
    sector_source_t source = iso_sector_source_new( iso, mode, lba, bootstrap, err );
    if( source != NULL ) {
        cdrom_disc_t disc = cdrom_disc_new_from_track(type, source, lba, err);
        if( disc == NULL ) {
            sector_source_unref( source );
        } else {
            return disc;
        }
    }
    return NULL;
}

/**
 * Get the track information for the given track. If there is no such track,
 * return NULL;
 */
cdrom_track_t cdrom_disc_get_track( cdrom_disc_t disc, cdrom_trackno_t track )
{
    if( track < 1 || track >= disc->track_count )
        return NULL;
    return &disc->track[track-1];
}

/**
 * Get the track information for the first track of the given session. If there
 * is no such session, return NULL;
 */
cdrom_track_t cdrom_disc_get_session( cdrom_disc_t disc, cdrom_sessionno_t session )
{
    for( unsigned i=0; i< disc->track_count; i++ ) {
        if( disc->track[i].sessionno == session )
            return &disc->track[i];
    }
    return NULL;
}

cdrom_count_t cdrom_disc_get_track_size( cdrom_disc_t disc, cdrom_track_t track )
{
    if( track->trackno == disc->track_count )
        return disc->leadout - track->lba;
    else
        return disc->track[track->trackno].lba - track->lba;
}

cdrom_track_t cdrom_disc_get_last_track( cdrom_disc_t disc )
{
    if( disc->track_count == 0 )
        return NULL;
    return &disc->track[disc->track_count-1];
}

cdrom_track_t cdrom_disc_get_last_data_track( cdrom_disc_t disc )
{
    for( unsigned i=disc->track_count; i>0; i-- ) {
        if( disc->track[i-1].flags & TRACK_FLAG_DATA ) {
            return &disc->track[i-1];
        }
    }
    return NULL;
}
cdrom_track_t cdrom_disc_prev_track( cdrom_disc_t disc, cdrom_track_t track )
{
    if( track->trackno <= 1 )
        return NULL;
    return cdrom_disc_get_track( disc, track->trackno-1 );
}

cdrom_track_t cdrom_disc_next_track( cdrom_disc_t disc, cdrom_track_t track )
{
    if( track->trackno >= disc->track_count )
        return NULL;
    return cdrom_disc_get_track( disc, track->trackno+1 );
}

/**
 * Find the track containing the sector specified by LBA.
 * Note: this function does not check for media change.
 * @return The track, or NULL if no track contains the sector.
 */
cdrom_track_t cdrom_disc_get_track_by_lba( cdrom_disc_t disc, cdrom_lba_t lba )
{
    if( disc->track_count == 0 || disc->track[0].lba > lba || lba >= disc->leadout )
        return NULL; /* LBA outside disc bounds */

    for( unsigned i=1; i< disc->track_count; i++ ) {
        if( lba < disc->track[i].lba )
            return &disc->track[i-1];
    }
    return &disc->track[disc->track_count-1];
}

cdrom_error_t cdrom_disc_read_sectors( cdrom_disc_t disc, cdrom_lba_t lba, cdrom_count_t count,
                                       cdrom_read_mode_t mode, unsigned char *buf, size_t *length )
{
    return disc->source.read_sectors( &disc->source, lba, count, mode, buf, length );
}

/**
 * Check if the disc contains valid media.
 * @return CDROM_ERROR_OK if disc is present, otherwise CDROM_ERROR_NODISC
 */
cdrom_error_t cdrom_disc_check_media( cdrom_disc_t disc )
{
    if( disc == NULL )
        return CDROM_ERROR_NODISC;
    if( disc->check_media != NULL )
        disc->check_media(disc);
    return disc->disc_type == CDROM_DISC_NONE ? CDROM_ERROR_NODISC : CDROM_ERROR_OK;
}

void cdrom_disc_print_toc( FILE *f, cdrom_disc_t disc )
{
    int i;
    int session = 0;

    if( disc == NULL || disc->track_count == 0 ) {
        fprintf( f, "No disc\n" );
        return;
    }
    for( i=0; i<disc->track_count; i++ ) {
        cdrom_track_t track = &disc->track[i];
        if( track->sessionno != session ) {
            session = disc->track[i].sessionno;
            fprintf( f, "Session %d:\n", session );
        }
        fprintf( f, "  %02d. %6d %02x\n", track->trackno, track->lba, track->flags );
    }
}

void cdrom_disc_dump_toc( cdrom_disc_t disc )
{
    cdrom_disc_print_toc( stderr, disc );
}
