/**
 * $Id$
 *
 * Tool to take an input .glsl file and write out a corresponding .c and .h
 * file based on the content. The .glsl file contains a number of shaders
 * marked with either #fragment <name> or #vertex <name>
 * a C file with appropriate escaping, as well as program definitions
 * written as #program <name> = <shader1> <shader2> ... <shaderN>
 *
 * Copyright (c) 2007-2012 Nathan Keynes.
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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <glib/gstrfuncs.h>
#include <glib/glist.h>

#define MAX_LINE 4096
#define DEF_ALLOC_SIZE 4096
#define MAX_SHADERS 128

typedef enum {
    VERTEX_SHADER = 0,
    FRAGMENT_SHADER = 1
} shader_type_t;

typedef struct variable {
    gboolean uniform; /* TRUE = uniform, FALSE = attribute */
    const char *name;
    const char *type;
} *variable_t;

typedef struct shader {
    shader_type_t type;
    const char *name;
    char *body;
    GList *variables;
} *shader_t;

typedef struct program {
    const char *name;
    gchar **shader_names;
    GList *shaders;
    GList *variables;
} *program_t;

typedef struct glsldata {
    const char *filename;
    unsigned max_shaders;
    GList *shaders;
    GList *programs;
} *glsldata_t;

#define isident(c) (isalnum(c)||(c)=='_')

static void parseVarDecl( shader_t shader, gboolean uniform, char *input )
{
    unsigned i;
    char *p = g_strstrip(input);
    for( i=0; isident(p[i]); i++)
    if( p[i] == 0 ) {
        fprintf( stderr, "Error: unable to parse variable decl '%s'\n", p );
        return; /* incomplete decl? */
    }
    char *type = g_strndup(p, i);
    p = g_strstrip(input+i);
    for( i=0; isident(p[i]); i++)
    if( p[i] == 0 ) {
        fprintf( stderr, "Error: unable to parse variable decl '%s'\n", p );
        return; /* incomplete decl? */
    }
    char *name = g_strndup(p, i);
    variable_t var = g_malloc0(sizeof(struct variable));
    var->uniform = uniform;
    var->type = type;
    var->name = name;
    shader->variables = g_list_append(shader->variables,var);
}

static shader_t findShader( GList *shaders, const char *name )
{
    GList *ptr = shaders;
    while( ptr != NULL ) {
        shader_t shader = ptr->data;
        if( strcmp(shader->name, name) == 0 )
            return shader;
        ptr = ptr->next;
    }
    return NULL;
}

static gboolean addProgramVariable( program_t program, variable_t variable )
{
    GList *ptr = program->variables;
    while( ptr != NULL ) {
        variable_t varp = ptr->data;
        if( strcmp(varp->name, variable->name) == 0 ) {
            if( varp->uniform == variable->uniform && strcmp(varp->type, variable->type) == 0 )
                return TRUE; /* All ok */
            fprintf( stderr, "Error: Variable type mismatch on '%s'\n", variable->name );
            return FALSE;
        }
        ptr = ptr->next;
    }
    program->variables = g_list_append(program->variables, variable);
    return TRUE;
}

static void linkPrograms( glsldata_t data )
{
    GList *program_ptr = data->programs;
    unsigned i;
    while( program_ptr != NULL ) {
        program_t program = program_ptr->data;
        for( i=0; program->shader_names[i] != NULL; i++ ) {
            shader_t shader = findShader(data->shaders, program->shader_names[i]);
            if( shader == NULL ) {
                fprintf( stderr, "Error: unable to resolve shader '%s'\n", program->shader_names[i] );\
            } else {
                GList *varptr = shader->variables;
                while( varptr != NULL ) {
                    addProgramVariable(program, varptr->data);
                    varptr = varptr->next;
                }
            }
        }
        program_ptr = program_ptr->next;
    }

}


static void readInput( const char *filename, glsldata_t result )
{
    char buf[MAX_LINE];
    size_t current_size = 0, current_posn = 0;
    unsigned i;

    FILE *f = fopen( filename, "ro" );
    if( f == NULL ) {
        fprintf( stderr, "Error: unable to open input file '%s': %s\n", filename, strerror(errno) );
        exit(2);
    }

    shader_t shader = NULL;
    if( result->filename == NULL ) {
        result->filename = g_strdup(filename);
    } else {
        const gchar *tmp = result->filename;
        result->filename = g_strdup_printf("%s, %s", tmp, filename);
        g_free((gchar *)tmp);
    }

    while( fgets(buf, sizeof(buf), f) != NULL ) {
        if( strlen(buf) == 0 )
            continue;

        if( strncmp(buf, "#vertex ", 8) == 0 ) {
            shader = g_malloc0(sizeof(struct shader));
            assert( shader != NULL );
            shader->type = VERTEX_SHADER;
            shader->name = strdup(g_strstrip(buf+8));
            shader->body = malloc(DEF_ALLOC_SIZE);
            shader->body[0] = '\0';
            current_size = DEF_ALLOC_SIZE;
            current_posn = 0;
            result->shaders = g_list_append(result->shaders, shader);
        } else if( strncmp( buf, "#fragment ", 10 ) == 0 ) {
            shader = g_malloc0(sizeof(struct shader));
            assert( shader != NULL );
            shader->type = FRAGMENT_SHADER;
            shader->name = strdup(g_strstrip(buf+10));
            shader->body = malloc(DEF_ALLOC_SIZE);
            shader->body[0] = '\0';
            current_size = DEF_ALLOC_SIZE;
            current_posn = 0;
            result->shaders = g_list_append(result->shaders, shader);
        } else if( strncmp( buf, "#program ", 9 ) == 0 ) {
            shader = NULL;
            program_t program = g_malloc0(sizeof(struct program));
            char *rest = buf+9;
            char *equals = strchr(rest, '=');
            if( equals == NULL ) {
                fprintf( stderr, "Error: invalid program line %s\n", buf );
                exit(2);
            }
            *equals = '\0';
            program->name = g_strdup(g_strstrip(rest));
            program->shader_names = g_strsplit_set(g_strstrip(equals+1), " \t\r,", 0);
            result->programs = g_list_append(result->programs, program);
            for(i=0;program->shader_names[i] != NULL; i++ );
            if( i > result->max_shaders )
                result->max_shaders = i;
        } else if( shader != NULL ) {
            size_t len = strlen(buf);
            if( current_posn + len > current_size ) {
                shader->body = realloc(shader->body, current_size*2);
                assert( shader->body != NULL );
                current_size *= 2;
            }
            strcpy( shader->body + current_posn, buf );
            current_posn += len;
            char *line = g_strstrip(buf);
            if( strncmp( line, "uniform ", 8 ) == 0 ) {
                parseVarDecl(shader, TRUE, line+8);
            } else if( strncmp( line, "attribute ", 10 ) == 0 ) {
                parseVarDecl(shader, FALSE, line+10);
            }
        }
    }

    fclose(f);
}

/**
 * Copy input to output, quoting " characters as we go.
 */
static void writeCString( FILE *out, const char *str )
{
    const char *p = str;

    while( *p != 0 ) {
        if( *p == '\"' ) {
            fputc( '\\', out );
        } else if( *p == '\n' ) {
            fputs( "\\n\\", out );
        }
        fputc( *p, out );
        p++;
    }
}

static const char *sl_type_map[][3] = {
        {"int", "int", "int *"},
        {"float", "float", "float *"},
        {"short", "short", "short *"},
        {"sampler", "int", "int *"},
        {"vec", "GLfloat *", "GLfloat *"},
        {"mat", "GLfloat *", "GLfloat *"},
        {NULL, NULL}
};

static const char *getCType( const char *sl_type, gboolean isUniform ) {
    for( unsigned i=0; sl_type_map[i][0] != NULL; i++ ) {
        if( strncmp(sl_type_map[i][0], sl_type, strlen(sl_type_map[i][0])) == 0 ) {
            if( isUniform ) {
                return sl_type_map[i][1];
            } else {
                return sl_type_map[i][2];
            }
        }
    }
    return "void *";
}

static void writeHeader( FILE *out, glsldata_t data )
{
    fprintf( out, "/*\n * This file automatically generated by genglsl from %s\n */\n", data->filename );
}

static void writeInterface( const char *filename, glsldata_t data )
{
    FILE *f = fopen(filename, "wo");
    if( f == NULL ) {
        fprintf( stderr, "Error: Unable to write interface file '%s': %s\n", filename, strerror(errno) );
        exit(1);
    }

    writeHeader( f, data );
    fprintf( f, "#ifndef lxdream_glsl_H\n#define lxdream_glsl_H 1\n\n" );

    fprintf( f, "typedef enum {\n" );
    const char *last_name = NULL;
    int count = 0;
    GList *shader_ptr;
    for( shader_ptr = data->shaders; shader_ptr != NULL; shader_ptr = shader_ptr->next ) {
        count++;
        shader_t shader = (shader_t)shader_ptr->data;
        fprintf( f, "    %s,\n", shader->name );
        last_name = shader->name;
    }
    fprintf( f, "} shader_id;\n\n" );

    if( last_name == NULL )
        last_name = "NULL";
    fprintf( f, "#define GLSL_LAST_SHADER %s\n", last_name );
    fprintf( f, "#define GLSL_NUM_SHADERS %d\n", count );
    fprintf( f, "#define GLSL_NO_SHADER -1\n\n" );
    fprintf( f, "#define GLSL_VERTEX_SHADER 1\n" );
    fprintf( f, "#define GLSL_FRAGMENT_SHADER 2\n" );

    count = 0;
    GList *program_ptr;
    for( program_ptr = data->programs; program_ptr != NULL; program_ptr = program_ptr->next ) {
        count++;
    }
    fprintf( f, "#define GLSL_NUM_PROGRAMS %d\n", count );

    for( program_ptr = data->programs; program_ptr != NULL; program_ptr = program_ptr->next ) {
        program_t program = program_ptr->data;
        GList *var_ptr;
        fprintf( f, "void glsl_use_%s();\n", program->name );
        for( var_ptr = program->variables; var_ptr != NULL; var_ptr = var_ptr->next ) {
            variable_t var = var_ptr->data;
            if( var->uniform ) {
                fprintf( f, "void glsl_set_%s_%s(%s value); /* uniform %s %s */ \n", program->name, var->name, getCType(var->type,var->uniform), var->type, var->name );
            } else {
                fprintf( f, "void glsl_set_%s_%s_pointer(%s ptr, GLint stride); /* attribute %s %s */ \n", program->name, var->name, getCType(var->type,var->uniform), var->type, var->name);
                if( strcmp(var->type,"vec4") == 0 ) { /* Special case */
                    fprintf( f, "void glsl_set_%s_%s_vec3_pointer(%s ptr, GLint stride); /* attribute %s %s */ \n", program->name, var->name, getCType(var->type,var->uniform), var->type, var->name);
                }
            }
        }
    }

    fprintf( f, "#endif /* !lxdream_glsl_H */\n" );

    fclose(f);
}

static void writeSource( const char *filename, glsldata_t data )
{
    FILE *f = fopen(filename, "wo");
    if( f == NULL ) {
        fprintf( stderr, "Error: Unable to write interface file '%s': %s\n", filename, strerror(errno) );
        exit(1);
    }

    writeHeader( f, data );
    fprintf( f, "struct shader_def {\n    int type;\n    const char *source;\n};\n" );

    fprintf( f, "const struct shader_def shader_source[] = {\n" );
    GList *shader_ptr;
    for( shader_ptr = data->shaders; shader_ptr != NULL; shader_ptr = shader_ptr->next ) {
        shader_t shader = (shader_t)shader_ptr->data;
        fprintf( f, "    {%s,\"", (shader->type == VERTEX_SHADER ? "GLSL_VERTEX_SHADER" : "GLSL_FRAGMENT_SHADER") );
        writeCString( f, shader->body );
        fprintf( f, "\"},\n" );
    }
    fprintf( f, "    {GLSL_NO_SHADER,NULL}};\n\n" );

    fprintf( f, "const int program_list[][%d] = {\n", data->max_shaders+1 );
    GList *program_ptr;
    GList *var_ptr;
    unsigned i;
    for( program_ptr = data->programs; program_ptr != NULL; program_ptr = program_ptr->next ) {
        program_t program = (program_t)program_ptr->data;
        fprintf( f, "    {" );
        for( i=0; program->shader_names[i] != NULL; i++ ) {
            fprintf(f, "%s,", program->shader_names[i] );
        }
        fprintf( f, "GLSL_NO_SHADER},\n" );
    }
    fprintf( f, "    {GLSL_NO_SHADER}};\n" );

    /* per-program functions */
    for( program_ptr = data->programs; program_ptr != NULL; program_ptr = program_ptr->next ) {
        program_t program = program_ptr->data;
        fprintf( f, "\nstatic gl_program_t prog_%s_id;\n",program->name );
        for( var_ptr = program->variables; var_ptr != NULL; var_ptr = var_ptr->next ) {
            variable_t var = var_ptr->data;
            fprintf( f, "static GLint var_%s_%s_loc;\n", program->name, var->name);
        }

    }
    for( program_ptr = data->programs; program_ptr != NULL; program_ptr = program_ptr->next ) {
        program_t program = program_ptr->data;
        fprintf( f, "\nstatic void glsl_cleanup_%s() {\n", program->name );
        for( var_ptr = program->variables; var_ptr != NULL; var_ptr = var_ptr->next ) {
            variable_t var = var_ptr->data;
            if( !var->uniform ) {
                fprintf( f, "    glsl_disable_attrib(var_%s_%s_loc);\n", program->name, var->name );
            }
        }
        fprintf( f, "}\n");

        fprintf( f, "\nvoid glsl_use_%s() {\n", program->name );
        fprintf( f, "    glsl_use_program(prog_%s_id);\n", program->name );
        fprintf( f, "    glsl_set_cleanup_fn(glsl_cleanup_%s);\n", program->name );
        for( var_ptr = program->variables; var_ptr != NULL; var_ptr = var_ptr->next ) {
            variable_t var = var_ptr->data;
            if( !var->uniform ) {
                fprintf( f, "    glsl_enable_attrib(var_%s_%s_loc);\n", program->name, var->name );
            }
        }
        fprintf( f, "}\n");


        for( var_ptr = program->variables; var_ptr != NULL; var_ptr = var_ptr->next ) {
            variable_t var = var_ptr->data;
            if( var->uniform ) {
                fprintf( f, "void glsl_set_%s_%s(%s value){ /* uniform %s %s */ \n", program->name, var->name, getCType(var->type,var->uniform), var->type, var->name );
                fprintf( f, "    glsl_set_uniform_%s(var_%s_%s_loc,value);\n}\n", var->type, program->name, var->name );
            } else {
                fprintf( f, "void glsl_set_%s_%s_pointer(%s ptr, GLsizei stride){ /* attribute %s %s */ \n", program->name, var->name, getCType(var->type,var->uniform), var->type, var->name);
                fprintf( f, "    glsl_set_attrib_%s(var_%s_%s_loc,stride, ptr);\n}\n", var->type, program->name, var->name );
                if( strcmp(var->type,"vec4") == 0 ) { /* Special case to load vec3 arrays into a vec4 */
                    fprintf( f, "void glsl_set_%s_%s_vec3_pointer(%s ptr, GLsizei stride){ /* attribute %s %s */ \n", program->name, var->name, getCType(var->type,var->uniform), var->type, var->name);
                    fprintf( f, "    glsl_set_attrib_vec3(var_%s_%s_loc,stride, ptr);\n}\n", program->name, var->name );
                }
            }
        }
    }

    fprintf( f, "\nstatic void glsl_init_programs( gl_program_t *ids ) {\n" );
    for( program_ptr = data->programs, i=0; program_ptr != NULL; program_ptr = program_ptr->next, i++ ) {
        program_t program = program_ptr->data;

        fprintf( f, "    prog_%s_id = ids[%d];\n\n", program->name, i );
        for( var_ptr = program->variables; var_ptr != NULL; var_ptr = var_ptr->next ) {
            variable_t var = var_ptr->data;
            if( var->uniform ) {
                fprintf( f, "    var_%s_%s_loc = glsl_get_uniform_location(prog_%s_id, \"%s\");\n", program->name, var->name, program->name, var->name );
            } else {
                fprintf( f, "    var_%s_%s_loc = glsl_get_attrib_location(prog_%s_id, \"%s\");\n", program->name, var->name, program->name, var->name );
            }
        }
    }
    fprintf( f, "}\n" );

    fclose(f);
}

static const char *makeExtension(const char *basename, const char *ext)
{
    const char *oldext = strrchr(basename, '.');
    if( oldext == NULL ) {
        return g_strdup_printf("%s%s", basename, ext);
    } else {
        return g_strdup_printf("%.*s%s", (int)(oldext-basename), basename, ext);
    }
}

static char *option_list = "hi:o:";
static struct option long_option_list[] = {
        { "help", no_argument, NULL, 'h' },
        { "interface", required_argument, 'i' },
        { "output", required_argument, NULL, 'o' },
        { NULL, 0, 0, 0 } };

static void usage() {
    fprintf( stderr, "Usage: genglsl <glsl-source-list> [-o output.def] [-i output.h]\n");
}
int main( int argc, char *argv[] )
{
    const char *output_file = NULL;
    const char *iface_file = NULL;
    int opt;

    while( (opt = getopt_long( argc, argv, option_list, long_option_list, NULL )) != -1 ) {
        switch( opt ) {
        case 'h':
            usage();
            exit(0);
            break;
        case 'i':
            if( iface_file != NULL ) {
                fprintf( stderr, "Error: at most one interface file can be supplied\n" );
                usage();
                exit(1);
            }
            iface_file = optarg;
            break;
        case 'o':
            if( output_file != NULL ) {
                fprintf( stderr, "Error: at most one output file can be supplied\n" );
                usage();
                exit(1);
            }
            output_file = optarg;
        }
    }

    if( optind == argc ) {
        usage();
        exit(1);
    }

    if( output_file == NULL ) {
        output_file = makeExtension(argv[optind], ".def");
    }
    if( iface_file == NULL ) {
        iface_file = makeExtension(output_file, ".h");
    }

    glsldata_t data = g_malloc0(sizeof(struct glsldata));
    while( optind < argc ) {
        readInput(argv[optind++], data);
    }
    linkPrograms(data);

    writeSource( output_file, data );
    writeInterface( iface_file, data );
    return 0;
}
