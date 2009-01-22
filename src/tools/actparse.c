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

static int add_action( char **actions, struct ruleset *rules, char *operation, char *action )
{
    char *act = g_strchomp(action);
    char opclean[strlen(operation)+1];
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
            if( actions[i] != NULL ) {
                fprintf( stderr, "Duplicate actions for operation '%s'\n", operation );
                return -1;
            }
            actions[i] = act;
            return 0;
        }
    }
    fprintf(stderr, "No operation found matching '%s'\n", operation );
    return -1;
}

struct actionfile {
    FILE *f;
    char *text;
    int length;
    int yyposn;
    int yyline;
    struct ruleset *rules;
    struct actiontoken token;
};

actionfile_t action_file_open( const char *filename, struct ruleset *rules )
{
    struct stat st;
    FILE *f = fopen( filename, "ro" );
    if( f == NULL ) 
        return NULL;
    fstat( fileno(f), &st );
    
    actionfile_t af = malloc( sizeof(struct actionfile) );
    af->f = f;
    af->length = st.st_size+1;
    af->text = malloc( st.st_size+1 );
    fread( af->text, st.st_size, 1, f );
    af->text[st.st_size] = '\0';
    af->yyline = 0;
    af->yyposn = 0;
    af->rules = rules;
    af->token.symbol = NONE;
    
    return af;
}

actiontoken_t action_file_next( actionfile_t af )
{
    if( af->token.symbol == ACTIONS ) {
        /* Destroy previous actions */
        memset( af->token.actions, 0, sizeof(af->token.actions) );
    }
    
    if( af->yyposn == af->length ) {
        af->token.symbol = END;
    } else if( af->token.symbol == TEXT || /* ACTIONS must follow TEXT */ 
            (af->token.symbol == NONE && af->text[af->yyposn] == '%' && af->text[af->yyposn+1] == '%') ) {
        /* Begin action block */
        af->token.symbol = ACTIONS;

        char *operation = &af->text[af->yyposn];
        while( af->yyposn < af->length ) {
            if( af->text[af->yyposn] == '\n' ) {
                yyline++;
                if( af->text[af->yyposn+1] == '%' && af->text[af->yyposn+2] == '%' ) {
                    af->yyposn += 3;
                    break;
                }
            }

            if( af->text[af->yyposn] == '{' && af->text[af->yyposn+1] == ':' ) {
                af->text[af->yyposn] = '\0';
                af->yyposn+=2;
                char *action = &af->text[af->yyposn];
                while( af->yyposn < af->length ) {
                    if( af->text[af->yyposn] == ':' && af->text[af->yyposn+1] == '}' ) {
                        af->text[af->yyposn] = '\0';
                        af->yyposn++;
                        if( add_action( af->token.actions, af->rules, operation, action ) != 0 ) {
                            af->token.symbol = ERROR;
                            return &af->token;
                        }
                        operation = &af->text[af->yyposn+1];
                        break;
                    }
                    af->yyposn++;
                }
            }
            af->yyposn++;
        }
    } else {
        /* Text block */
        af->token.symbol = TEXT;
        af->token.text = &af->text[af->yyposn]; 
        while( af->yyposn < af->length ) {
            af->yyposn++;
            if( af->text[af->yyposn-1] == '\n' ) {
                af->yyline++;
                if( af->text[af->yyposn] == '%' && af->text[af->yyposn+1] == '%' ) {
                    af->text[af->yyposn] = '\0';
                    af->yyposn += 2;
                    break;
                }
            }
        }
    }
    return &af->token;
}

void action_file_close( actionfile_t af )
{
    free( af->text );
    fclose( af->f );
    free( af );
}

