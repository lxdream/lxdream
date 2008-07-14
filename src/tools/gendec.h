/**
 * $Id$
 *
 * mem is responsible for creating and maintaining the overall system memory
 * map, as visible from the SH4 processor. (Note the ARM has a different map)
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

#ifndef lxdream_gendec_H
#define lxdream_gendec_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_OPERAND_NAME 8
#define MAX_OPERANDS 4
#define MAX_OPERATION_FORMAT 64
#define MAX_RULES 512

#define USE_NONE 0
#define USE_READ 1
#define USE_WRITE 2
#define USE_READWRITE 3

struct operand {
    int bit_count;
    int bit_shift;
    int left_shift;
    int is_signed;
    int use_mode;
    char name[MAX_OPERAND_NAME+1];
};

struct rule {
    uint32_t bits;
    uint32_t mask;
    int bit_count;
    int operand_count;
    int flags_use_mode;
    struct operand operands[MAX_OPERANDS];
    char format[MAX_OPERATION_FORMAT+1];
};

struct ruleset {
    uint32_t rule_count;
    struct rule *rules[MAX_RULES];
};

struct ruleset *parse_ruleset_file( FILE *f );
void dump_ruleset( struct ruleset *rules, FILE *f );
void dump_rulesubset( struct ruleset *rules, int ruleidx[], int rule_count, FILE *f );

struct action {
    char operand_names[MAX_OPERANDS][MAX_OPERAND_NAME+1];
    char *body;
};

struct actionset {
    char *pretext;
    char *posttext;
    char *actions[MAX_RULES];
};

struct actionset *parse_action_file( struct ruleset *rules, FILE *f );

int generate_decoder( struct ruleset *rules, struct actionset *actions, FILE *f );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_gendec_H */
