/**
 * $Id$
 *
 * Test cases for path helper functions
 *
 * Copyright (c) 2012 Nathan Keynes.
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
#include <stdio.h>
#include <string.h>
#include <glib/gmem.h>
#include "lxpaths.h"

char *lxdream_get_global_config_path_value() { }
void log_message( void *ptr, int level, const gchar *source, const char *msg, ... ) { }

struct expanded_path_case_t {
    const char *input;
    const char *output;
};

char *env_vars[] = { "TEST1=quux", "TEST2=${BLAH}", "TEST3=", "2=3", "TEST_HOME=/home/foo", NULL };
const char *unset_env_vars[] = { "PATH_TEST", "1", NULL };
struct expanded_path_case_t expanded_path_cases[] = {
    {NULL, NULL},
    {"", ""},
    {"a", "a"},
    {"$", "$"},
    {"blah$", "blah$"},
    {"\\$", "$"},
    {"foo\\${TEST}\\n\\\\r", "foo${TEST}n\\r"},
    {"/home/user/.lxdreamrc", "/home/user/.lxdreamrc"},
    {"${TEST_HOME}/.lxdreamrc", "/home/foo/.lxdreamrc"},
    {"$TEST_HOME/bar", "/home/foo/bar"},
    {"/home/$TEST1/blah", "/home/quux/blah"},
    {"/tmp/${TEST2}/abcd", "/tmp/${BLAH}/abcd"},
    {"$TEST1$TEST2$TEST3$1$2", "quux${BLAH}3"},
    {"\"/home/foo\"", "/home/foo"},
    {NULL,NULL}
};

gboolean check_expanded_path( const char *input, const char *output )
{
    char * result = get_expanded_path(input);
    if( output == NULL ) {
        if( result != NULL ) {
            printf( "Unexpected non-null result from get_expanded_path(NULL), got '%s'\n", result );
            g_free(result);
            return FALSE;
        } else {
            return TRUE;
        }
    } else if( result == NULL ) {
        printf( "Unexpected NULL result from get_expanded_path('%s'), expected '%s'\n", input, output );
        return FALSE;
    } else if( strcmp(result, output) != 0 ) {
        printf( "Unexpected result from get_expanded_path('%s'), expected '%s' but was '%s'\n", input, output, result );
        g_free(result);
        return FALSE;
    } else {
        g_free(result);
        return TRUE;
    }
}


gboolean test_get_expanded_path()
{
    int count, i;
    int fails = 0;
    
    for( i=0; env_vars[i] != NULL; i++ ) {
        putenv(env_vars[i]);
    }
    for( i=0; unset_env_vars[i] != NULL; i++ ) {
        unsetenv(unset_env_vars[i]);
    }

    for( count=0; expanded_path_cases[count].input != NULL || count == 0; count++ ) {
        gboolean success = check_expanded_path(expanded_path_cases[count].input, expanded_path_cases[count].output);
        if( !success )
            fails ++;
    }
    printf( "get_expanded_path: %d/%d (%s)\n", (count-fails), count, (fails == 0 ? "OK" : "ERROR"));
    return fails == 0 ? TRUE : FALSE;

    /* FIXME: Should probably restore the env state, but doesn't matter at the moment */
}

int main()
{
    gboolean result = TRUE;
    result =  test_get_expanded_path() && result;
    
    return result ? 0 : 1;
}
