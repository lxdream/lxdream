/**
 * $Id: util.c,v 1.6 2006-09-12 08:36:09 nkeynes Exp $
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

void fwrite_dump( unsigned char *data, unsigned int length, FILE *f ) 
{
    unsigned int i, j;
    for( i =0; i<length; i+=16 ) {
	fprintf( f, "%08X:", i);
	for( j=i; j<i+16; j++ ) {
	    if( (j % 4) == 0 )
		fprintf( f, " " );
	    if( j < length )
		fprintf( f, " %02X", (unsigned int)(data[j]) );
	    else
		fprintf( f, "   " );
	}
	fprintf( f, "  " );
	for( j=i; j<i+16 && j<length; j++ ) {
	    fprintf( f, "%c", isprint(data[j]) ? data[j] : '.' );
	}
	fprintf( f, "\n" );
    }
}

void fwrite_dump32( unsigned int *data, unsigned int length, FILE *f ) 
{
    fwrite_dump32v( data, length, 8, f );
}

void fwrite_dump32v( unsigned int *data, unsigned int length, int wordsPerLine, FILE *f ) 
{
    unsigned int i, j;
    for( i =0; i<length>>2; i+=wordsPerLine ) {
	fprintf( f, "%08X:", i);
	for( j=i; j<i+wordsPerLine; j++ ) {
	    if( j < length )
		fprintf( f, " %08X", (unsigned int)(data[j]) );
	    else
		fprintf( f, "         " );
	}
	fprintf( f, "\n" );
    }
}
