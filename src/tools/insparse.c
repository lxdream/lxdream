#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "tools/gendec.h"

#define CONSUME_CHAR(x) if( **str != x ) { fprintf( stderr, "Unexpected input character '%c', expected '%c' at line %d\n", **str, x, yyline ); return -1; } else { (*str)++; }
static int yyline;

struct rule *new_rule() {
    struct rule *rule = malloc( sizeof( struct rule ) );
    memset( rule, 0, sizeof( struct rule ) );
    return rule;
}

int parse_registers_block( char *buf, int buflen, FILE *f );
int parse_rule( char **str, struct rule *rule );
int parse_bitstring( char **str, struct rule *rule );
int parse_bitoperand( char **str, struct rule *rule );
int parse_integer( char **str );
int parse_rule_format( char **str, struct rule *rule );
int parse_operand_uses( char **str, struct rule *rule );



struct ruleset *parse_ruleset_file( FILE *f ) 
{
    struct ruleset *rules = malloc( sizeof(struct ruleset ) );
    char buf[512];

    rules->rule_count = 0;
    yyline = 0;
    while( fgets( buf, sizeof(buf), f ) != NULL ) {
	yyline++;
	if( strncasecmp(buf, "registers", 9) == 0 ) {
	    parse_registers_block(buf, sizeof(buf), f);
	} else if( buf[0] != '\0' && buf[0] != '#' && buf[0] != '\n' ) {
	    struct rule *rule;
	    char *p = buf;
	    rule = new_rule();
	    if( parse_rule( &p, rule ) != 0 ) {
		free( rule );
	    } else {
		rules->rules[rules->rule_count++] = rule;
	    }
	}
    }
    return rules;
}

int parse_registers_block( char *buf, int buflen, FILE *f ) {
    do {
	if( strchr(buf, '}') != NULL ) {
	    break;
	}
    } while( fgets( buf, buflen, f ) != NULL );
    return 0;
}

/**
 * Parse a single complete rule
 * @return 0 on success, non-zero on failure
 */
int parse_rule( char **str, struct rule *rule ) 
{
    if( parse_bitstring( str, rule ) != 0 ) {
	return -1;
    }

    /* consume whitespace in between */
    while( isspace(**str) ) (*str)++;
    if( **str == '\0' ) {
	fprintf( stderr, "Unexpected end of file in rule on line %d\n", yyline );
	return -1;
    }
    
    int result = parse_rule_format( str, rule );
    if( result == 0 ) {
	/* Reverse operand bit shifts */
	int j;
	for( j=0; j<rule->operand_count; j++ ) {
	    rule->operands[j].bit_shift = 
		rule->bit_count - rule->operands[j].bit_shift - rule->operands[j].bit_count;
	}
	if( **str == '!' ) {
	    (*str)++;
	    result = parse_operand_uses( str, rule );
	}
    }

    return 0;
}

int parse_bitstring( char **str, struct rule *rule )
{
    while( !isspace(**str) ) {
	int ch = **str;
	(*str)++;
	switch( ch ) {
	case '0':
	    rule->bits = rule->bits << 1;
	    rule->mask = (rule->mask << 1) | 1;
	    rule->bit_count++;
	    break;
	case '1':
	    rule->bits = (rule->bits << 1) | 1;
	    rule->mask = (rule->mask << 1) | 1;
	    rule->bit_count++;
	    break;
	case '(':
	    if( parse_bitoperand( str, rule ) != 0 ) {
		return -1 ;
	    }
	    break;
	default:
	    (*str)--;
	    fprintf( stderr, "Unexpected character '%c' in bitstring at line %d\n", ch, yyline );
	    return -1;
	}
    }
    return 0;
}

int parse_bitoperand( char **str, struct rule *rule )
{
    char *p = rule->operands[rule->operand_count].name;

    if( rule->operand_count == MAX_OPERANDS ) {
	fprintf( stderr, "Maximum operands/rule exceeded (%d) at line %d\n", MAX_OPERANDS, yyline );
	return -1;
    }

    while( isalnum(**str) || **str == '_' ) {
	*p++ = *(*str)++;
    }
    *p = '\0';
    CONSUME_CHAR(':');
    
    int size = parse_integer( str );
    if( size == -1 ) {
	return -1; 
    }
    rule->operands[rule->operand_count].bit_count = size;
    if( **str == 's' || **str == 'S' ) {
	(*str)++;
	rule->operands[rule->operand_count].is_signed = 1;
    } else if( **str == 'u' || **str == 'U' ) {
	(*str)++;
	rule->operands[rule->operand_count].is_signed = 0;
    }
    if( **str == '<' ) {
	(*str)++;
	CONSUME_CHAR('<');
	int lsl = parse_integer(str);
	if( lsl == -1 ) {
	    return -1;
	}
	rule->operands[rule->operand_count].left_shift = lsl;
    }
    CONSUME_CHAR(')');

    rule->operands[rule->operand_count].bit_shift = rule->bit_count;
    rule->bit_count += size;
    rule->bits = rule->bits << size;
    rule->mask = rule->mask << size;
    rule->operand_count++;
    return 0;
}

int parse_integer( char **str )
{
    uint32_t val = 0;
    if( !isdigit(**str) ) {
	fprintf(stderr, "Expected digit (0-9) but was '%c' at line %d\n", **str, yyline );
	return -1;
    }
    do {
	val = val * 10 + (**str - '0');
	(*str)++;
    } while( isdigit(**str) );
    return val;
}

int parse_rule_format( char **str, struct rule *rule )
{

    char tmp[64];
    char *p = tmp;
    while( **str != '\n' && **str != '\0' && **str != '!' ) {
	*p++ = *(*str)++;
    }
    *p = '\0';
    strcpy( rule->format, tmp );

    return 0;
}

int parse_operand_uses( char **str, struct rule *rule )
{
    return 0;
}

void dump_ruleset( struct ruleset *rules, FILE *f ) 
{
    int i, j;
    fprintf( f, "Rulset: %d rules\n", rules->rule_count );
    for( i=0; i<rules->rule_count; i++ ) {
	struct rule *rule = rules->rules[i];
	fprintf( f, "Match: %08X/%08X %s: ", rule->bits, rule->mask, rule->format );
	for( j=0; j<rule->operand_count; j++ ) {
	    fprintf( f, "%s = (%s)(ir>>%d)&0x%X ", rule->operands[j].name,
		     rule->operands[j].is_signed ? "signed" : "unsigned", 
		     rule->operands[j].bit_shift,
		     (1<<rule->operands[j].bit_count)-1 );
	}
	fprintf( f, "\n" );
    }
}

void dump_rulesubset( struct ruleset *rules, int ruleidx[], int rule_count, FILE *f )
{
    int i,j;
    for( i=0; i<rule_count; i++ ) {
	struct rule *rule = rules->rules[ruleidx[i]];
	fprintf( f, "Match: %08X/%08X %s: ", rule->bits, rule->mask, rule->format );
	for( j=0; j<rule->operand_count; j++ ) {
	    fprintf( f, "%s = (%s)(ir>>%d)&0x%X ", rule->operands[j].name,
		     rule->operands[j].is_signed ? "signed" : "unsigned", 
		     rule->operands[j].bit_shift,
		     (1<<rule->operands[j].bit_count)-1 );
	}
	fprintf( f, "\n" );
    }
}
