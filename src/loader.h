/**
 * $Id: loader.h,v 1.1 2005-12-24 08:02:14 nkeynes Exp $
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

#ifndef dreamcast_loader_H
#define dreamcast_loader_H 1

#include <stdio.h>
#include <glib/gtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * NULL-terminated list of file extension/name pairs,
 * supported by the loader 
 */
extern char *file_loader_extensions[][2];

/**
 * Load the CD bootstrap, aka IP.BIN. Identified by "SEGA SEGAKATANA" at
 * start of file. IP.BIN is loaded as-is at 8C008000.
 * This is mainly for testing as it's unlikely anyone would want to do this
 * for any other reason.
 * @return TRUE on success, otherwise FALSE and errno 
 */
gboolean file_load_bootstrap( gchar *filename );

/**
 * Load a miscellaneous .bin file, as commonly used in demos. No magic
 * applies, file is loaded as is at 8C010000
 */
gboolean file_load_binary( gchar *filename );

/**
 * Load a "Self Boot Inducer" .sbi file, also commonly used to package
 * demos. (Actually a ZIP file with a predefined structure
 */
gboolean file_load_sbi( gchar *filename );

/**
 * Load an ELF executable binary file. Origin is file-dependent.
 */
gboolean file_load_elf( gchar *filename );

/**
 * Load any of the above file types, using the appropriate magic to determine
 * which is actually applicable
 */
gboolean file_load_magic( gchar *filename );

#ifdef __cplusplus
}
#endif

#endif /* !dream_loader_H */

