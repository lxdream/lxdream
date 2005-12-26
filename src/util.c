/**
 * $Id: util.c,v 1.3 2005-12-26 03:54:52 nkeynes Exp $
 *
 * Miscellaneous utility functions.
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

#include "dream.h"

void fwrite_string( char *s, FILE *f )
{
    uint32_t len = 0;
    if( s == NULL ) {
	fwrite( &len, sizeof(len), 1, f );
    } else {
	len = strlen(s)+1;
	fwrite( &len, sizeof(len), 1, f );
	fwrite( s, len, 1, f );
    }
}

int fread_string( char *s, int maxlen, FILE *f ) 
{
    uint32_t len;
    fread( &len, sizeof(len), 1, f );
    if( len != 0 ) {
	fread( s, len > maxlen ? maxlen : len, 1, f );
    }
    return len;
}
