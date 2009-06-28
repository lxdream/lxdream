/**
 * $Id$
 *
 * VMU volume (ie block device) support
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

#include <glib/gmem.h>
#include <glib/gstrfuncs.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "vmu/vmuvol.h"
#include "dream.h"
#include "lxpaths.h"

#define VMU_MAX_PARTITIONS 256
#define VMU_MAX_BLOCKS 65536 /* Actually slightly less than this, but it'll do */

typedef struct vmu_partition {
    struct vmu_volume_metadata metadata;
    uint32_t block_count;
    char *blocks;
} *vmu_partition_t;

struct vmu_volume {
    const gchar *display_name;
    vmu_partnum_t part_count;
    gboolean dirty;
    struct vmu_partition part[0];
};

/* On-VMU structures, courtesy of Marcus Comstedt */ 
struct vmu_superblock {
    char magic[16];
    uint8_t colour_flag;
    uint8_t bgra[4];
    uint8_t pad1[27];
    char timestamp[8];
    char pad2[8];
    char unknown[6];
    uint16_t fat_block;
    uint16_t fat_size;
    uint16_t dir_block;
    uint16_t dir_size;
    uint16_t icon_shape;
    uint16_t user_size;
    /* remainder unknown */
};

struct vmu_direntry {
    uint8_t filetype;
    uint8_t copy_flag;
    uint16_t blkno;
    char filename[12];
    char timestamp[8];
    uint16_t blksize; /* Size in blocks*/
    uint16_t hdroff; /* Header offset in blocks */
    char pad[4];
};

#define MD(vmu,ptno) ((vmu)->part[ptno].metadata)

#define VMU_BLOCK(vmu,ptno,blkno) (&(vmu)->part[ptno].blocks[(blkno)*VMU_BLOCK_SIZE])

#define VMU_FAT_ENTRY(vmu,pt,ent)  ((uint16_t *)VMU_BLOCK(vmu, pt, (MD(vmu,pt).fat_block - ((ent)>>8))))[(ent)&0xFF]

#define FAT_EMPTY 0xFFFC
#define FAT_EOF   0xFFFA

static const struct vmu_volume_metadata default_metadata = { 255, 255, 254, 1, 253, 13, 0, 200, 31, 0, 128 };

vmu_volume_t vmu_volume_new_default( const gchar *display_name )
{
    vmu_volume_t vol = g_malloc0( sizeof(struct vmu_volume) + sizeof(struct vmu_partition) );
    vol->part_count = 1;
    vol->dirty = FALSE;
    memcpy( &vol->part[0].metadata, &default_metadata, sizeof(struct vmu_volume_metadata) );
    vol->part[0].block_count = VMU_DEFAULT_VOL_BLOCKS;
    vol->part[0].blocks = g_malloc0( VMU_DEFAULT_VOL_BLOCKS * VMU_BLOCK_SIZE );
    vol->display_name = display_name == NULL ? NULL : g_strdup(display_name);
    vmu_volume_format( vol, 0, TRUE );
    return vol;
}

void vmu_volume_destroy( vmu_volume_t vol )
{
    int i;
    if( vol == NULL )
        return;
    
    for( i=0; i<vol->part_count; i++ ) {
        g_free( vol->part[i].blocks );
        vol->part[i].blocks = NULL;
    }
    if( vol->display_name ) {
        g_free( (char *)vol->display_name );
        vol->display_name = NULL;
    }
    g_free(vol);
}

void vmu_volume_format( vmu_volume_t vol, vmu_partnum_t pt, gboolean quick )
{
    if( pt >= vol->part_count ) {
        return;
    }
    
    if( !quick ) {
        /* Wipe it completely first */
        memset( vol->part[pt].blocks, 0, (vol->part[pt].block_count) * VMU_BLOCK_SIZE );
    }
    
    struct vmu_volume_metadata *meta = &vol->part[pt].metadata;
    unsigned int fatblkno = meta->fat_block;
    unsigned int dirblkno = meta->dir_block;
    
    /* Write superblock */
    struct vmu_superblock *super = (struct vmu_superblock *)VMU_BLOCK(vol,pt, meta->super_block);
    memset( super->magic, 0x55, 16 );
    memset( &super->colour_flag, 0, 240 ); /* Blank the rest for now */
    super->fat_block = meta->fat_block;
    super->fat_size = meta->fat_size;
    super->dir_block = meta->dir_block;
    super->user_size = meta->user_size;
    
    /* Write file allocation tables */
    int i,j;
    for( j=0; j<meta->fat_size; j++ ) {
        uint16_t *fat = (uint16_t *)VMU_BLOCK(vol,pt,fatblkno-j); 
        for( i=0; i<256; i++ ) {
            fat[i] = FAT_EMPTY;
        }
    }
    
    /* Fill in the system allocations in the FAT */
    for( i=0; i<meta->fat_size-1; i++ ) {
        VMU_FAT_ENTRY(vol,pt,fatblkno-i) = fatblkno-i-1;
    }
    VMU_FAT_ENTRY(vol,pt,fatblkno - i) = FAT_EOF;
    for( i=0; i<meta->dir_size-1; i++ ) {
        VMU_FAT_ENTRY(vol,pt,dirblkno-i) = dirblkno-i-1;
    }
    VMU_FAT_ENTRY(vol,pt,dirblkno-i) = FAT_EOF;
  
    /* If quick-format, blank the directory. Otherwise it's already been done */
    if( quick ) {
        memset( VMU_BLOCK(vol,pt,dirblkno-meta->dir_size+1),
                0, meta->dir_size * VMU_BLOCK_SIZE );
    }
}

/*************************** File load/save ********************************/

/**
 * Current file has 1 META chunk for all volume metadata, followed by a
 * DATA chunk for each partition's block data. The META chunk is required to
 * occur before any DATA blocks.
 * Unknown chunks are skipped to allow for forwards compatibility if/when
 * we add the VMU runtime side of things
 */

struct vmu_file_header {
    char magic[16];
    uint32_t version;
    uint32_t head_len;
    uint32_t part_count;
    uint32_t display_name_len;
    char display_name[0];
};

struct vmu_chunk_header {
    char name[4];
    uint32_t length;
};


gboolean vmu_volume_save( const gchar *filename, vmu_volume_t vol, gboolean create_only )
{
    struct vmu_file_header head;
    struct vmu_chunk_header chunk;
    int i;

    gchar *tempfile = get_filename_at(filename, ".XXXXXXX");
    int fd = mkstemp( tempfile );
    if( fd == -1 ) {
        g_free(tempfile);
        return FALSE;
    }
    
    FILE *f = fdopen( fd, "w+" );
    
    
    /* File header */
    memcpy( head.magic, VMU_FILE_MAGIC, 16 );
    head.version = VMU_FILE_VERSION;
    head.part_count = vol->part_count;
    head.display_name_len = vol->display_name == NULL ? 0 : (strlen(vol->display_name)+1);
    head.head_len = sizeof(head) + head.display_name_len;
    fwrite( &head, sizeof(head), 1, f );
    if( vol->display_name != NULL ) {
        fwrite( vol->display_name, head.display_name_len, 1, f );
    }
    
    /* METAdata chunk */
    memcpy( chunk.name, "META", 4 );
    chunk.length = sizeof(struct vmu_volume_metadata) * vol->part_count;
    fwrite( &chunk, sizeof(chunk), 1, f );
    for( i=0; i < vol->part_count; i++ ) {
        fwrite( &vol->part[i].metadata, sizeof(struct vmu_volume_metadata), 1, f );
    }
    
    /* partition DATA chunks */
    for( i=0; i< vol->part_count; i++ ) {
        memcpy( chunk.name, "DATA", 4 );
        chunk.length = 0;
        if( fwrite( &chunk, sizeof(chunk), 1, f ) != 1 ) goto cleanup;
        long posn = ftell(f);
        if( fwrite( &vol->part[i].block_count, sizeof(vol->part[i].block_count), 1, f ) != 1 ) goto cleanup;
        fwrite_gzip( vol->part[i].blocks, vol->part[i].block_count, VMU_BLOCK_SIZE, f );
        long end = ftell(f);
        fseek( f, posn - sizeof(chunk.length), SEEK_SET );
        chunk.length = end-posn;
        if( fwrite( &chunk.length, sizeof(chunk.length), 1, f ) != 1 ) goto cleanup;
        fseek( f, end, SEEK_SET );
    }
    fclose(f);
    f = NULL;
    
    if( rename(tempfile, filename) != 0 )
        goto cleanup;
    
    /* All good */
    vol->dirty = FALSE;
    g_free(tempfile);
    return TRUE;
    
cleanup:
    if( f != NULL )
        fclose(f);
    unlink(tempfile);
    g_free(tempfile);
    return FALSE;
}

vmu_volume_t vmu_volume_load( const gchar *filename )
{
    struct vmu_file_header head;
    struct vmu_chunk_header chunk;
    vmu_volume_t vol;
    int i;
    
    FILE *f = fopen( filename, "ro" );
    if( f == NULL ) {
        ERROR( "Unable to open VMU file '%s': %s", filename, strerror(errno) );
        return FALSE;
    }
    
    if( fread( &head, sizeof(head), 1, f ) != 1 ||
        memcmp(head.magic, VMU_FILE_MAGIC, 16) != 0 ||
        head.part_count > VMU_MAX_PARTITIONS || 
        head.head_len < (sizeof(head) + head.display_name_len) )  {
        fclose(f);
        ERROR( "Unable to load VMU '%s': bad file header", filename );
        return NULL;
    }
    
    vol = (vmu_volume_t)g_malloc0( sizeof(struct vmu_volume) + sizeof(struct vmu_partition)*head.part_count );
    vol->part_count = head.part_count;
    vol->dirty = FALSE;
    if( head.display_name_len != 0 ) {
        vol->display_name = g_malloc( head.display_name_len );
        fread( (char *)vol->display_name, head.display_name_len, 1, f );
    }
    fseek( f, head.head_len, SEEK_SET );
    
    gboolean have_meta = FALSE;
    int next_part = 0;
    while( !feof(f) && fread( &chunk, sizeof(chunk), 1, f ) == 1 ) {
        if( memcmp( &chunk.name, "META", 4 ) == 0 ) {
            if( have_meta || chunk.length != head.part_count * sizeof(struct vmu_volume_metadata) ) {
                vmu_volume_destroy(vol);
                fclose(f);
                ERROR( "Unable to load VMU '%s': bad metadata size (expected %d but was %d)", filename,
                       head.part_count * sizeof(struct vmu_volume_metadata), chunk.length );
                return NULL;
            }
            for( i=0; i<head.part_count; i++ ) {
                fread( &vol->part[i].metadata, sizeof(struct vmu_volume_metadata), 1, f );
            }
            have_meta = TRUE;
        } else if( memcmp( &chunk.name, "DATA", 4 ) == 0 ) {
            uint32_t block_count;
            fread( &block_count, sizeof(block_count), 1, f );
            if( next_part >= vol->part_count || block_count >= VMU_MAX_BLOCKS ) {
                // Too many partitions / blocks
                vmu_volume_destroy(vol);
                fclose(f);
                ERROR( "Unable to load VMU '%s': too large (%d/%d)", filename, next_part, block_count );
                return NULL;
            }
            vol->part[next_part].block_count = block_count;
            vol->part[next_part].blocks = g_malloc(block_count*VMU_BLOCK_SIZE);
            fread_gzip(vol->part[next_part].blocks, VMU_BLOCK_SIZE, block_count, f );
            next_part++;
        } else {
            // else skip unknown block
            fseek( f, SEEK_CUR, chunk.length );
            WARN( "Unexpected VMU data chunk: '%4.4s'", chunk.name );
        }
    }
    
    fclose(f);
    
    if( !have_meta || next_part != vol->part_count ) {
        vmu_volume_destroy( vol );
        return NULL;
    }
    
    return vol;
}

/*************************** Accessing data ********************************/
const char *vmu_volume_get_display_name( vmu_volume_t vol ) 
{
    return vol->display_name;
}

void vmu_volume_set_display_name( vmu_volume_t vol, const gchar *name )
{
    if( vol->display_name != NULL ) {
        g_free( (char *)vol->display_name );
    }
    if( name == NULL ) {
        vol->display_name = NULL;
    } else {
        vol->display_name = g_strdup(name);
    }
}
    
gboolean vmu_volume_is_dirty( vmu_volume_t vol )
{
    return vol->dirty;
}

gboolean vmu_volume_read_block( vmu_volume_t vol, vmu_partnum_t pt, unsigned int block, unsigned char *out )
{
    if( pt >= vol->part_count || block >= vol->part[pt].block_count ) {
        return FALSE;
    }

    memcpy( out, VMU_BLOCK(vol,pt,block), VMU_BLOCK_SIZE );
    return TRUE;
}

gboolean vmu_volume_write_block( vmu_volume_t vol, vmu_partnum_t pt, unsigned int block, unsigned char *in )
{
    if( pt >= vol->part_count || block >= vol->part[pt].block_count ) {
        return FALSE;
    }
    memcpy( VMU_BLOCK(vol,pt,block), in, VMU_BLOCK_SIZE );
    vol->dirty = TRUE;
}

gboolean vmu_volume_write_phase( vmu_volume_t vol, vmu_partnum_t pt, unsigned int block, unsigned int phase, unsigned char *in )
{
    if( pt >= vol->part_count || block >= vol->part[pt].block_count || phase >= 4 ) {
        return FALSE;
    }
    memcpy( VMU_BLOCK(vol,pt,block) + (phase*128), in, VMU_BLOCK_SIZE/4 );
    vol->dirty = TRUE;
}

const struct vmu_volume_metadata *vmu_volume_get_metadata( vmu_volume_t vol, vmu_partnum_t partition )
{
    if( partition >= vol->part_count ) {
        return NULL;
    } else {
        return &vol->part[partition].metadata;
    }
}
