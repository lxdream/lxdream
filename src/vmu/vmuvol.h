/**
 * $Id$
 *
 * VMU volume (ie block device) declarations
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

#ifndef lxdream_vmuvol_H
#define lxdream_vmuvol_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "lxdream.h"

typedef struct vmu_volume *vmu_volume_t;
typedef unsigned int vmu_partnum_t;

#define VMU_FILE_MAGIC "%!Dreamcast$VMU\0"
#define VMU_FILE_VERSION 0x00010000

/** VMU block size is 512 bytes */
#define VMU_BLOCK_SIZE 512

/** Default VMU volume is 256 blocks */
#define VMU_DEFAULT_VOL_BLOCKS 256

/** Default VMU has 200 user blocks */
#define VMU_DEFAULT_VOL_USERBLOCKS 200

/** Default superblock is at block 255 */
#define VMU_DEFAULT_VOL_SUPERBLOCK 255

/** Default file allocation table is at block 254 */
#define VMU_DEFAULT_VOL_FATBLOCK 254

/** Default root directory block starts at block 253 */
#define VMU_DEFAULT_VOL_ROOTDIR 253

/**
 * VMU metadata structure. Note that this is structured to match the maple 
 * memory info result packet.
 */
struct vmu_volume_metadata {
    uint32_t last_block;
    uint16_t super_block;
    uint16_t fat_block;
    uint16_t fat_size;
    uint16_t dir_block;
    uint16_t dir_size;
    uint16_t user_block; /* ?? */
    uint16_t user_size;
    uint16_t unknown[3]; /* 0x001F, 0x0000, 0x0080 */
};

/**
 * Construct a new VMU volume with a single partition of the default size 
 * (128Kb). The partitions is formatted using the default filesystem 
 * organization. 
 */
vmu_volume_t vmu_volume_new_default( const gchar *display_name );

void vmu_volume_destroy( vmu_volume_t vol );

void vmu_volume_set_display_name( vmu_volume_t vol, const gchar *display_name );

const gchar *vmu_volume_get_display_name( vmu_volume_t vol );

gboolean vmu_volume_is_dirty( vmu_volume_t vol );

/**
 * Format a VMU partition according to the standard layout
 */
void vmu_volume_format( vmu_volume_t vol, vmu_partnum_t partition, gboolean quick );



/**
 * Load a VMU volume from a file.
 */
vmu_volume_t vmu_volume_load( const gchar *filename );

/**
 * Save a VMU volume to a file.
 */
gboolean vmu_volume_save( const gchar *filename, vmu_volume_t vol, gboolean create_only ); 

gboolean vmu_volume_read_block( vmu_volume_t vol, vmu_partnum_t partition, unsigned int block, unsigned char *out );
gboolean vmu_volume_write_block( vmu_volume_t vol, vmu_partnum_t partition, unsigned int block, unsigned char *in );
gboolean vmu_volume_write_phase( vmu_volume_t vol, vmu_partnum_t partition, unsigned int block, 
                                 unsigned int phase, unsigned char *in );

/**
 * Retrieve the metadata for the given volume. The metadata is owned by the
 * volume and should not be modified or freed.
 */
const struct vmu_volume_metadata *vmu_volume_get_metadata( vmu_volume_t vol, vmu_partnum_t partition );


#ifdef __cplusplus
}
#endif

#endif /* !lxdream_vmuvol_H */
