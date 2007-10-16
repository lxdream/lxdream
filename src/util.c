/**
 * $Id: util.c,v 1.10 2007-10-16 12:36:29 nkeynes Exp $
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

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "dream.h"
#include "sh4/sh4core.h"

char *msg_levels[] = { "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE" };
int global_msg_level = EMIT_WARN;

void fwrite_string( const char *s, FILE *f )
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


void log_message( void *ptr, int level, const gchar *source, const char *msg, ... )
{
    char buf[20], addr[10] = "", *p;
    const gchar *arr[4] = {buf, source, addr};
    int posn;
    time_t tm = time(NULL);
    va_list ap;

    if( level > global_msg_level ) {
	return; // ignored
    }

    va_start(ap, msg);

    if( level <= EMIT_ERR ) {
	gchar *text = g_strdup_vprintf( msg, ap );
	if( gui_error_dialog( text ) ) {
	    g_free(text);
	    va_end(ap);
	    return;
	}
	g_free(text);
    }


    strftime( buf, sizeof(buf), "%H:%M:%S", localtime(&tm) );
    fprintf( stderr, "%s %08X %-5s ", buf, sh4r.pc, msg_levels[level] );
    vfprintf( stderr, msg, ap );
    va_end(ap);
    fprintf( stderr, "\n" );
}
