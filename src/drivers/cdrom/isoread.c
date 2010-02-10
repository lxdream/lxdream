/**
 * $Id$
 *
 * ISO9660 filesystem reading support
 *
 * Copyright (c) 2010 Nathan Keynes.
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

#include "drivers/cdrom/cdrom.h"
#include "drivers/cdrom/isoread.h"
#include "drivers/cdrom/iso_impl.h"

#include <string.h>
#include <errno.h>

static char isofs_magic[5] = { 'C', 'D', '0', '0', '1' };

#define ISO_DIR_TAG 0x52494449

struct isofs_reader_dir {
    uint32_t tag;
    isofs_reader_dir_t parent;
    size_t num_entries;
    struct isofs_reader_dirent entries[];
};

struct isofs_reader {
    /**
     * Base sector source to read the filesystem from (must support Mode 1 reads)
     */
    sector_source_t source;

    /**
     * Offset of the source relative to the start of the (notional) disc -
     * this is subtracted from all source lba addresses.
     */
    cdrom_lba_t source_offset;

    /**
     * Start of the ISO9660 filesystem relative to the start of the disc.
     * (The actual superblock is at fs_start+16)
     */
    cdrom_lba_t fs_start;

    /**
     * If TRUE, read the little-endian side of the FS, otherwise the big-endian
     * side. (They should normally give the same result, but in case it matters...)
     */
    gboolean little_endian;

    /**
     * The volume sequence number, for multi-volume sets.
     */
    uint16_t volume_seq_no;

    char volume_label[33];

    /**
     * Filesystem root directory
     */
    isofs_reader_dir_t root_dir;
};

/**
 * Read a 16-bit dual-endian field using the defined endianness of the reader
 */
#define ISO_GET_DE16( iso, field ) \
    ( ((iso)->little_endian) ? GINT16_FROM_LE(field) : GINT16_FROM_BE(*((&field)+1)) )

/**
 * Read a 32-bit dual-endian field using the defined endianness of the reader
 */
#define ISO_GET_DE32( iso, field ) \
    ( ((iso)->little_endian) ? GINT32_FROM_LE(field) : GINT32_FROM_BE(*((&field)+1)) )


static void isofs_reader_convert_dirent( isofs_reader_t iso, isofs_reader_dirent_t dest, iso_dirent_t src,
                                         char **strp )
{
    dest->start_lba = ISO_GET_DE32(iso, src->file_lba_le);
    dest->size = ISO_GET_DE32(iso, src->file_size_le);
    dest->is_dir = (src->flags & ISO_FILE_DIR) ? TRUE : FALSE;
    dest->interleave_gap = src->gap_size;
    dest->interleave_size = src->unit_size;
    dest->name = *strp;
    memcpy( *strp, src->file_id, src->file_id_len );
    (*strp)[src->file_id_len] = '\0';
    *strp += src->file_id_len + 1;
}

/**
 * Read a directory from the disc into memory.
 */
isofs_reader_dir_t isofs_reader_read_dir( isofs_reader_t iso, cdrom_lba_t lba, size_t size )
{
    cdrom_count_t count = (size+2047)/2048;

    char buf[count*2048];

    if( isofs_reader_read_sectors( iso, lba, count, buf ) != CDROM_ERROR_OK )
        return NULL;

    size_t len = 0;
    unsigned num_entries = 0, i=0, offset=0;
    /* Compute number of entries and total string length */
    while( offset < size ) {
        struct iso_dirent *p = (struct iso_dirent *)&buf[offset];
        offset += p->record_len;
        if( offset > size || p->record_len < sizeof(struct iso_dirent) )
            break; // Bad record length
        if( p->file_id_len + sizeof(struct iso_dirent)-1 > p->record_len )
            break; // Bad fileid length
        if( p->file_id_len == 1 && (p->file_id[0] == 0 || p->file_id[0] == 1 ) )
            continue; /* self and parent-dir references */
        num_entries++;
        len += p->file_id_len + 1;
    }

    size_t table_len = num_entries * sizeof(struct isofs_reader_dirent);
    isofs_reader_dir_t dir = g_malloc0( sizeof(struct isofs_reader_dir) + table_len + len );
    dir->tag = ISO_DIR_TAG;
    dir->num_entries = num_entries;

    char *strp = (char *)&dir->entries[num_entries];
    offset = 0;
    for( i=0; i < num_entries; i++ ) {
        struct iso_dirent *p;
        do {
            p = (struct iso_dirent *)&buf[offset];
            offset += p->record_len;
            /* Skip over self and parent-dir references */
        } while( p->file_id_len == 1 && (p->file_id[0] == 0 || p->file_id[0] == 1 ) );

        isofs_reader_convert_dirent( iso, &dir->entries[i], p, &strp );

    }
    return dir;
}

static gboolean isofs_reader_dirent_match_exact( const char *file, const char *find )
{
    return strcasecmp( file, find ) == 0;
}

static gboolean isofs_reader_dirent_match_unversioned( const char *file, const char *find )
{
    char *semi = strchr(file, ';');
    if( semi == NULL ) {
        /* Unversioned ISO file */
        return strcasecmp( file, find ) == 0;
    } else {
        int len = semi - file;
        return strncasecmp( file, find, len ) == 0 && strlen(find) == len;
    }
}

/**
 * Search a directory for a given filename. If found, return the corresponding
 * dirent structure, otherwise NULL. Comparison is case-insensitive, and returns
 * the most recent (highest numbered) version of a file in case of multiple
 * versions unless the requested component is also explicitly versioned.
 *
 * For now just do a linear search, although we could do a binary search given
 * that the directory should be sorted.
 */
static isofs_reader_dirent_t isofs_reader_get_file_component( isofs_reader_dir_t dir, const char *component )
{

    if( strchr( component, ';' ) != NULL ) {
        for( unsigned i=0; i<dir->num_entries; i++ ) {
            if( isofs_reader_dirent_match_exact(dir->entries[i].name,component) ) {
                return &dir->entries[i];
            }
        }
    } else {
        for( unsigned i=0; i<dir->num_entries; i++ ) {
            if( isofs_reader_dirent_match_unversioned(dir->entries[i].name,component) ) {
                return &dir->entries[i];
            }
        }
    }
    return NULL;
}

isofs_reader_dirent_t isofs_reader_get_file( isofs_reader_t iso, const char *pathname )
{
    int pathlen = strlen(pathname);
    char tmp[pathlen+1];
    char *p = tmp;
    isofs_reader_dir_t dir = iso->root_dir;

    memcpy( tmp, pathname, pathlen+1 );
    char *q = strchr(p, '/');
    while( q != NULL ) {
        *q = '\0';
        isofs_reader_dirent_t ent = isofs_reader_get_file_component( dir, p );
        if( ent == NULL || !ent->is_dir ) {
            return NULL;
        }
        if( ent->subdir == NULL ) {
            ent->subdir = dir = isofs_reader_read_dir( iso, ent->start_lba, ent->size );
            if( dir == NULL ) {
                return NULL;
            }
        }

        p = q+1;
        q = strchr(p, '/');

    }
    return isofs_reader_get_file_component( dir, p );
}

cdrom_error_t isofs_reader_read_file( isofs_reader_t iso, isofs_reader_dirent_t file,
                                      size_t offset, size_t byte_count, unsigned char *buf )
{
    char tmp[2048];

    if( offset + byte_count > file->size )
        return CDROM_ERROR_BADREAD;

    if( file->interleave_gap == 0 ) {
        cdrom_lba_t lba = file->start_lba + (offset>>11);
        lba += ((file->xa_size+2047)>>11); /* Skip XA record if present */

        if( (offset & 2047) != 0 ) {
            /* Read an unaligned start block */
            cdrom_error_t status = isofs_reader_read_sectors( iso, lba, 1, tmp );
            if( status != CDROM_ERROR_OK )
                return status;
            unsigned align = offset & 2047;
            size_t length = 2048 - align;
            if( length >= byte_count ) {
                memcpy( buf, &tmp[align], byte_count );
                return CDROM_ERROR_OK;
            } else {
                memcpy( buf, &tmp[align], length );
                byte_count -= length;
                buf += length;
                lba++;
            }
        }
        /* Read the bulk of the data */
        cdrom_count_t sector_count = byte_count >> 11;
        if( sector_count > 0 ) {
            cdrom_error_t status = isofs_reader_read_sectors( iso, lba, sector_count, buf );
            if( status != CDROM_ERROR_OK )
                return status;
            buf += (sector_count << 11);
            lba += sector_count;
        }
        /* Finally read a partial final block */
        if( (byte_count & 2047) != 0 ) {
            cdrom_error_t status = isofs_reader_read_sectors( iso, lba, 1, tmp );
            if( status != CDROM_ERROR_OK )
                return status;
            memcpy( buf, tmp, byte_count & 2047 );
        }
        return CDROM_ERROR_OK;
    } else {
        // ERROR("Interleaved files not supported");
        return CDROM_ERROR_BADREAD;
    }
}

void isofs_reader_destroy_dir( isofs_reader_dir_t dir )
{
    dir->tag = 0;
    for( unsigned i=0; i<dir->num_entries; i++ ) {
        if( dir->entries[i].subdir != NULL ) {
            isofs_reader_dir_t subdir = dir->entries[i].subdir;
            dir->entries[i].subdir = NULL;
            isofs_reader_destroy_dir(dir);
        }
    }
    g_free(dir);
}

cdrom_error_t isofs_reader_read_sectors( isofs_reader_t iso, cdrom_lba_t lba, cdrom_count_t count,
                                         unsigned char *buf )
{
    if( lba < iso->source_offset )
        return CDROM_ERROR_BADREAD;
    return sector_source_read_sectors( iso->source, lba - iso->source_offset, count,
           CDROM_READ_MODE2_FORM1|CDROM_READ_DATA, buf, NULL );
}


isofs_reader_t isofs_reader_new( sector_source_t source, cdrom_lba_t offset, cdrom_lba_t start, ERROR *err )
{
    char buf[2048];
    iso_pvd_t pvd = (iso_pvd_t)&buf;
    unsigned i = 0;

    isofs_reader_t iso = g_malloc0( sizeof(struct isofs_reader) );
    if( iso == NULL ) {
        SET_ERROR( err, ENOMEM, "Unable to allocate memory" );
        return NULL;
    }
    iso->source = source;
    iso->source_offset = offset;
    iso->fs_start = start;
    iso->little_endian = TRUE;

    do {
        /* Find the primary volume descriptor */
        cdrom_error_t status = isofs_reader_read_sectors( iso, iso->fs_start + ISO_SUPERBLOCK_OFFSET + i, 1, buf );
        if( status != CDROM_ERROR_OK ) {
            SET_ERROR( err, EBADF, "Unable to read superblock from ISO9660 filesystem" );
            g_free(iso);
            return NULL;
        }
        if( memcmp(pvd->tag, isofs_magic, 5) != 0 || /* Not an ISO volume descriptor */
                pvd->desc_type == ISO_TERMINAL_DESCRIPTOR ) { /* Reached the end of the descriptor list */
            SET_ERROR( err, EINVAL, "ISO9660 filesystem not found" );
            g_free(iso);
            return NULL;
        }
        i++;
    } while( pvd->desc_type != ISO_PRIMARY_DESCRIPTOR );

    if( pvd->desc_version != 1 ) {
        SET_ERROR( err, EINVAL, "Incompatible ISO9660 filesystem" );
        g_free(iso);
        return NULL;
    }

    iso->volume_seq_no = ISO_GET_DE16(iso, pvd->volume_seq_le);
    memcpy( iso->volume_label, pvd->volume_id, 32 );
    for( i=32; i>0 && iso->volume_label[i-1] == ' '; i-- );
    iso->volume_label[i] = '\0';

    iso->root_dir = isofs_reader_read_dir( iso,
            ISO_GET_DE32(iso, pvd->root_dirent.file_lba_le),
            ISO_GET_DE32(iso, pvd->root_dirent.file_size_le) );
    if( iso->root_dir == NULL ) {
        SET_ERROR( err, EINVAL, "Unable to read root directory from ISO9660 filesystem" );
        g_free(iso);
        return NULL;
    }

    sector_source_ref( source );
    return iso;
}

isofs_reader_t isofs_reader_new_from_disc( cdrom_disc_t disc, cdrom_lba_t lba, ERROR *err )
{
    return isofs_reader_new( &disc->source, 0, lba, err );
}

isofs_reader_t isofs_reader_new_from_track( cdrom_disc_t disc, cdrom_track_t track, ERROR *err )
{
    return isofs_reader_new( &disc->source, 0, track->lba, err );
}

isofs_reader_t isofs_reader_new_from_source( sector_source_t source, ERROR *err )
{
    return isofs_reader_new( source, 0, 0, err );
}

void isofs_reader_destroy( isofs_reader_t iso )
{
    isofs_reader_destroy_dir( iso->root_dir );
    iso->root_dir = NULL;
    sector_source_unref( iso->source );
    iso->source = NULL;
    g_free( iso );
}

isofs_reader_dir_t isofs_reader_get_root_dir( isofs_reader_t iso )
{
    return iso->root_dir;
}

void isofs_reader_print_dir( FILE *f, isofs_reader_dir_t dir )
{
    fprintf( f, "Total %d files\n", dir->num_entries );
    for( unsigned i=0; i<dir->num_entries; i++ ) {
        fprintf( f, "%7d %s\n", dir->entries[i].size, dir->entries[i].name );
    }

}
