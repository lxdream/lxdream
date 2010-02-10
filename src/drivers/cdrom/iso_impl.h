/**
 * $Id$
 *
 * ISO9660 filesystem support
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

#define ISO_SUPERBLOCK_OFFSET 16
#define ISO_BOOT_DESCRIPTOR 0
#define ISO_PRIMARY_DESCRIPTOR 1
#define ISO_SECONDARY_DESCRIPTOR 2
#define ISO_PARTITION_DESCRIPTOR 3
#define ISO_TERMINAL_DESCRIPTOR 0xFF

typedef struct iso_timestamp_full { /* 17 bytes */
    char year[4];
    char month[2];
    char day[2];
    char hour[2];
    char minute[2];
    char second[2];
    char hundredths[2];
    int8_t utc_offset; /* 15 min intervals from -48 to +52 7.1.2 */
} *iso_timestamp_full_t;

typedef struct iso_timestamp { /* 7 bytes */
    uint8_t year; /* Since 1900 */
    uint8_t month; /* 1 .. 12 */
    uint8_t day; /* 1 ..  31 */
    uint8_t hour; /* 0 .. 23 */
    uint8_t minute; /* 0 .. 59 */
    uint8_t second; /* 0 .. 59 */
    uint8_t utc_offset; /* 15 min intervals from -48 to +52 7.1.2 */
} *iso_timestamp_t;

#define ISO_FILE_EXISTS   0x01
#define ISO_FILE_DIR      0x02
#define ISO_FILE_ASSOC    0x04 /* Associated file */
#define ISO_FILE_RECORD   0x08 /* Structure specified by XA record */
#define ISO_FILE_PROTECT  0x10 /* Permissions specified */
#define ISO_FILE_MULTIEXT 0x80 /* Multiple extents */

typedef struct iso_dirent { /* 34+ bytes 9.1 */
    uint8_t record_len;
    uint8_t xa_record_len;
    uint32_t file_lba_le, file_lba_be;
    uint32_t file_size_le, file_size_be;
    struct iso_timestamp timestamp;
    uint8_t flags;
    uint8_t unit_size;
    uint8_t gap_size;
    uint16_t volume_seq_le, volume_seq_be;
    uint8_t file_id_len;
    char file_id[1];
} __attribute__((packed)) *iso_dirent_t;

typedef struct iso_pathtabrec { /* 8+ bytes 9.4 */
    uint8_t record_len;
    uint8_t xa_record_len;
    uint32_t file_lba;
    uint16_t parent_dir_no;
    char file_id[];
} *iso_pathtabrec_t;

typedef struct iso_xattrrec {
    uint16_t uid_le, uid_be;
    uint16_t gid_le, gid_be;
    uint16_t permissions;
    struct iso_timestamp_full create_time;
    struct iso_timestamp_full modify_time;
    struct iso_timestamp_full expiry_time;
    struct iso_timestamp_full effective_time;
    uint8_t record_format;
    uint8_t record_attrs;
    uint16_t record_len_le, record_len_be;
    char system_id[32];
    char system_use[64];
    uint8_t record_version; /* Should be 1 */
    uint8_t escape_len;
    char reserved[64];
    uint16_t app_use_len_le, app_use_len_be;
    char app_use[];
} *iso_xattrrec_t;

/** Primary Volume Descriptor */
typedef struct iso_pvd {
    uint8_t desc_type;
    char tag[5];
    uint8_t desc_version;
    char pad0;
    char system_id[32];
    char volume_id[32];
    char pad1[8];
    uint32_t volume_size_le, volume_size_be;
    char pad2[32];
    uint16_t volume_sets_le, volume_sets_be;
    uint16_t volume_seq_le, volume_seq_be;
    uint16_t block_size_le, block_size_be;
    uint32_t pathtab_size_le, pathtab_size_be;
    uint32_t pathtab_offset_le, pathtab2_offset_le;
    uint32_t pathtab_offset_be, pathtab2_offset_be;
    struct iso_dirent root_dirent;
    char volume_set_id[128];
    char publisher_id[128];
    char preparer_id[128];
    char app_id[128];
    char copyright_file_id[37];
    char abstract_file_id[37];
    char biblio_file_id[37];
    struct iso_timestamp_full create_time;
    struct iso_timestamp_full modify_time;
    struct iso_timestamp_full expiry_time;
    struct iso_timestamp_full effective_time;
    uint8_t fs_version; /* must be 1 */
    char pad3[1166];
} *iso_pvd_t;

