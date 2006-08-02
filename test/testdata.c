/**
 * $Id: testdata.c,v 1.2 2006-08-02 04:13:15 nkeynes Exp $
 * 
 * Test data loader.
 *
 * Copyright (c) 2006 Nathan Keynes.
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "testdata.h"

#define DEFAULT_SIZE 1024

/* get the next 32-byte aligned address that is no less than x */
#define ALIGN_32(x)  ((char *)((((unsigned int)(x))+0x1F)&0xFFFFFFE0))

void free_test_dataset( test_data_t tests ) 
{
    test_data_t next;

    do {
	next = tests->next;
	free(tests);
	tests = next;
    } while( next != NULL );
}

test_data_block_t get_test_data( test_data_t data, char *name )
{
    int i;
    for( i=0; i<MAX_DATA_BLOCKS; i++ ) {
	if( data->item[i].name != NULL &&
	    strcmp(name, data->item[i].name) == 0 ) {
	    return &data->item[i];
	}
    }
    return NULL;
}

void dump_test_dataset( FILE *f, test_data_t dataset )
{
    test_data_t test = dataset;
    int i;
    while( test != NULL ) {
	fprintf( f, "Test: %s\n", test->test_name );
	for( i=0; i<MAX_DATA_BLOCKS; i++ ) {
	    if( test->item[i].name != NULL ) {
		fprintf( f, "Block: %s, %d bytes\n", test->item[i].name, test->item[i].length );
		fwrite_dump( f, test->item[i].data, test->item[i].length );
	    }
	}
	test = test->next;
    }
}

int test_block_compare( test_data_block_t block, char *result, int result_length )
{
    if( block->length != result_length )
	return -1;
    return memcmp( block->data, result, block->length );
}
    

/**
 * Load a batch of test data from the given IO stream.
 */
test_data_t load_test_dataset( FILE *f )
{
    test_data_t head = NULL;
    test_data_t current = NULL;
    test_data_t last = NULL;
    int current_size = 0;
    int current_block = -1;
    char *current_end = NULL;
    char *dataptr = NULL;

    char buf[512];
    char *line;
    while( fgets(buf, sizeof(buf), f ) != NULL ) {
	line = buf;
	while( isspace(*line) ) /* Trim leading whitespace */
	    line++;
	if( line[0] == '[' ) { /* New test */
	    char *test_name = line+1;
	    char *end = strchr(test_name, ']');
	    if( end != NULL )
		*end = '\0';
	    current_size = DEFAULT_SIZE;
	    test_data_t test = malloc(current_size);
	    memset( test, 0, current_size );
	    
	    dataptr = (char *)(test+1);
	    test->next = NULL;
	    if( head == NULL )
		head = test;
	    if( current != NULL )
		current->next = test;
	    last = current;
	    current = test;
	    current_end = ((char *)test) + current_size;
	    current_block = -1;
	    strcpy( dataptr, test_name );
	    test->test_name = dataptr;
	    dataptr = ALIGN_32(dataptr + strlen(test_name)+1);
	} else if( *line == '#' ) { /* Comment */
	} else {
	    char *equals = strrchr( line, '=' );
	    if( equals != NULL ) {
		char *block_name = line;
		int len;
		char *p = equals;
		*p-- = '\0';
		while( isspace(*p) )
		    *p-- = '\0';
		len = strlen(line)+1;
		if( dataptr + len > current_end ) {
		    current_end += current_size;
		    current_size *= 2;
		    current = realloc(current, current_size );
		    if( last != NULL )
			last->next = current;
		}
		current_block++;
		strcpy( dataptr, block_name );
		current->item[current_block].name = dataptr;
		dataptr = ALIGN_32(dataptr+len);
		current->item[current_block].data = dataptr;
		current->item[current_block].length = 0;

		line = equals+1;
		while( isspace(*line) )
		    line++;
	    } 

	    /* Data */
	    if( current == NULL || current_block == -1 )
		continue;
	    char *p = strtok(line, "\t\r\n ");
	    while( p != NULL ) {
		if( dataptr + 8 > current_end ) {
		    int old_size = current_size;
		    current_end += current_size;
		    current_size *= 2;
		    current = realloc(current, current_size );
		    memset( current + old_size, 0, old_size );
		    if( last != NULL )
			last->next = current;
		}
		int len = strlen(p);
		int datalen = 0;
		char *dot = strchr(p, '.');
		if( dot != NULL ) { /* FP */
		    if( p[len-1] == 'L' ) { /* Ending in L */
			p[len-1] = '\0';
			double d = strtod(p, NULL);
			*((double *)dataptr) = d;
			datalen = 8;
		    } else {
			float f = (float)strtod(p,NULL);
			*((float *)dataptr) = f;
			datalen = 4;
		    }
		} else {
		    unsigned long value = strtoul(p, NULL, 16);
		    if( len == 8 ) {
			*((unsigned int *)dataptr) = value;
			datalen = 4;
		    } else if( len == 4 ) {
			*((unsigned short *)dataptr) = value;
			datalen = 2;
		    } else if( len == 2 ) {
			*((unsigned char *)dataptr) = value;
			datalen = 1;
		    }
		}
		dataptr += datalen;
		current->item[current_block].length += datalen;
		p = strtok(NULL, "\t\r\n ");
	    }
	}
    }
    fclose(f);
    return head;
}
