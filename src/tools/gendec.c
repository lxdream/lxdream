/**
 * $Id$
 * 
 * Parse the instruction and action files and generate an appropriate
 * instruction decoder.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>
#include <glib/gstrfuncs.h>
#include <assert.h>
#include "tools/gendec.h"

#define DEFAULT_OUT_EXT ".c"

const char *ins_filename = NULL;
const char *act_filename = NULL;
const char *out_filename = NULL;

#define GEN_SOURCE 1
#define GEN_TEMPLATE 2

FILE *ins_file, *act_file, *out_file;

char *option_list = "tmho:";
int gen_mode = GEN_SOURCE;
struct option longopts[1] = { { NULL, 0, 0, 0 } };

static void usage() {
    printf( "gendec <instruction-file> <action-file> [ -o <output-file> ]\n" );
}

/**
 * Find a mask that can be used to split up the given rules
 */
static uint32_t find_mask( struct ruleset *rules, int ruleidx[], int rule_count, 
                    uint32_t input_mask )
{
    int i;
    uint32_t mask = rules->rules[ruleidx[0]]->mask;

    for( i=1; i<rule_count; i++ ) {
        mask = mask & rules->rules[ruleidx[i]]->mask;
    }

    assert( (mask & input_mask) == input_mask ); /* input_mask should always be included in the mask */

    return mask & (~input_mask); /* but we don't want to see the input mask again */
}

static int get_option_count_for_mask( uint32_t mask ) {
    int count = 0;

    while( mask ) {
        if( mask&1 ) 
            count++;
        mask >>= 1;
    }
    return 1<<count;
}

int get_bitshift_for_mask( uint32_t mask ) {
    int shift = 0;
    while( mask && !(mask&1) ) {
        shift++;
        mask >>= 1;
    }
    return shift;
}

static void get_option_values_for_mask( uint32_t *options, 
                                 uint32_t mask ) 
{
    /* This could be a lot smarter. But it's not */
    int i;
    *options = 0;
    for( i=1; i<=mask; i++ ) {
        if( (i & mask) > *options ) {
            options++;
            *options = (i&mask);
        }
    }
}

static void fprint_indent( const char *action, int depth, FILE *f )
{
    int spaces = 0, needed = depth*8, i;
    const char *text = action;

    /* Determine number of spaces in first line of input */
    for( i=0; isspace(action[i]); i++ ) {
        if( action[i] == '\n' ) {
            spaces = 0;
            text = &action[i+1];
        } else {
            spaces++;
        }
    }

    needed -= spaces;
    fprintf( f, "%*c", needed, ' ' );
    for( i=0; text[i] != '\0'; i++ ) {
        fputc( text[i], f );
        if( text[i] == '\n' && text[i+1] != '\0' ) {
            fprintf( f, "%*c", needed, ' ' );
        }
    }
    if( text[i-1] != '\n' ) {
        fprintf( f, "\n" );
    }
}

static void fprint_action( struct rule *rule, const struct action *action, int depth, FILE *f ) 
{
    int i;
    if( action == NULL ) {
        fprintf( f, "%*cUNIMP(ir); /* %s */\n", depth*8, ' ', rule->format );
    } else {
        fprintf( f, "%*c{ /* %s */", depth*8, ' ', rule->format );
        if( rule->operand_count != 0 ) {
            fprintf( f, "\n%*c", depth*8, ' ' );
            for( i=0; i<rule->operand_count; i++ ) {
                if( rule->operands[i].is_signed ) {
                    fprintf( f, "int32_t %s = SIGNEXT%d", rule->operands[i].name, rule->operands[i].bit_count );
                } else {
                    fprintf( f, "uint32_t %s = ", rule->operands[i].name );
                }
                if( rule->operands[i].bit_shift == 0 ) {
                    fprintf( f, "(ir&0x%X)", (1<<(rule->operands[i].bit_count))-1 );
                } else {
                    fprintf( f, "((ir>>%d)&0x%X)", rule->operands[i].bit_shift,
                            (1<<(rule->operands[i].bit_count))-1 );
                }
                if( rule->operands[i].left_shift != 0 ) {
                    fprintf( f, "<<%d", rule->operands[i].left_shift );
                }
                fprintf( f, "; " );
            }
        }
        fputs( "\n", f );
        if( action->text && action->text[0] != '\0' ) {
            fprintf( f, "#line %d \"%s\"\n", action->lineno, action->filename );
            fprint_indent( action->text, depth, f );
        }
        fprintf( f, "%*c}\n", depth*8, ' ' );
    }
}

static void split_and_generate( struct ruleset *rules, const struct action *actions, 
                         int ruleidx[], int rule_count, int input_mask, 
                         int depth, FILE *f ) {
    uint32_t mask;
    int i,j;

    if( rule_count == 0 ) {
        fprintf( f, "%*cUNDEF(ir);\n", depth*8, ' ' );
    } else if( rule_count == 1 ) {
        fprint_action( rules->rules[ruleidx[0]], &actions[ruleidx[0]], depth, f );
    } else {

        mask = find_mask(rules, ruleidx, rule_count, input_mask);
        if( mask == 0 ) { /* No matching mask? */
            fprintf( stderr, "Error: unable to find a valid bitmask (%d rules, %08X input mask)\n", rule_count, input_mask );
            dump_rulesubset( rules, ruleidx, rule_count, stderr );
            return;
        }

        /* break up the rules into sub-sets, and process each sub-set.
         * NB: We could do this in one pass at the cost of more complex
         * data structures. For now though, this keeps it simple
         */
        int option_count = get_option_count_for_mask( mask );
        uint32_t options[option_count];
        int subruleidx[rule_count];
        int subrule_count;
        int mask_shift = get_bitshift_for_mask( mask );
        int has_empty_options = 0;
        get_option_values_for_mask( options, mask );

        if( mask_shift == 0 ) {
            fprintf( f, "%*cswitch( ir&0x%X ) {\n", depth*8, ' ', mask );
        } else {
            fprintf( f, "%*cswitch( (ir&0x%X) >> %d ) {\n", depth*8, ' ',
                    mask, mask_shift);
        }
        for( i=0; i<option_count; i++ ) {
            subrule_count = 0;
            for( j=0; j<rule_count; j++ ) {
                int match = rules->rules[ruleidx[j]]->bits & mask;
                if( match == options[i] ) {
                    subruleidx[subrule_count++] = ruleidx[j];
                }
            }
            if( subrule_count == 0 ) {
                has_empty_options = 1;
            } else {
                fprintf( f, "%*ccase 0x%X:\n", depth*8+4, ' ', options[i]>>mask_shift );
                split_and_generate( rules, actions, subruleidx, subrule_count,
                                    mask|input_mask, depth+1, f );
                fprintf( f, "%*cbreak;\n", depth*8+8, ' ' );
            }
        }
        if( has_empty_options ) {
            fprintf( f, "%*cdefault:\n%*cUNDEF(ir);\n%*cbreak;\n",
                    depth*8+4, ' ', depth*8+8, ' ', depth*8 + 8, ' ' );
        }
        fprintf( f, "%*c}\n", depth*8, ' ' );
    }
}

static int generate_decoder( struct ruleset *rules, actionfile_t af, FILE *out )
{
    int ruleidx[rules->rule_count];
    int i;

    for( i=0; i<rules->rule_count; i++ ) {
        ruleidx[i] = i;
    }

    actiontoken_t token = action_file_next(af);
    while( token->symbol != END ) {
        if( token->symbol == TEXT ) {
            fprintf( out, "#line %d \"%s\"\n", token->lineno, token->filename );
            fputs( token->text, out );
        } else if( token->symbol == ERROR ) {
            fprintf( stderr, "Error parsing action file" );
            return -1;
        } else {
            split_and_generate( rules, token->actions, ruleidx, rules->rule_count, 0, 1, out );
        }
        token = action_file_next(af);
    }
    return 0;
}

static int generate_template( struct ruleset *rules, actionfile_t af, FILE *out )
{
    int i;
    
    actiontoken_t token = action_file_next(af);
    while( token->symbol != END ) {
        if( token->symbol == TEXT ) {
            fputs( token->text, out );
        } else if( token->symbol == ERROR ) {
            fprintf( stderr, "Error parsing action file" );
            return -1;
        } else {
            fputs( "%%\n", out );
            for( i=0; i<rules->rule_count; i++ ) {
                fprintf( out, "%s {: %s :}\n", rules->rules[i]->format,
                        token->actions[i].text == NULL ? "" : token->actions[i].text );
            }
            fputs( "%%\n", out );
        }
        token = action_file_next(af);
    }

    return 0;
}


int main( int argc, char *argv[] )
{
    int opt;

    /* Parse the command line */
    while( (opt = getopt_long( argc, argv, option_list, longopts, NULL )) != -1 ) {
        switch( opt ) {
        case 't':
            gen_mode = GEN_TEMPLATE;
            break;
        case 'o':
            out_filename = optarg;
            break;
        case 'h':
            usage();
            exit(0);
        }
    }
    if( optind < argc ) {
        ins_filename = argv[optind++];
    }
    if( optind < argc ) {
        act_filename = argv[optind++];
    }

    if( optind < argc || ins_filename == NULL || act_filename == NULL ) {
        usage();
        exit(1);
    }

    if( out_filename == NULL ) {
        if( gen_mode == GEN_TEMPLATE ) {
            out_filename = act_filename;
        } else {
            char tmp[strlen(act_filename)+1];
            strcpy( tmp, act_filename);
            char *c = strrchr( tmp, '.' );
            if( c != NULL ) {
                *c = '\0';
            }
            out_filename = g_strconcat( tmp, DEFAULT_OUT_EXT, NULL );
        }
    }

    /* Open the files */
    ins_file = fopen( ins_filename, "ro" );
    if( ins_file == NULL ) {
        fprintf( stderr, "Unable to open '%s' for reading (%s)\n", ins_filename, strerror(errno) );
        exit(2);
    }

    /* Parse the input */
    struct ruleset *rules = parse_ruleset_file( ins_file );
    fclose( ins_file );
    if( rules == NULL ) {
        exit(5);
    }
    
    actionfile_t af = action_file_open( act_filename, rules );
    if( af == NULL ) {
        fprintf( stderr, "Unable to open '%s' for reading (%s)\n", act_filename, strerror(errno) );
        exit(3);
    }


    /* Open the output file */
    out_file = fopen( out_filename, "wo" );
    if( out_file == NULL ) {
        fprintf( stderr, "Unable to open '%s' for writing (%s)\n", out_filename, strerror(errno) );
        exit(4);
    }

    switch( gen_mode ) {
    case GEN_SOURCE:
        if( generate_decoder( rules, af, out_file ) != 0 ) {
            exit(7);
        }
        break;
    case GEN_TEMPLATE:
        if( generate_template( rules, af, out_file ) != 0 ) {
            exit(7);
        }
        break;
    }
    
    action_file_close(af);
    fclose( out_file );
    return 0;
}
