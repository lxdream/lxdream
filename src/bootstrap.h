/**
 * $Id$
 *
 * CD Bootstrap header parsing. Mostly for informational purposes.
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

/*
 * IP.BIN related code. Ref: http://mc.pp.se/dc/ip0000.bin.html
 */
#ifndef lxdream_bootstrap_H
#define lxdream_bootstrap_H 1

#include "lxdream.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOOTSTRAP_LOAD_ADDR 0x8C008000
#define BOOTSTRAP_ENTRY_ADDR 0x8c008300
#define BOOTSTRAP_SIZE 32768
#define BOOTSTRAP_MAGIC "SEGA SEGAKATANA SEGA ENTERPRISES"
#define BOOTSTRAP_MAGIC_SIZE 32

#define BINARY_LOAD_ADDR 0x8C010000

/**
 * Bootstrap header structure
 */
typedef struct dc_bootstrap_head {
    char magic[32];
    char crc[4];
    char padding;         /* normally ascii space */
    char gdrom_id[6];
    char disc_no[5];
    char regions[8];
    char peripherals[8];
    char product_id[10];
    char product_ver[6];
    char product_date[16];
    char boot_file[16];
    char vendor_id[16];
    char product_name[128];
} *dc_bootstrap_head_t;

/**
 * Dump the bootstrap info to the output log for infomational/debugging
 * purposes.
 */
void bootstrap_dump(void *data, gboolean detail);

void bootprogram_scramble( unsigned char *dest, unsigned char *src, size_t length );
void bootprogram_unscramble( unsigned char *dest, unsigned char *src, size_t length );
#ifdef __cplusplus
}
#endif

#endif /* !lxdream_bootstrap_H */
