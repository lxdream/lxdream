/*
 * $Id: testdata.h,v 1.3 2007-01-03 09:05:13 nkeynes Exp $
 * 
 * Test data loader
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

#include <stdio.h>

#define MAX_DATA_BLOCKS 16

typedef struct test_data_block {
    const char *name;
    unsigned int length;
    char *data;
} *test_data_block_t;

typedef struct test_data {
    const char *test_name;
    struct test_data *next;
    struct test_data_block item[MAX_DATA_BLOCKS];
} *test_data_t;

typedef int (*test_func_t)();
int run_tests( test_func_t *tests );

test_data_t load_test_dataset( FILE *f );
void free_test_dataset( test_data_t dataset );
void dump_test_dataset( FILE *f, test_data_t dataset );
int test_block_compare( test_data_block_t expect, char *actual, int actual_length );
test_data_block_t get_test_data( test_data_t dataset, char *item_name );

