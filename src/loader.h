/**
 * $Id$
 *
 * Interface declarations for the binary loader routines (loader.c, elf.c)
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

#ifndef lxdream_loader_H
#define lxdream_loader_H 1

#include <stdio.h>
#include <glib.h>

#include "drivers/cdrom/cdrom.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * NULL-terminated list of file extension/name pairs,
 * supported by the loader 
 */
extern char *file_loader_extensions[][2];

typedef enum {
    FILE_ERROR,
    FILE_BINARY,
    FILE_ELF,
    FILE_ISO,
    FILE_DISC,
    FILE_ZIP,
    FILE_SAVE_STATE,
    FILE_UNKNOWN,
} lxdream_file_type_t;

/**
 * Attempt to identify the given file as one of the above file types
 */
lxdream_file_type_t file_identify( const gchar *filename, int fd, ERROR *err );

/**
 * Load any supported file, and return the type of file loaded.
 * If the file is a disc, the disc is mounted. 
 * 
 * @param filename The file to load
 * @param wrap_exec If true, load executables as disc images. Otherwise load 
 *    directly into RAM
 * @param err Updated with error message on failure.
 */
lxdream_file_type_t file_load_magic( const gchar *filename, gboolean wrap_exec, ERROR *err );

/**
 * Load an ELF or .bin executable file based on magic.
 */
gboolean file_load_exec( const gchar *filename, ERROR *err );

cdrom_disc_t cdrom_wrap_magic( cdrom_disc_type_t type, const gchar *filename, ERROR *err );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_loader_H */

