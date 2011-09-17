/**
 * $Id$
 *
 * Nero (NRG) CD file format. File information stolen shamelessly from
 * libcdio.
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

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include "drivers/cdrom/cdimpl.h"
#include "dream.h"

static gboolean nrg_image_is_valid( FILE *f );
static gboolean nrg_image_read_toc( cdrom_disc_t disc, ERROR *err );

struct cdrom_disc_factory nrg_disc_factory = { "Nero", "nrg",
        nrg_image_is_valid, NULL, nrg_image_read_toc };

#define NERO_V55_ID  0x4e455235 
#define NERO_V50_ID  0x4e45524f 

/* Courtesy of libcdio */
/* 5.0 or earlier */
#define NERO_ID  0x4e45524f  /* Nero pre 5.5.x */
#define CUES_ID  0x43554553  /* Nero pre version 5.5.x-6.x */
#define DAOI_ID  0x44414f49
#define ETNF_ID  0x45544e46
#define SINF_ID  0x53494e46  /* Session information */
#define END_ID  0x454e4421
/* 5.5+ only */
#define NER5_ID  0x4e455235  /* Nero version 5.5.x */
#define CDTX_ID  0x43445458  /* CD TEXT */
#define CUEX_ID  0x43554558  /* Nero version 5.5.x-6.x */
#define DAOX_ID  0x44414f58  /* Nero version 5.5.x-6.x */
#define ETN2_ID  0x45544e32
#define MTYP_ID  0x4d545950  /* Disc Media type? */


union nrg_footer {
    struct nrg_footer_v50 {
        uint32_t dummy;
        uint32_t id;
        uint32_t offset;
    } v50;
    struct nrg_footer_v55 {
        uint32_t id;
        uint64_t offset;
    } __attribute__((packed)) v55;
};

struct nrg_chunk {
    uint32_t id;
    uint32_t length;
};

struct nrg_etnf {
    uint32_t offset;
    uint32_t length;
    uint32_t mode;
    uint32_t lba;
    uint32_t padding;
};

struct nrg_etn2 {
    uint64_t offset;
    uint64_t length;
    uint32_t mode;
    uint32_t lba;
    uint64_t padding;
};

struct nrg_cues {
    uint8_t type;
    uint8_t track;
    uint8_t control;
    uint8_t pad;
    uint32_t addr;
};

struct nrg_daoi {
    uint32_t length;
    char mcn[14];
    uint8_t disc_mode;
    uint8_t unknown[2]; /* always 01 01? */
    uint8_t track_count;
    struct nrg_daoi_track {
        char unknown[10];
        uint32_t sector_size __attribute__((packed)); /* Always 0? */
        uint8_t mode;
        uint8_t unknown2[3]; /* Always 00 00 01? */
        uint32_t pregap __attribute__((packed));
        uint32_t offset __attribute__((packed));
        uint32_t end __attribute__((packed));
    } track[0];
} __attribute__((packed));

struct nrg_daox {
    uint32_t length;
    char mcn[14];
    uint8_t disc_mode;
    uint8_t unknown[2]; /* always 01 01? */
    uint8_t track_count;
    struct nrg_daox_track {
        char unknown[10];
        uint32_t sector_size __attribute__((packed)); /* Always 0? */
        uint8_t mode;
        uint8_t unknown2[3]; /* Always 00 00 01? */
        uint64_t pregap __attribute__((packed));
        uint64_t offset __attribute__((packed));
        uint64_t end __attribute__((packed));
    } track[0];
} __attribute__((packed));


sector_mode_t static nrg_track_mode( uint8_t mode )
{
    switch( mode ) {
    case 0: return SECTOR_MODE1;
    case 2: return SECTOR_MODE2_FORM1;
    case 3: return SECTOR_SEMIRAW_MODE2;
    case 7: return SECTOR_CDDA;
    case 16: return SECTOR_CDDA_SUBCHANNEL;
    default: return -1;
    }
}

static gboolean nrg_image_is_valid( FILE *f )
{
    union nrg_footer footer;

    fseek( f, -12, SEEK_END );
    fread( &footer, sizeof(footer), 1, f );
    if( GUINT32_FROM_BE(footer.v50.id) == NERO_V50_ID ||
            GUINT32_FROM_BE(footer.v55.id) == NERO_V55_ID ) {
        return TRUE;
    } else {
        return FALSE;
    }
}

#define RETURN_PARSE_ERROR( ... ) do { SET_ERROR(err, LX_ERR_FILE_INVALID, __VA_ARGS__); return FALSE; } while(0)

static gboolean nrg_image_read_toc( cdrom_disc_t disc, ERROR *err )
{
    union nrg_footer footer;
    struct nrg_chunk chunk;
    struct nrg_daoi *dao;
    struct nrg_daox *daox;
    struct nrg_etnf *etnf;
    struct nrg_etn2 *etn2;
    gboolean end = FALSE;
    uint32_t chunk_id;
    int session_id = 1;
    int session_track_id = 0;
    int track_id = 0;
    int cue_track_id = 0, cue_track_count = 0;
    int i, count;

    FILE *f = cdrom_disc_get_base_file(disc);

    fseek( f, -12, SEEK_END );
    fread( &footer, sizeof(footer), 1, f );
    uint32_t start = 0;
    if( GUINT32_FROM_BE(footer.v50.id) == NERO_V50_ID ) {
        start = GUINT32_FROM_BE(footer.v50.offset);
    } else if( GUINT32_FROM_BE(footer.v55.id) == NERO_V55_ID ) {
        start = (uint32_t)GUINT64_FROM_BE(footer.v55.offset);
    } else {
        /* Not a (recognized) Nero image (should never happen) */
        RETURN_PARSE_ERROR("File is not an NRG image" );
    }
    if( fseek( f, start, SEEK_SET) != 0 ) {
        RETURN_PARSE_ERROR("File is not a valid NRG image" );
    }

    do {
        fread( &chunk, sizeof(chunk), 1, f );
        chunk.length = GUINT32_FROM_BE(chunk.length);
        char data[chunk.length];
        fread( data, chunk.length, 1, f );
        chunk_id = GUINT32_FROM_BE(chunk.id);
        switch( chunk_id ) {
        case CUES_ID:
        case CUEX_ID:
            cue_track_id = track_id;
            cue_track_count = ((chunk.length / sizeof(struct nrg_cues)) >> 1) - 1;
            track_id += cue_track_count;
            for( i=0; i<chunk.length; i+= sizeof(struct nrg_cues) ) {
                struct nrg_cues *cue = (struct nrg_cues *)(data+i);
                int track = 0;
                uint32_t lba;
                if( chunk_id == CUEX_ID ) {
                    lba = GUINT32_FROM_BE( cue->addr );
                } else {
                    lba = BCD_MSFTOLBA( cue->addr );
                }
                if( cue->track == 0 )
                    continue; /* Track 0. Leadin? always 0? */
                if( cue->track == 0xAA ) { /* end of disc */
                    disc->leadout = lba;
                } else {
                    track = BCDTOU8(cue->track) - 1;
                    if( (cue->control & 0x01) != 0 ) {
                        /* Track-start address */
                        disc->track[track].lba = lba;
                        disc->track[track].flags = cue->type;
                    }
                }
            }
            break;
        case DAOI_ID:
            dao = (struct nrg_daoi *)data;
            count = dao->track_count - cue_track_id;
            memcpy( disc->mcn, dao->mcn, 13 );
            disc->mcn[13] = '\0';
            if( dao->track_count != track_id ||
                count * 30 + 22 != chunk.length ) {
                RETURN_PARSE_ERROR( "Invalid NRG image file (bad DAOI block)" );
            }
            for( i=0; i<count; i++ ) {
                uint32_t offset = GUINT32_FROM_BE(dao->track[i].offset);
                sector_mode_t mode = nrg_track_mode( dao->track[i].mode );
                if( mode == -1 ) {
                    RETURN_PARSE_ERROR("Unknown track mode in NRG image file (%d)", dao->track[i].mode);
                }
                if( CDROM_SECTOR_SIZE(mode) != GUINT32_FROM_BE(dao->track[i].sector_size) ) {
                    /* Sector size mismatch */
                    RETURN_PARSE_ERROR("Invalid NRG image file (Bad sector size in DAOI block)");
                }
                cdrom_count_t sector_count =
                    (GUINT32_FROM_BE(dao->track[i].end) - GUINT32_FROM_BE(dao->track[i].offset))/
                    CDROM_SECTOR_SIZE(mode);
                disc->track[cue_track_id].source = file_sector_source_new_source( disc->base_source, mode, offset, sector_count );
                cue_track_id++;
            }
            break;
        case DAOX_ID:
            daox = (struct nrg_daox *)data;
            count = daox->track_count - cue_track_id;
            memcpy( disc->mcn, daox->mcn, 13 );
            disc->mcn[13] = '\0';
            if( daox->track_count != track_id ||
                count * 42 + 22 != chunk.length ) {
                RETURN_PARSE_ERROR( "Invalid NRG image file (bad DAOX block)" );
            }
            for( i=0; i<count; i++ ) {
                uint32_t offset = (uint32_t)GUINT64_FROM_BE(daox->track[i].offset);
                sector_mode_t mode = nrg_track_mode( daox->track[i].mode );
                if( mode == -1 ) {
                    RETURN_PARSE_ERROR("Unknown track mode in NRG image file (%d)", daox->track[i].mode);
                }
                if( CDROM_SECTOR_SIZE(mode) != GUINT32_FROM_BE(daox->track[i].sector_size) ) {
                    /* Sector size mismatch */
                    RETURN_PARSE_ERROR("Invalid NRG image file (Bad sector size in DAOX block)");
                }
                cdrom_count_t sector_count = (cdrom_count_t)
                    ((GUINT64_FROM_BE(daox->track[i].end) - GUINT64_FROM_BE(daox->track[i].offset))/
                    CDROM_SECTOR_SIZE(mode));
                disc->track[cue_track_id].source = file_sector_source_new_source( disc->base_source, mode, offset, sector_count );
                cue_track_id++;
            }
            break;

        case SINF_ID: 
            /* Data is a single 32-bit number representing number of tracks in session */
            i = GUINT32_FROM_BE( *(uint32_t *)data );
            while( i-- > 0 )
                disc->track[session_track_id++].sessionno = session_id;
            session_id++;
            break;
        case ETNF_ID:
            etnf = (struct nrg_etnf *)data;
            count = chunk.length / sizeof(struct nrg_etnf);
            for( i=0; i < count; i++, etnf++ ) {
                uint32_t offset = GUINT32_FROM_BE(etnf->offset);
                sector_mode_t mode = nrg_track_mode( GUINT32_FROM_BE(etnf->mode) );
                if( mode == -1 ) {
                    RETURN_PARSE_ERROR("Unknown track mode in NRG image file (%d)", etnf->mode);
                }
                cdrom_count_t sector_count = GUINT32_FROM_BE(etnf->length) /
                        CDROM_SECTOR_SIZE(mode);

                disc->track[track_id].lba = GUINT32_FROM_BE(etnf->lba) + i*CDROM_PREGAP;
                if( mode == SECTOR_CDDA )
                    disc->track[track_id].flags = 0x01;
                else
                    disc->track[track_id].flags = 0x01 | TRACK_FLAG_DATA;
                disc->track[track_id].source = file_sector_source_new_source( disc->base_source, mode, offset, sector_count );
                track_id++;
            }
            break;
        case ETN2_ID:
            etn2 = (struct nrg_etn2 *)data;
            count = chunk.length / sizeof(struct nrg_etn2);
            for( i=0; i < count; i++, etn2++ ) {
                uint32_t offset = (uint32_t)GUINT64_FROM_BE(etn2->offset);
                sector_mode_t mode = nrg_track_mode( GUINT32_FROM_BE(etn2->mode) );
                if( mode == -1 ) {
                    RETURN_PARSE_ERROR("Unknown track mode in NRG image file (%d)", etn2->mode);
                }
                cdrom_count_t sector_count = (uint32_t)(GUINT64_FROM_BE(etn2->length) /
                        CDROM_SECTOR_SIZE(mode));

                disc->track[track_id].lba = GUINT32_FROM_BE(etn2->lba) + i*CDROM_PREGAP;
                if( mode == SECTOR_CDDA )
                    disc->track[track_id].flags = 0x01;
                else
                    disc->track[track_id].flags = 0x01 | TRACK_FLAG_DATA;
                disc->track[track_id].source = file_sector_source_new_source( disc->base_source, mode, offset, sector_count );
                track_id++;
            }
            break;

        case END_ID:
            end = TRUE;
            break;
        }
    } while( !end );

    disc->track_count = track_id;
    disc->session_count = session_id-1;
    return TRUE;
}
