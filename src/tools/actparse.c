/**
 * $Id$
 *
 * gendec action file parser. 
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <glib/gstrfuncs.h>
#include "tools/gendec.h"

static int yyline;

struct action *new_action() {
    struct action *action = malloc( sizeof( struct action ) );
    memset( action, 0, sizeof( struct action ) );
    return action;
}

int add_action( struct actionset *actions, struct ruleset *rules, char *operation, char *action )
{
    char *act = g_strchomp(action);

    char opclean[strlen(operation)];
    char *p = operation, *q = opclean;
    int i;

    // Strip c-style comments 
    while( *p ) {
	if( *p == '/' && *(p+1) == '*' ) {
	    p+=2;
	    while( *p ) {
		if( *p == '*' && *(p+1) == '/' ) {
		    p+=2;
		    break;
		}
		p++;
	    }
	} else if( *p == '/' && *(p+1) == '/' ) {
	    p+=2;
	    while( *p && *p != '\n' ) {
		p++;
	    }
	} else {
	    *q++ = *p++;
	}
    }
    *q = '\0';
    strcpy( operation, g_strstrip(opclean) );

    for( i=0; i<rules->rule_count; i++ ) {
	if( strcasecmp(rules->rules[i]->format, operation) == 0 ) {
	    if( actions->actions[i] != NULL ) {
		fprintf( stderr, "Duplicate actions for operation '%s'\n", operation );
		return -1;
	    }
	    actions->actions[i] = act;
	    return 0;
	}
    }
    fprintf(stderr, "No operation found matching '%s'\n", operation );
    return -1;
}
	

struct actionset *parse_action_file( struct ruleset *rules, FILE *f ) 
{
    struct actionset *actions = malloc( sizeof(struct actionset ) );
    struct stat st;
    char *text;
    int i, length;
    
    memset( actions, 0, sizeof( struct actionset ) );
    /* Read whole file in (for convenience) */
    fstat( fileno(f), &st );
    length = st.st_size;
    text = malloc( length+1 );
    fread( text, length, 1, f );
    text[length] = '\0';
    yyline = 0;
    actions->pretext = text;
    for( i=0; i<length; i++ ) {
	if( text[i] == '\n' ) {
	    yyline++;
	    if( i+3 < length && text[i+1] == '%' && text[i+2] == '%' ) {
		text[i+1] = '\0';
		i+=3;
		break;
	    }
	}
    }

    char *operation = &text[i];
    for( ; i<length; i++ ) {
	if( text[i] == '\n' ) {
	    yyline++;
	    if( i+3 < length && text[i+1] == '%' && text[i+2] == '%' ) {
		i+=3;
		break;
	    }
	}
	
	if( text[i] == '{' && text[i+1] == ':' ) {
	    text[i] = '\0';
	    i+=2;
	    char *action = &text[i];
	    for( ;i<length; i++ ) {
		if( text[i] == ':' && text[i+1] == '}' ) {
		    text[i] = '\0';
		    i++;
		    if( add_action( actions, rules, operation, action ) != 0 ) {
			free(actions);
			free(text);
			return NULL;
		    }
		    operation = &text[i+1];
		    break;
		}
	    }
	}
    }

    actions->posttext = &text[i];
	
    return actions;
}
