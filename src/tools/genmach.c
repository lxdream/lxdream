/**
 * $Id$
 *
 * mmio register code generator
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include "gettext.h"
#include "genmach.h"

#define LXDREAM_PAGE_SIZE 4096

const char *short_options = "cd:ho:v";
struct option long_options[] = {
        { "output", required_argument, NULL, 'o' },
        { "header", required_argument, NULL, 'd' },
        { "check-only", no_argument, NULL, 'c' },
        { "help", no_argument, NULL, 'h' },
        { "verbose", no_argument, NULL, 'v' },
        { NULL, 0, 0, 0 } };

static void print_version()
{
    printf( "genmmio 0.1\n" );
}

static void print_usage()
{
    print_version();
    printf( "Usage: genmmio [options] <input-register-files>\n" );
    printf( "Options:\n" );
    printf( "   -c, --check-only       %s\n", _("Check specification files but don't write any output") );
    printf( "   -d, --header=FILE      %s\n", _("Specify header output file [corresponding .h for .c file]") );
    printf( "   -h, --help             %s\n", _("Display this usage information") );
    printf( "   -o, --output=FILE      %s\n", _("Specify main output file [corresponding .c for input file]") );
    printf( "   -v, --verbose          %s\n", _("Print verbose output") );
}

/**
 * Given an input baseName of the form "path/file.blah", return
 * a newly allocated string "file<ext>" where <ext> is the second
 * parameter.
 *
 * @param baseName a non-null filename
 * @param ext a file extension including leading '.'
 */
char *makeFilename( const char *baseName, const char *ext )
{
    const char *p = strrchr(baseName, '/');
    if( p == NULL )
        p = baseName;
    const char *q = strchr(p, '.');
    if( q == NULL ) {
        return g_strdup_printf("%s%s", p, ext);
    } else {
        return g_strdup_printf("%.*s%s", q-p, p, ext );
    }
}

/**
 * Process all registers after parsing is complete. This does a few things:
 *   1. Ensures that there are no overlapping registers, so that every operation
 *      is well-defined.
 *   2. Replaces transitive mirror groups with a set of equivalent direct
 *      mirrors so that later processing doesn't need to check for this case.
 *      This also checks for cycles in the graph.
 *   3. Organises the memory map(s) into page-size quantities (combining and
 *      splitting groups where necessary).
 */
GList *postprocess_registers( GList *blocks )
{
    for( GList *ptr = blocks; ptr != NULL; ptr = ptr->next ) {
        regblock_t block = (regblock_t)ptr->data;
        for( unsigned i=0; i<block->numRegs; i++ ) {
            regdef_t reg = block->regs[i];
            if( reg->mode == REG_MIRROR ) {
            }
        }
    }
    return blocks;
}

/**
 * Check register definition semantics. Primarily this verifies that there
 * are no overlapping definitions.
 * @return number of errors found.
 */
int check_registers( GList *registers )
{
    return 0;
}

/**
 * Dump the high-level register map to the given file.
 */
void dump_register_blocks( FILE *f, GList *blocks, gboolean detail )
{
    const char *mode_names[] = { "--", "RW", "RO", "RW" };
    for( GList *ptr = blocks; ptr != NULL; ptr=ptr->next ) {
        regblock_t block = (regblock_t)ptr->data;
        fprintf( f, "%08X: %s - %s\n", block->address,
                 block->name,
                 (block->description == NULL ? "<no description>" : block->description) );
        if( detail ) {
            for( unsigned i=0; i<block->numRegs; i++ ) {
                regdef_t reg = block->regs[i];
                fprintf( f, "    %04X: %s %s", reg->offset,
                        mode_names[reg->mode],
                        reg->name == NULL ? "<anon>" : reg->name );
                if( reg->numElements > 1 ) {
                    fprintf( f, "[%d]", reg->numElements );
                }
                fprintf( f, " - %s\n",
                         (reg->description == NULL ? "<no description>" : reg->description) );
            }
        }
    }
}

char *build_page_initializer( regblock_t block )
{
    char *page = g_malloc(LXDREAM_PAGE_SIZE);

    /* First, background fill if any */
    if( block->flags.fillSizeBytes == 0 ) {
        memset(page, 0, LXDREAM_PAGE_SIZE);
    } else {
        for( unsigned i=0; i<LXDREAM_PAGE_SIZE; i++ ) {
            page[i] = block->flags.fillValue.a[i%block->flags.fillSizeBytes];
        }
    }

    /* Next, set register initializer (except for wo + mirror regs) */
    for( unsigned i=0; i<block->numRegs; i++ ) {
        regdef_t reg = block->regs[i];
        if( reg->mode != REG_WO && reg->mode != REG_MIRROR ) {
            unsigned offset = reg->offset;
            for( unsigned j=0; j<reg->numElements; j++ ) {
                if( reg->type == REG_STRING ) {
                    memcpy( &page[offset], reg->initValue.s, reg->numBytes );
                } else {
                    memcpy( &page[offset], reg->initValue.a, reg->numBytes );
                }
                offset += reg->numBytes;
            }
        }
    }

    /* Finally clone mirror groups */
    for( unsigned i=0; i<block->numRegs; i++ ) {
        regdef_t reg = block->regs[i];
        if( reg->mode == REG_MIRROR ) {
            unsigned offset = reg->offset;
            unsigned target = reg->initValue.i;
            for( unsigned i=0; i<reg->numElements; i++ ) {
                memcpy( &page[offset], &page[target], reg->numBytes );
                offset += reg->stride;
            }
        }
    }

    return page;
}


void fwrite_dump( unsigned char *data, unsigned int length, FILE *f )
{
    unsigned int i, j;
    int skipped = 0;
    for( i =0; i<length; i+=16 ) {
        if( i >= 16 && i < length-16 && memcmp( &data[i], &data[i-16], 16) == 0 ) {
            skipped++;
            continue;
        }
        if( skipped ) {
            fprintf( f, "   **    \n" );
            skipped = 0;
        }
        fprintf( f, "%08X:", i);
        for( j=i; j<i+16; j++ ) {
            if( j != i && (j % 4) == 0 )
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

static const char *file_top = "/**\n"
        " * This file was automatically generated by genmmio, do not edit\n"
        " */\n";

void write_source( FILE *f, GList *blocks, const char *header_filename )
{
    fputs( file_top, f );
    fprintf( f, "\n#include \"%s\"\n\n", header_filename );

    for( GList *ptr = blocks; ptr != NULL; ptr = ptr->next ) {
        regblock_t block = (regblock_t)ptr->data;
        /* Generate the mmio region struct */
        fprintf( f, "void FASTCALL mmio_region_%s_write(uint32_t, uint32_t);\n", block->name );
        fprintf( f, "void FASTCALL mmio_region_%s_write_word(uint32_t, uint32_t);\n", block->name );
        fprintf( f, "void FASTCALL mmio_region_%s_write_byte(uint32_t, uint32_t);\n", block->name );
        fprintf( f, "void FASTCALL mmio_region_%s_write_burst(uint32_t, unsigned char *);\n", block->name );
        fprintf( f, "int32_t FASTCALL mmio_region_%s_read(uint32_t);\n", block->name );
        fprintf( f, "int32_t FASTCALL mmio_region_%s_read_word(uint32_t);\n", block->name );
        fprintf( f, "int32_t FASTCALL mmio_region_%s_read_byte(uint32_t);\n", block->name );
        fprintf( f, "void FASTCALL mmio_region_%s_read_burst(unsigned char *, uint32_t);\n", block->name );
        fprintf( f, "struct mmio_region mmio_region_%s = {\n", block->name );
        fprintf( f, "    \"%s\", \"%s\", %08x, {\n", block->name,
                block->description == NULL ? block->name : block->description,
                block->address );
        fprintf( f, "        mmio_region_%s_read, mmio_region_%s_write,\n", block->name, block->name );
        fprintf( f, "        mmio_region_%s_read_word, mmio_region_%s_write_word,\n", block->name, block->name );
        fprintf( f, "        mmio_region_%s_read_byte, mmio_region_%s_write_byte,\n", block->name, block->name );
        fprintf( f, "        mmio_region_%s_read_burst, mmio_region_%s_write_burst,\n", block->name, block->name );
        fprintf( f, "        unmapped_prefetch, mmio_region_%s_read_byte },\n", block->name, block->name );
        fprintf( f, "    NULL, NULL, {\n" );
        for( unsigned i=0; i<block->numRegs; i++ ) {
            regdef_t reg = block->regs[i];
            if( reg->mode != REG_CONST ) {
                char tmp[32];
                const char *regname = reg->name;
                if( regname == NULL ) {
                    regname = tmp;
                    snprintf( tmp, sizeof(tmp), "%s_%03X", block->name, reg->offset );
                }

                fprintf( f, "        { \"%s\", \"%s\", %d, 0x%03x, 0x%08x, %s },\n", regname,
                         reg->description == NULL ? regname : reg->description,
                                 reg->numBytes*8, reg->offset, (unsigned)reg->initValue.i,
                                 (reg->mode == REG_RW ? "PORT_MRW" : (reg->mode == REG_WO ? "PORT_W" : "PORT_R")) );
            }
        }
        fprintf( f, "    }, NULL };\n" );
    }

}

void write_header( FILE *f, GList *blocks )
{
    fputs( file_top, f );

    fputs( "\n#include \"mmio.h\"\n\n", f );

    for( GList *ptr = blocks; ptr != NULL; ptr = ptr->next ) {
        regblock_t block = (regblock_t)ptr->data;
        fprintf( f, "extern struct mmio_region mmio_region_%s;\n", block->name );
        fprintf( f, "enum mmio_region_%s_port_t {\n", block->name );
        for( unsigned i=0; i<block->numRegs; i++ ) {
            regdef_t reg = block->regs[i];
            if( reg->name != NULL ) {
                fprintf( f, "    %s = 0x%03x,\n", reg->name, reg->offset );
            }
        }
        fprintf( f, "};\n" );
    }
}

void write_test( FILE *f, GList *blocks )
{


}

int main(int argc, char *argv[])
{
    const char *header = NULL;
    const char *output = NULL;
    gboolean check_only = FALSE;
    int verbose = 0, opt;
    GList *block_list = NULL;

    while( (opt = getopt_long( argc, argv, short_options, long_options, NULL )) != -1 ) {
        switch(opt) {
        case 'c':
            check_only = TRUE;
            break;
        case 'd':
            header = optarg;
            break;
        case 'h':
            print_usage();
            exit(0);
        case 'o':
            output = optarg;
            break;
        case 'v':
            verbose++;
            break;
        }
    }

    if( optind == argc ) {
        print_usage();
        exit(1);
    }

    if( output == NULL ) {
        output = makeFilename(argv[optind],".c");
    }
    if( header == NULL ) {
        header = makeFilename(argv[optind],".h");
    }

    for( ; optind < argc; optind++ ) {
        block_list = ioparse(argv[optind], block_list);
    }

    if( verbose ) {
        dump_register_blocks(stdout, block_list, verbose>1);
    }

    int errors = check_registers(block_list);
    if( errors != 0 ) {
        fprintf( stderr, "Aborting due to validation errors\n" );
        return 2;
    }

    if( !check_only ) {
        FILE *f = fopen( output, "wo" );
        if( f == NULL ) {
            fprintf( stderr, "Unable to open output file '%s': %s\n", output, strerror(errno) );
            return 3;
        }
        write_source( f, block_list, header );
        fclose(f);

        f = fopen( header, "wo" );
        if( f == NULL ) {
            fprintf( stderr, "Unable to open header file '%s': %s\n", header, strerror(errno) );
            return 4;
        }
        write_header( f, block_list );
        fclose(f);
    }

    for( GList *ptr = block_list; ptr != NULL; ptr = ptr->next ) {
        char *data = build_page_initializer((regblock_t)ptr->data);
        fwrite_dump( data, LXDREAM_PAGE_SIZE, stdout );
        g_free(data);
    }
    return 0;
}

