/**
 * $Id$
 *
 * Unit tests for the ISO9660 filesystem reader
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
#include <stdio.h>

int main( int argc, char *argv[] )
{
    if( argc < 2 ) {
        fprintf( stderr, "Usage: testisoread <disc image>\n" );
        return 1;
    }

    ERROR err;
    cdrom_disc_t disc = cdrom_disc_open(argv[1], &err);

    if( disc == NULL ) {
        fprintf( stderr, "Unable to open disc image '%s': %s\n", argv[1], err.msg );
        return 2;
    }
    cdrom_track_t track = cdrom_disc_get_last_data_track(disc);
    if( track == NULL ) {
        fprintf( stderr, "Disc has no data tracks\n" );
        return 3;
    }

    isofs_reader_t iso = isofs_reader_new_from_track( disc, track, &err );
    if( iso == NULL ) {
        fprintf( stderr, "Unable to open ISO filesystem: %s\n", err.msg );
        return 4;
    }
    isofs_reader_print_dir( stdout, isofs_reader_get_root_dir(iso) );

    isofs_reader_dirent_t boot = isofs_reader_get_file( iso, "1st_read.bin" );
    if( boot == NULL ) {
        fprintf( stderr, "Unable to find 1st_read.bin" );
        return 5;
    }

    printf( "Bootstrap: %s (%d)\n", boot->name, boot->size );
    char tmp[boot->size];
    if( isofs_reader_read_file( iso, boot, 0, boot->size, tmp ) != CDROM_ERROR_OK ) {
        fprintf( stderr, "Unable to read 1st_read.bin" );
        return 6;
    }
    return 0;
}
