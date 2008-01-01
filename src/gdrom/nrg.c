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
#include <glib/gtypes.h>
#include "gdrom/gdrom.h"
#include "dream.h"

static gboolean nrg_image_is_valid( FILE *f );
static gdrom_disc_t nrg_image_open( const gchar *filename, FILE *f );

struct gdrom_image_class nrg_image_class = { "Nero", "nrg", 
					     nrg_image_is_valid, nrg_image_open };

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
    } v55;
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

/**
 * Convert an 8-bit BCD number to normal integer form. 
 * Eg, 0x79 => 79
 */
uint8_t static bcd_to_uint8( uint8_t bcd )
{
    return (bcd & 0x0F) + (((bcd & 0xF0)>>4)*10);
}


/**
 * Convert a 32 bit MSF address (BCD coded) to the
 * equivalent LBA form. 
 * Eg, 0x
 */
uint32_t static msf_to_lba( uint32_t msf )
{
    msf = GUINT32_FROM_BE(msf);
    int f = bcd_to_uint8(msf);
    int s = bcd_to_uint8(msf>>8);
    int m = bcd_to_uint8(msf>>16);
    return (m * 60 + s) * 75 + f;

}

uint32_t static nrg_track_mode( uint8_t mode )
{
    switch( mode ) {
    case 0: return GDROM_MODE1;
    case 2: return GDROM_MODE2_XA1;
    case 3: return GDROM_MODE2;
    case 7: return GDROM_CDDA;
    default: 
	ERROR( "Unrecognized track mode %d in Nero image", mode );
	return -1;
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

static gdrom_disc_t nrg_image_open( const gchar *filename, FILE *f )
{
    union nrg_footer footer;
    struct nrg_chunk chunk;
    struct nrg_daoi *dao;
    struct nrg_daox *daox;
    struct nrg_etnf *etnf;
    struct nrg_etn2 *etn2;
    gdrom_disc_t disc;
    gdrom_image_t image;
    gboolean end = FALSE;
    uint32_t chunk_id;
    int session_id = 0;
    int session_track_id = 0;
    int track_id = 0;
    int cue_track_id = 0, cue_track_count = 0;
    int i, count;

    fseek( f, -12, SEEK_END );
    fread( &footer, sizeof(footer), 1, f );
    if( GUINT32_FROM_BE(footer.v50.id) == NERO_V50_ID ) {
	INFO( "Loading Nero 5.0 image" );
	fseek( f, GUINT32_FROM_BE(footer.v50.offset), SEEK_SET );
    } else if( GUINT32_FROM_BE(footer.v55.id) == NERO_V55_ID ) {
	INFO( "Loading Nero 5.5+ image" );
	fseek( f, (uint32_t)GUINT64_FROM_BE(footer.v55.offset), SEEK_SET );
    } else {
	/* Not a (recognized) Nero image */
	return NULL;
    }
    
    disc = gdrom_image_new(filename, f);
    if( disc == NULL ) {
	ERROR("Unable to allocate memory!");
	return NULL;
    }
    image = (gdrom_image_t)disc;

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
		    lba = GUINT32_FROM_BE( cue->addr ) + GDROM_PREGAP;
		} else {
		    lba = msf_to_lba( cue->addr );
		}
		if( cue->track == 0 )
		    continue; /* Track 0. Leadin? always 0? */
		if( cue->track == 0xAA ) { /* end of disc */
		    image->track[track_id-1].sector_count =
			lba - image->track[track_id-1].lba;
		} else {
		    track = cue_track_id + bcd_to_uint8(cue->track) - 1;
		    if( (cue->control & 0x01) == 0 ) { 
			/* Pre-gap address. */
			if( track != 0 ) {
			    image->track[track-1].sector_count = 
				lba - image->track[track-1].lba;
			}
		    } else { /* Track-start address */
			image->track[track].lba = lba;
			image->track[track].flags = cue->type;
		    }
		}
	    }
	    break;
	case DAOI_ID:
	    dao = (struct nrg_daoi *)data;
	    memcpy( image->mcn, dao->mcn, 13 );
	    image->mcn[13] = '\0';
	    assert( dao->track_count * 30 + 22 == chunk.length );
	    assert( dao->track_count == cue_track_count );
	    for( i=0; i<dao->track_count; i++ ) {
		image->track[cue_track_id].sector_size = GUINT32_FROM_BE(dao->track[i].sector_size);
		image->track[cue_track_id].offset = GUINT32_FROM_BE(dao->track[i].offset);
		image->track[cue_track_id].mode = nrg_track_mode( dao->track[i].mode );
		image->track[cue_track_id].sector_count =
		    (GUINT32_FROM_BE(dao->track[i].end) - GUINT32_FROM_BE(dao->track[i].offset))/
		    GUINT32_FROM_BE(dao->track[i].sector_size);
		cue_track_id++;
	    }
	    break;
	case DAOX_ID:
	    daox = (struct nrg_daox *)data;
	    memcpy( image->mcn, daox->mcn, 13 );
	    image->mcn[13] = '\0';
	    assert( daox->track_count * 42 + 22 == chunk.length );
	    assert( daox->track_count == cue_track_count );
	    for( i=0; i<daox->track_count; i++ ) {
		image->track[cue_track_id].sector_size = GUINT32_FROM_BE(daox->track[i].sector_size);
		image->track[cue_track_id].offset = GUINT64_FROM_BE(daox->track[i].offset);
		image->track[cue_track_id].mode = nrg_track_mode( daox->track[i].mode );
		image->track[cue_track_id].sector_count =
		    (GUINT64_FROM_BE(daox->track[i].end) - GUINT64_FROM_BE(daox->track[i].offset))/
		    GUINT32_FROM_BE(daox->track[i].sector_size);
		cue_track_id++;
	    }
	    break;
	    
	case SINF_ID: 
	    /* Data is a single 32-bit number representing number of tracks in session */
	    i = GUINT32_FROM_BE( *(uint32_t *)data );
	    while( i-- > 0 )
		image->track[session_track_id++].session = session_id;
	    session_id++;
	    break;
	case ETNF_ID:
	    etnf = (struct nrg_etnf *)data;
	    count = chunk.length / sizeof(struct nrg_etnf);
	    for( i=0; i < count; i++, etnf++ ) {
		image->track[track_id].offset = GUINT32_FROM_BE(etnf->offset);
		image->track[track_id].lba = GUINT32_FROM_BE(etnf->lba) + (i+1)*GDROM_PREGAP;
		image->track[track_id].mode = nrg_track_mode( GUINT32_FROM_BE(etnf->mode) );
		if( image->track[track_id].mode == -1 ) {
		    gdrom_image_destroy_no_close(disc);
		    return NULL;
		}
		if( image->track[track_id].mode == GDROM_CDDA )
		    image->track[track_id].flags = 0x01;
		else
		    image->track[track_id].flags = 0x01 | TRACK_DATA;
		image->track[track_id].sector_size = GDROM_SECTOR_SIZE(image->track[track_id].mode);
		image->track[track_id].sector_count = GUINT32_FROM_BE(etnf->length) / 
		    image->track[track_id].sector_size;
		track_id++;
	    }
	    break;
	case ETN2_ID:
	    etn2 = (struct nrg_etn2 *)data;
	    count = chunk.length / sizeof(struct nrg_etn2);
	    for( i=0; i < count; i++, etn2++ ) {
		image->track[track_id].offset = (uint32_t)GUINT64_FROM_BE(etn2->offset);
		image->track[track_id].lba = GUINT32_FROM_BE(etn2->lba) + (i+1)*GDROM_PREGAP;
		image->track[track_id].mode = nrg_track_mode( GUINT32_FROM_BE(etn2->mode) );
		if( image->track[track_id].mode == -1 ) {
		    gdrom_image_destroy_no_close(disc);
		    return NULL;
		}
		if( image->track[track_id].mode == GDROM_CDDA )
		    image->track[track_id].flags = 0x01;
		else
		    image->track[track_id].flags = 0x01 | TRACK_DATA;
		image->track[track_id].sector_size = GDROM_SECTOR_SIZE(image->track[track_id].mode);
		image->track[track_id].sector_count = (uint32_t)(GUINT64_FROM_BE(etn2->length) / 
								 image->track[track_id].sector_size);
		track_id++;
	    }
	    break;

	case END_ID:
	    end = TRUE;
	    break;
	}
    } while( !end );
    image->track_count = track_id;
    return disc;
}



