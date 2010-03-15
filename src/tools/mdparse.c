/**
 * $Id$
 *
 * genmmio I/O register definition parser.
 *
 * Copyright (c) 2010 Nathan Keynes.
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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <glib/gmem.h>

#include "genmach.h"
/*
 * Grammar:
 *
 * start: start register_block |  ;
 * register_block: 'registers' IDENTIFIER 'at' INTEGER opt_paramlist '{' register_list '}';
 * opt_paramlist: '(' param_list ')' | '(' ')' |  ;
 * param_list: param_list ',' param | param ;
 * param: access_param | fill_param | endian_param | mask_param ;
 * access_param: 'access' '=' 'any'  // Behaves as if it were memory
 *        | 'access' '=' 'nooffset' // Recognize any size access (zero-ext or trunc) as long as the address is exactly the same
 *        | 'access' '=' 'exact'    // Recognize only the exact access type at the exact address
 *        ;
 * fill_param: 'fill' '=' literal;
 * endian_param: 'endian' '=' 'little'
 *        | 'endian' '=' 'big'
 *        ;
 * mask_param: 'mask' '=' literal;
 *
 * register_list: register_list register_desc | ;
 * register_desc: INTEGER ':' mode_spec type_spec IDENTIFIER opt_paramlist [ '=' literal ] [ ACTION ]
 * mode_spec: 'const' | 'rw' | 'ro' | 'wo';
 * type_spec: 'i8' | 'i16' | 'i32' | 'i64' | 'f32' | 'f64' | 'string' | 'cstring';
 * literal: INTEGER | STRING | 'undefined' ;
 *
 * C-style comments are recognized as such.
 */

#define TOK_IDENTIFIER 1
#define TOK_INTEGER    2
#define TOK_STRING     3
#define TOK_ACTION     4
#define TOK_EOF        5
#define TOK_ERROR      6
#define TOK_LPAREN     7
#define TOK_RPAREN     8
#define TOK_LBRACE     9
#define TOK_RBRACE    10
#define TOK_COMMA     11
#define TOK_COLON     12
#define TOK_SEMI      13
#define TOK_PERIOD    14
#define TOK_RANGE     15
#define TOK_LSQUARE   16
#define TOK_RSQUARE   17
#define TOK_EQUALS    18
#define TOK_ACCESS    19
#define TOK_AT        20
#define TOK_ENDIAN    21
#define TOK_FILL      22
#define TOK_INCLUDE   23
#define TOK_MASK      24
#define TOK_REGISTERS 25
#define TOK_SPACE     26
#define TOK_STRIDE    27
#define TOK_TEST      28
#define TOK_TRACE     29
#define TOK_UNDEFINED 30


#define FIRST_KEYWORD TOK_ACCESS
#define LAST_KEYWORD  TOK_UNDEFINED
static const char *TOKEN_NAMES[] = {"NULL", "IDENTIFIER", "INTEGER", "STRING", "ACTION", "EOF", "ERROR",
        "(", ")", "{", "}", ",", ":", ";", ".", "..", "[", "]", "=",
        "ACCESS", "AT", "ENDIAN", "FILL", "INCLUDE", "MASK", "REGISTERS", "SPACE", "STRIDE", "TEST", "TRACE", "UNDEFINED" };
static const char *MODE_NAMES[] = {"const", "rw", "ro", "wo", "mirror"};
static const char *TYPE_NAMES[] = {"i8", "i16", "i32", "i64", "f32", "f64", "string"};
static int TYPE_SIZES[] = {1,2,4,8,4,8,0};
static const char *ENDIAN_NAMES[] = {"default", "little", "big"};
static const char *ACCESS_NAMES[] = {"default", "any", "nooffset", "exact"};
static const char *TRACE_NAMES[] = {"default", "off", "on"};

#define elementsof(x) (sizeof(x)/sizeof(x[0]))

typedef struct token_data {
    char *yytext;
    int yylength;
    int yyline;
    int yycol;
    union {
        long i;
        char *s;
    } v;
    int slen;
} token_data;

struct yystate {
    char *yybuffer;
    char *yyfilename;
    char *yyposn, *yylineposn, *yyend;
    int yylineno;
};

static GList *yyfile_stack = NULL;
static struct yystate yystate;
static struct token_data yytok;

#define YYPARSE_ERROR( msg, ... ) \
    do { \
        fprintf( stderr, "Parse error in %s:%d:%d: " msg "\n", yystate.yyfilename, yytok.yyline, yytok.yycol, __VA_ARGS__ ); \
        exit(2); \
    } while(0)

#define READ(x) \
{   int _tok = iolex(x); \
    if( _tok != (x) ) { \
        YYPARSE_ERROR( "Expected %s but got %s", TOKEN_NAMES[x], TOKEN_NAMES[_tok] ); \
    } \
}

static int iolex( int expectToken );
static int iolex_open( const char *filename );
static void iolex_close();
static int iolex_push( const char *filename );
static int iolex_pop( );

static inline char *yystrdup()
{
    char *str = g_malloc0(yytok.yylength+1);
    memcpy( str, yytok.yytext, yytok.yylength);
    return str;
}

static inline int yystrcasecmp(const char *cmp)
{
    int len = strlen(cmp);
    if( len != yytok.yylength ) {
        return yytok.yylength - len;
    }
    return strncasecmp(yytok.yytext, cmp, yytok.yylength);
}

static int yymatch( const char *arr[], unsigned numOptions )
{
    for( unsigned i=0; i<numOptions; i++ ) {
        if( yystrcasecmp( arr[i] ) == 0 ) {
            return i;
        }
    }
    return -1;
}

static gint register_block_sort_cb( gconstpointer a, gconstpointer b )
{
    struct regblock *blocka = (struct regblock *)a;
    struct regblock *blockb = (struct regblock *)b;
    return blocka->address - blockb->address;
}

static int register_ptr_sort_cb( const void *a, const void *b )
{
    regdef_t *ptra = (regdef_t *)a;
    regdef_t *ptrb = (regdef_t *)b;
    if( (*ptra)->offset != (*ptrb)->offset )
        return (*ptra)->offset - (*ptrb)->offset;
    return (*ptra)->type - (*ptrb)->type;
}

static void ioparse_apval(int tok, union apval *apv, unsigned *numBytes)
{
    if( tok == TOK_INTEGER ) {
        apv->i = yytok.v.i;
        if( *numBytes == 0 ) {
            YYPARSE_ERROR( "Expected string initializer but was an integer (0x%x)", yytok.v.i );
        }
    } else if( tok = TOK_STRING ) {
        if( *numBytes == 0 ) {
            *numBytes = yytok.slen;
            apv->s = yytok.v.s;
        } else {
            if( *numBytes != yytok.slen ) {
                YYPARSE_ERROR( "Expected %d byte initializer but was %d", *numBytes, yytok.slen );
            }
            assert( *numBytes < sizeof(uint64_t) );
            apv->i = 0;
            /* FIXME: handle endian mismatches */
            memcpy( &apv->i, yytok.v.s, yytok.slen );
        }
    } else {
        YYPARSE_ERROR( "Expected literal (integer or string), but got %s", TOKEN_NAMES[tok] );
    }
}

static void ioparse_regflags( regflags_t flags, unsigned *numBytes )
{
    do {
        int tok = iolex(TOK_RPAREN);
        switch(tok) {
        case TOK_FILL:
            READ(TOK_EQUALS);
            tok = iolex(TOK_INTEGER);
            flags->fillSizeBytes = 4;
            ioparse_apval( tok, &flags->fillValue, &flags->fillSizeBytes );
            break;
        case TOK_ACCESS:
            READ(TOK_EQUALS);
            READ(TOK_IDENTIFIER);
            flags->access = yymatch(ACCESS_NAMES,elementsof(ACCESS_NAMES));
            if( flags->access == -1 ) {
                YYPARSE_ERROR("Unknown access mode '%s'", yystrdup());
            }
            break;
        case TOK_MASK:
            if( numBytes ) {
                READ(TOK_EQUALS);
                tok = iolex(TOK_INTEGER);
                ioparse_apval( tok, &flags->maskValue, numBytes );
            } else {
                YYPARSE_ERROR("mask is not valid on a register block",0);
            }
            break;
        case TOK_ENDIAN:
            READ(TOK_EQUALS);
            READ(TOK_IDENTIFIER);
            flags->endian = yymatch(ENDIAN_NAMES,elementsof(ENDIAN_NAMES));
            if( flags->endian == -1 ) {
                YYPARSE_ERROR("Unknown endianness '%s'", yystrdup());
            }
            break;
        case TOK_TRACE:
            READ(TOK_EQUALS);
            READ(TOK_IDENTIFIER);
            flags->traceFlag = yymatch(TRACE_NAMES,elementsof(TRACE_NAMES));
            if( flags->traceFlag == -1 ) {
                YYPARSE_ERROR("Unknown trace flag '%s'", yystrdup());
            }
            break;
        case TOK_TEST:
            READ(TOK_EQUALS);
            READ(TOK_IDENTIFIER);
            flags->testFlag = yymatch(TRACE_NAMES,elementsof(TRACE_NAMES));
            if( flags->testFlag == -1 ) {
                YYPARSE_ERROR("Unknown test flag '%s'", yystrdup());
            }
            break;
        case TOK_COMMA:
            break;
        case TOK_RPAREN:
            return;
        default:
            YYPARSE_ERROR("Expected flag or ')' but was %s", TOKEN_NAMES[tok]);
        }
    } while(1);
}

static void ioparse_regdef( struct regdef *reg )
{
    reg->offset = yytok.v.i;
    reg->numElements = 1;
    reg->numBytes = 0;
    reg->initValue.i = 0;
    reg->flags.maskValue.i = -1;
    unsigned rangeOffset = reg->offset;

    int tok = iolex(TOK_COLON);
    if( tok == TOK_RANGE ) {
        READ(TOK_INTEGER);
        rangeOffset = yytok.v.i;
        if( rangeOffset < reg->offset ) {
            YYPARSE_ERROR( "Range end (0x%x) must be greater than the range start (0x%x)", rangeOffset, reg->offset );
        }
        READ(TOK_COLON);
    } else if( tok != TOK_COLON ) {
        YYPARSE_ERROR( "Expected ':' but was %s\n", TOKEN_NAMES[tok] );
    }
    READ(TOK_IDENTIFIER);
    reg->mode = yymatch(MODE_NAMES, elementsof(MODE_NAMES));
    if( reg->mode == -1 ) {
        YYPARSE_ERROR( "Unknown register mode '%s'", yystrdup() );
    }
    if( reg->mode == REG_MIRROR ) {
        /* Mirror regions have a target range only */
        READ(TOK_INTEGER);
        reg->initValue.i = yytok.v.i;
        reg->type = REG_I8;
        tok = iolex(TOK_RANGE);
        if( tok == TOK_RANGE ) {
            READ(TOK_INTEGER);
            if( yytok.v.i < reg->initValue.i ) {
                YYPARSE_ERROR( "Invalid mirror target range 0x%x..0x%x", reg->initValue.i, yytok.v.i );
            }
            reg->numBytes = yytok.v.i - reg->initValue.i + 1;
            tok = iolex(TOK_STRIDE);
        }
        if( tok == TOK_STRIDE ) {
            READ(TOK_INTEGER);
            reg->stride = yytok.v.i;
            tok = iolex(TOK_ACTION);
        } else {
            reg->stride = reg->numBytes;
        }

        if( tok != TOK_SEMI ) {
            YYPARSE_ERROR( "Expected ; but gut %s", TOKEN_NAMES[tok] );
        }

        if( reg->stride < reg->numBytes ) {
            YYPARSE_ERROR( "Invalid mirror stride: %x is less than block size %x\n", reg->stride, reg->numBytes );
        }

        if( rangeOffset != reg->offset ) {
            reg->numElements *= ((rangeOffset - reg->offset) + reg->stride - 1) / reg->stride;
        }

    } else {
        READ(TOK_IDENTIFIER);
        reg->type = yymatch(TYPE_NAMES, elementsof(TYPE_NAMES));
        if( reg->type == -1 ) {
            YYPARSE_ERROR( "Unknown register type '%s'", yystrdup() );
        }
        reg->numBytes = TYPE_SIZES[reg->type];
        tok = iolex(TOK_IDENTIFIER);
        if( tok == TOK_IDENTIFIER ) {
            reg->name = yystrdup();
            tok = iolex(TOK_ACTION);
        }
        if( tok == TOK_STRING ) {
            reg->description = yytok.v.s;
            tok = iolex(TOK_ACTION);
        }
        if( tok == TOK_LPAREN ) {
            ioparse_regflags(&reg->flags, &reg->numBytes);
            tok = iolex(TOK_ACTION);
        }
        if( tok == TOK_EQUALS ) {
            tok = iolex(TOK_INTEGER);
            if( tok == TOK_UNDEFINED ) {
                reg->initValue.i = 0xDEADBEEFDEADBEEF;
                reg->initUndefined = TRUE;
            } else {
                ioparse_apval(tok, &reg->initValue, &reg->numBytes);
            }
            tok = iolex(TOK_ACTION);
        } else if( reg->type == REG_STRING ) {
            YYPARSE_ERROR( "String declarations must have an initializer (ie = 'abcd')",0 );
        }
        if( tok == TOK_ACTION ) {
            // reg->action = yystrdup();
        } else if( tok != TOK_SEMI ) {
            YYPARSE_ERROR( "Expected ; or {, but got %s", TOKEN_NAMES[tok] );
        }

        if( rangeOffset != reg->offset ) {
            reg->numElements *= ((rangeOffset - reg->offset) + reg->numBytes - 1) / reg->numBytes;
        }
    }

}

static struct regblock *ioparse_regblock( )
{
    unsigned regsAllocSize = 128;
    struct regblock *block = g_malloc0(sizeof(struct regblock) + sizeof(regdef_t)*regsAllocSize);
    block->numRegs = 0;

    READ(TOK_IDENTIFIER);
    block->name = yystrdup();
    int tok = iolex(TOK_AT);
    if( tok == TOK_STRING ) {
        block->description = yytok.v.s;
        tok = iolex(TOK_AT);
    }
    if( tok != TOK_AT ) {
        YYPARSE_ERROR("Expected AT but got %s\n", TOKEN_NAMES[tok] );
    }
    READ(TOK_INTEGER);
    block->address = yytok.v.i;

    tok = iolex(TOK_LBRACE);
    if( tok == TOK_LPAREN) {
        ioparse_regflags(&block->flags, NULL);
        READ(TOK_LBRACE);
    } else if( tok != TOK_LBRACE ) {
        YYPARSE_ERROR("Expected { but got %s\n", TOKEN_NAMES[tok] );
    }

    tok = iolex(TOK_INTEGER);
    while( tok != TOK_RBRACE ) {
        if( tok != TOK_INTEGER ) {
            YYPARSE_ERROR("Expected INTEGER or } but got %s\n", TOKEN_NAMES[tok]);
        }
        struct regdef *regdef = g_malloc0(sizeof(struct regdef));
        if( block->numRegs >= regsAllocSize ) {
            regsAllocSize *= 2;
            block = g_realloc(block, sizeof(struct regblock) + sizeof(regdef_t)*regsAllocSize);
        }
        block->regs[block->numRegs++] = regdef;
        ioparse_regdef(regdef);

        tok = iolex(TOK_INTEGER);
    }

    qsort( &block->regs[0], block->numRegs, sizeof(block->regs[0]), register_ptr_sort_cb );

    return block;
}

GList *ioparse( const char *filename, GList *list )
{
    GList *blocks = list;
    int count = 0;

    if( iolex_open(filename) != 0  )
        return blocks;

    int tok;
    while(1) {
        tok = iolex(TOK_REGISTERS);
        if( tok == TOK_EOF ) {
            int result = iolex_pop();
            if( result == -1 )
                break;
        } else if( tok == TOK_INCLUDE) {
            READ(TOK_STRING);
            char *tmp = yystrdup();
            READ(TOK_SEMI);
            int result = iolex_push( tmp );
            if( result == -1 ) {
                YYPARSE_ERROR("Unable to include file '%s'", tmp);
            }
            free(tmp);
        } else if( tok == TOK_SPACE ) {
        } else if( tok == TOK_REGISTERS ) {
            struct regblock *block = ioparse_regblock(block);
            count++;
            blocks = g_list_insert_sorted(blocks, block, register_block_sort_cb);
        } else {
            YYPARSE_ERROR("Expected REGISTERS but got %s\n", TOKEN_NAMES[tok] );
        }
    }
    return blocks;
}

/**************************** Lexical analyser ***************************/

static int iolex_push( const char *filename )
{
    struct yystate *save = g_malloc(sizeof(struct yystate));
    memcpy( save, &yystate, sizeof(struct yystate) );

    int result = iolex_open(filename);
    if( result == 0 ) {
        yyfile_stack = g_list_prepend(yyfile_stack, save);
    }
    return result;
}

static int iolex_pop( )
{
    iolex_close();
    if( yyfile_stack == NULL )
        return -1;
    struct yystate *top = (struct yystate *)yyfile_stack->data;
    yyfile_stack = g_list_remove(yyfile_stack, top);
    memcpy( &yystate, top, sizeof(struct yystate) );
    g_free( top );
    return 0;
}

static int iolex_open( const char *filename )
{
    struct stat st;
    int fd = open( filename, O_RDONLY );
    if( fd == -1 ) {
        fprintf( stderr, "Error opening file '%s': %s\n", filename, strerror(errno) );
        return -1;
    }

    if( fstat( fd, &st ) != 0 ) {
        fprintf( stderr, "Error statting file '%s': %s\n", filename, strerror(errno) );
        close(fd);
        return -1;
    }

    char *data = g_malloc( st.st_size + 1 );
    if( read(fd, data, st.st_size) != st.st_size ) {
        fprintf( stderr, "Error reading file '%s': %s\n", filename, strerror(errno) );
        close(fd);
        return -1;
    }
    close(fd);
    data[st.st_size] = 0;

    yystate.yybuffer = yystate.yyposn = data;
    yystate.yyend = data + st.st_size;
    yystate.yylineno = 1;
    yystate.yylineposn = yystate.yyposn;
    yystate.yyfilename = strdup(filename);
    return 0;
}

static void iolex_close()
{
    g_free(yystate.yybuffer);
    free(yystate.yyfilename);
    memset(&yystate, 0, sizeof(struct yystate));
}

#define YYRETURN(x) do{ \
    yytok.yylength = yystate.yyposn - yystart; \
    return (x); \
} while(0)

static int iolex_readhex(char *p, int digits)
{
    int result = 0;
    for( int i=0; i < digits; i++ ) {
        result <<= 4;
        if( isdigit(p[i]) ) {
            result += p[i]-'0';
        } else if( p[i] >= 'a' && p[i] <= 'f' ) {
            result += p[i]-'a'+10;
        } else if( p[i] >= 'A' && p[i] <= 'F' ) {
            result += p[i]-'A'+10;
        } else {
            return (result >> 4);
        }

    }
    return result;
}

static int iolex_readoctal(char *p, int digits)
{
    int result = 0;
    for( int i=0; i < digits; i++ ) {
        result <<= 3;
        if( p[i] >= '0' && p[i] <= '7' ) {
            result += p[i]-'0';
        } else {
            return (result >> 4);
        }
    }
    return result;
}

/**
 * Copy and interpret the string segment as a C string, replacing escape
 * expressions with the corresponding value.
 */
static char *iolex_getcstring( char *start, char *end, int *len )
{
   char *result = g_malloc0(end-start+1); /* Allocate enough memory for the string as-is */
   char *q = result;

   for( char *p = start; p != end; p++ ) {
       if( *p == '\\' ) {
           if( ++p == end ) {
               *q++ = '\\';
               break;
           }
           if( p[0] >= '0' && p[0] <= '3' && p+3 <= end &&
               p[1] >= '0' && p[1] <= '7' &&
               p[2] >= '0' && p[2] <= '7' ) {
               *q++ = (char)iolex_readoctal(p,3);
               p+=2;
           } else {
               switch( *p ) {
               case 'n':
                   *q++ = '\n';
                   break;
               case 'r':
                   *q++ = '\r';
                   break;
               case 't':
                   *q++ = '\t';
                   break;
               case 'x': /* hex */
                   if( p + 3 > end || !isxdigit(p[1]) || !isxdigit(p[2]) ) {
                       *q++ = '\\';
                       *q++ = *p;
                   } else {
                       *q++ = (char)iolex_readhex(p+1, 2);
                       p+=2;
                   }
                   break;
               default:
                   *q++ = '\\';
                   *q++ = *p;
               }
           }
       } else {
           *q++ = *p;
       }
   }
   *len = q - result;
   return result;
}

int iolex( int expectToken )
{
    int count = 0;
    while( yystate.yyposn < yystate.yyend ) {
        char *yystart = yytok.yytext = yystate.yyposn;
        yytok.yylength = 1;
        yytok.yyline = yystate.yylineno;
        yytok.yycol = yystate.yyposn - yystate.yylineposn+1;
        int ch = *yystate.yyposn++;
        if( isdigit(ch) ) {
            /* INTEGER */
            if( ch == '0' ) {
                if( *yystate.yyposn == 'x' ) {
                    while( yystate.yyposn < yystate.yyend && isxdigit(*++yystate.yyposn) ) ;
                } else {
                    while( yystate.yyposn < yystate.yyend && *yystate.yyposn >= '0' && *yystate.yyposn <= '7' )
                        yystate.yyposn++;
                }
            } else {
                while( yystate.yyposn < yystate.yyend && isdigit(*yystate.yyposn) )
                    yystate.yyposn++;
            }
            yytok.v.i = strtol( yystart, NULL, 0 );
            YYRETURN(TOK_INTEGER);
        } else if( isalpha(ch) || ch == '_' ) {
            /* IDENTIFIER */
            while( yystate.yyposn < yystate.yyend && (isalnum(*yystate.yyposn) || *yystate.yyposn == '_') )
                yystate.yyposn++;
            yytok.yylength = yystate.yyposn - yystart;
            if( expectToken == TOK_IDENTIFIER ) {
                YYRETURN(TOK_IDENTIFIER);
            }
            /* Otherwise check for keywords */
            for( int i=FIRST_KEYWORD; i <= LAST_KEYWORD; i++ ) {
                if( strlen(TOKEN_NAMES[i]) == yytok.yylength &&
                    strncasecmp(TOKEN_NAMES[i], yystart, yytok.yylength ) == 0 ) {
                    YYRETURN(i);
                }
            }
            YYRETURN(TOK_IDENTIFIER);
        } else if( isspace(ch) ) {
            if( ch == '\n' ) {
                yystate.yylineno++;
                yystate.yylineposn = yystate.yyposn;
            }
            while( isspace(*yystate.yyposn) ) {
                if( *yystate.yyposn == '\n' ) {
                    yystate.yylineno++;
                    yystate.yylineposn = yystate.yyposn+1;
                }
                yystate.yyposn++;
            }
        } else {
            switch( ch ) {
            case '(': YYRETURN(TOK_LPAREN);
            case ')': YYRETURN(TOK_RPAREN);
            case '[': YYRETURN(TOK_LSQUARE);
            case ']': YYRETURN(TOK_RSQUARE);
            case ',': YYRETURN(TOK_COMMA);
            case ':': YYRETURN(TOK_COLON);
            case ';': YYRETURN(TOK_SEMI);
            case '=': YYRETURN(TOK_EQUALS);
            case '/':
                if( *yystate.yyposn == '/' ) { /* Line comment */
                    while( yystate.yyposn < yystate.yyend && *++yystate.yyposn != '\n' ) ;
                } else if( *yystate.yyposn == '*' ) { /* C comment */
                    while( yystate.yyposn < yystate.yyend && (*++yystate.yyposn != '*' || *++yystate.yyposn != '/' ) ) {
                        if( *yystate.yyposn == '\n' ) {
                            yystate.yylineno++;
                            yystate.yylineposn = yystate.yyposn+1;
                        }
                    }
                }
                break;
            case '\'': /* STRING */
                while( *yystate.yyposn != '\'' ) {
                    if( *yystate.yyposn == '\n' ) {
                        fprintf( stderr, "Unexpected newline in string constant!\n" );
                        YYRETURN(TOK_ERROR);
                    } else if( yystate.yyposn >= yystate.yyend ) {
                        fprintf( stderr, "Unexpected EOF in string constant!\n" );
                        YYRETURN(TOK_ERROR);
                    } else if( *yystate.yyposn == '\\' && yystate.yyposn[1] == '\'' ) {
                        yystate.yyposn++;
                    }
                    yystate.yyposn++;
                }
                yystate.yyposn++;
                yytok.v.s = iolex_getcstring(yystart+1, yystate.yyposn-1, &yytok.slen);
                YYRETURN(TOK_STRING);
            case '\"': /* STRING */
                while( *yystate.yyposn != '\"' ) {
                    if( *yystate.yyposn == '\n' ) {
                        fprintf( stderr, "Unexpected newline in string constant!\n" );
                        YYRETURN(TOK_ERROR);
                    } else if( yystate.yyposn >= yystate.yyend ) {
                        fprintf( stderr, "Unexpected EOF in string constant!\n" );
                        YYRETURN(TOK_ERROR);
                    } else if( *yystate.yyposn == '\\' && yystate.yyposn[1] == '\"' ) {
                        yystate.yyposn++;
                    }
                    yystate.yyposn++;
                }
                yystate.yyposn++;
                yytok.v.s = iolex_getcstring(yystart+1, yystate.yyposn-1, &yytok.slen);
                YYRETURN(TOK_STRING);
            case '}':
                YYRETURN(TOK_RBRACE);
            case '{': /* ACTION or LBRACE */
                if( expectToken == TOK_LBRACE ) {
                    YYRETURN(TOK_LBRACE);
                } else {
                    count++;
                    while( count > 0 && yystate.yyposn < yystate.yyend ) {
                        if( *yystate.yyposn == '{' )
                            count++;
                        if( *yystate.yyposn == '}' )
                            count--;
                        yystate.yyposn++;
                    }
                    YYRETURN(TOK_ACTION);
                }
            case '.':
                if( *yystate.yyposn == '.' ) {
                    yystate.yyposn++;
                    YYRETURN(TOK_RANGE);
                } else {
                    YYRETURN(TOK_PERIOD);
                }
            default:
                fprintf( stderr, "Illegal character: '%c'\n", ch );
                YYRETURN(TOK_ERROR);
            }
        }

    }
    return TOK_EOF;
}
