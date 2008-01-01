/**
 * $Id: util.c,v 1.14 2007-11-08 11:54:16 nkeynes Exp $
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

#define HAVE_EXECINFO_H 1

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <zlib.h>
#include <glib.h>
#include <png.h>
#include "dream.h"
#include "display.h"
#include "gui.h"
#include "sh4/sh4core.h"

char *msg_levels[] = { "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE" };
int global_msg_level = EMIT_WARN;

static void report_crash( int signo, siginfo_t *info, void *ptr )
{
    char buf[128];

    fprintf( stderr, "--- Aborting with signal %d ---\n", signo );
    // Get gdb to print a nice backtrace for us
    snprintf( buf, 128, "gdb -batch -f --quiet --pid=%d -ex bt", getpid() );
    system(buf);

    abort();
}

void install_crash_handler(void)
{
    struct sigaction sa;

    sa.sa_sigaction = report_crash;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND|SA_SIGINFO;
    sigaction( SIGSEGV, &sa, NULL );
}


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

void fwrite_gzip( void *p, size_t sz, size_t count, FILE *f )
{
    uLongf size = sz*count;
    uLongf csize = ((int)(size*1.001))+13;
    unsigned char *tmp = g_malloc0( csize );
    int status = compress( tmp, &csize, p, size );
    assert( status == Z_OK );
    fwrite( &csize, sizeof(csize), 1, f );
    fwrite( tmp, csize, 1, f );
    g_free(tmp);
}

int fread_gzip( void *p, size_t sz, size_t count, FILE *f )
{
    uLongf size = sz*count;
    uLongf csize;
    unsigned char *tmp;

    fread( &csize, sizeof(csize), 1, f );
    assert( csize <= (size*2) );
    tmp = g_malloc0( csize );
    fread( tmp, csize, 1, f );
    int status = uncompress( p, &size, tmp, csize );
    g_free(tmp);
    if( status == Z_OK ) {
	return count;
    } else {
	fprintf( stderr, "Error reading compressed data\n" );
	return 0;
    }
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

gboolean write_png_to_stream( FILE *f, frame_buffer_t buffer )
{
    int coltype, i;
    png_bytep p;
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
	return FALSE;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
	png_destroy_write_struct(&png_ptr, NULL);
	return FALSE;
    }
    
    if( setjmp(png_jmpbuf(png_ptr)) ) {
	png_destroy_write_struct(&png_ptr, &info_ptr);
	return FALSE;
    }
    png_init_io( png_ptr, f );
    switch( buffer->colour_format ) {
    case COLFMT_BGR888:
	coltype = PNG_COLOR_TYPE_RGB;
	break;
    case COLFMT_BGRA8888:
	coltype = PNG_COLOR_TYPE_RGB_ALPHA;
	break;
    case COLFMT_BGR0888:
	coltype = PNG_COLOR_TYPE_RGB;
	break;
    default:
	coltype = PNG_COLOR_TYPE_RGB;
    }
    png_set_IHDR(png_ptr, info_ptr, buffer->width, buffer->height,
		 8, coltype, PNG_INTERLACE_NONE, 
		 PNG_COMPRESSION_TYPE_DEFAULT, 
		 PNG_FILTER_TYPE_DEFAULT );
    png_write_info(png_ptr, info_ptr);
    if( buffer->colour_format == COLFMT_BGR0888 ) {
	png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
    }
    png_set_bgr(png_ptr);
    if( buffer->inverted ) {
	p = (png_bytep)(buffer->data + (buffer->height*buffer->rowstride) - buffer->rowstride);
	for(i=0; i<buffer->height; i++ ) {
	    png_write_row(png_ptr, p);
	    p-=buffer->rowstride;
	}
    } else {
	p = (png_bytep)buffer->data;
	for(i=0; i<buffer->height; i++ ) {
	    png_write_row(png_ptr, p);
	    p+=buffer->rowstride;
	}
    }
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return TRUE;
}

frame_buffer_t read_png_from_stream( FILE *f )
{
    png_bytep p;
    int i;
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 
						 NULL, NULL, NULL);
    if (!png_ptr) {
	return NULL;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
	png_destroy_read_struct(&png_ptr, NULL, NULL);
	return NULL;
    }
    
    png_infop end_info = png_create_info_struct(png_ptr);
    if (!end_info) {
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL );
	return NULL;
    }

    if( setjmp(png_jmpbuf(png_ptr)) ) {
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	return NULL;
    }

    png_init_io(png_ptr, f);
    png_read_info(png_ptr, info_ptr);
    
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type,
	compression_type, filter_method;
    png_get_IHDR(png_ptr, info_ptr, &width, &height,
		 &bit_depth, &color_type, &interlace_type,
		 &compression_type, &filter_method);
    assert( interlace_type == PNG_INTERLACE_NONE );
    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    int channels = png_get_channels(png_ptr, info_ptr);
    frame_buffer_t buffer = g_malloc( sizeof(struct frame_buffer) + rowbytes*height );
    buffer->data = (unsigned char *)(buffer+1);
    buffer->width = width;
    buffer->height = height;
    buffer->rowstride = rowbytes;
    buffer->address = -1;
    buffer->size = rowbytes*height;
    buffer->inverted = FALSE;
    if( channels == 4 ) {
	buffer->colour_format = COLFMT_BGRA8888;
    } else if( channels == 3 ) {
	buffer->colour_format = COLFMT_RGB888;
    }
    
    p = (png_bytep)buffer->data;
    for( i=0; i<height; i++ ) {
	png_read_row(png_ptr, p, NULL );
	p += rowbytes;
    }

    png_read_end(png_ptr, end_info);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    return buffer;
}

int get_log_level_from_string( const gchar *str )
{
    switch( tolower(str[0]) ) {
    case 'd': return EMIT_DEBUG;
    case 'e': return EMIT_ERR;
    case 'f': return EMIT_FATAL;
    case 'i': return EMIT_INFO;
    case 't': return EMIT_TRACE;
    case 'w': return EMIT_WARN;
    default: return -1;
    }
}

gboolean set_global_log_level( const gchar *str ) 
{
    int l = get_log_level_from_string(str);
    if( l == -1 ) {
	return FALSE;
    } else {
	global_msg_level = l;
	return TRUE;
    }
}

void log_message( void *ptr, int level, const gchar *source, const char *msg, ... )
{
    char buf[20];
    time_t tm = time(NULL);
    va_list ap;

    if( level > global_msg_level ) {
	return; // ignored
    }

    va_start(ap, msg);
    gchar *text = g_strdup_vprintf( msg, ap );
    va_end(ap);
    
    if( level <= EMIT_ERR ) {
	if( gui_error_dialog( text ) ) {
	    g_free(text);
	    return;
	}
    }


    strftime( buf, sizeof(buf), "%H:%M:%S", localtime(&tm) );
    fprintf( stderr, "%s %08X %-5s %s\n", buf, sh4r.pc, msg_levels[level], text );
    g_free(text);
}
