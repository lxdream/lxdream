/**
 * $Id$
 *
 * GLSL wrapper code to hide the differences between the different gl/sl APIs.
  *
 * Copyright (c) 2007-2010 Nathan Keynes.
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

#include "lxdream.h"
#include "display.h"
#include "pvr2/glutil.h"

#define MAX_ERROR_BUF 4096
#define INVALID_SHADER 0
#define INVALID_PROGRAM 0

#ifdef HAVE_OPENGL_SHADER_ARB
typedef GLhandleARB gl_program_t;
typedef GLhandleARB gl_shader_t;
#else
typedef GLuint gl_program_t;
typedef GLuint gl_shader_t;
#endif

gboolean glsl_is_supported();
gl_shader_t glsl_create_vertex_shader( const char *source );
gl_shader_t glsl_create_fragment_shader( const char *source );
gl_program_t glsl_create_program( gl_shader_t *shaderv );
void glsl_use_program(gl_program_t program);
void glsl_destroy_shader(gl_shader_t shader);
void glsl_destroy_program(gl_program_t program);

#ifdef HAVE_OPENGL_SHADER_ARB

gboolean glsl_is_supported()
{
    return isGLExtensionSupported("GL_ARB_fragment_shader") &&
    isGLExtensionSupported("GL_ARB_vertex_shader") &&
    isGLExtensionSupported("GL_ARB_shading_language_100");
}

const char *glsl_get_version()
{
    return glGetString(GL_SHADING_LANGUAGE_VERSION_ARB);
}

void glsl_print_error( char *msg, GLhandleARB obj )
{
    char buf[MAX_ERROR_BUF];
    GLsizei length;
    glGetInfoLogARB( obj, sizeof(buf), &length, buf );
    ERROR( "%s: %s", msg, buf );
}

gboolean glsl_check_shader_error( char *msg, GLhandleARB obj )
{
    GLint value;

    glGetObjectParameterivARB(obj, GL_OBJECT_COMPILE_STATUS_ARB, &value);
    if( value == 0 ) {
        glsl_print_error(msg, obj);
        return FALSE;
    }
    return TRUE;
}

gboolean glsl_check_program_error( char *msg, GLhandleARB obj )
{
    if( glGetError() != GL_NO_ERROR ) {
        glsl_print_error(msg, obj);
    }
    return TRUE;
}

gl_shader_t glsl_create_vertex_shader( const char *source )
{
    gboolean ok;
    gl_shader_t shader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);

    glShaderSourceARB( shader, 1, &source, NULL );
    glCompileShaderARB(shader);
    ok = glsl_check_shader_error("Failed to compile vertex shader", shader);
    if( !ok ) {
        glDeleteObjectARB(shader);
        return INVALID_SHADER;
    } else {
        return shader;
    }
}

gl_shader_t glsl_create_fragment_shader( const char *source )
{
    gboolean ok;
    gl_shader_t shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);

    glShaderSourceARB( shader, 1, &source, NULL );
    glCompileShaderARB(shader);
    ok = glsl_check_shader_error("Failed to compile fragment shader", shader);
    if( !ok ) {
        glDeleteObjectARB(shader);
        return INVALID_SHADER;
    } else {
        return shader;
    }
}

gl_program_t glsl_create_program( gl_shader_t *shaderv )
{
    gboolean ok;
    unsigned i;
    gl_program_t program = glCreateProgramObjectARB();

    for( i=0; shaderv[i] != INVALID_SHADER; i++ ) {
        glAttachObjectARB(program, shaderv[i]);
    }

    glLinkProgramARB(program);
    ok = glsl_check_program_error( "Failed to link shader program", program );
    if( !ok ) {
        glDeleteObjectARB(program);
        return INVALID_PROGRAM;
    } else {
        return program;
    }
}

void glsl_use_program(gl_program_t program)
{
    glUseProgramObjectARB(program);
}

void glsl_destroy_shader(gl_shader_t shader)
{
    glDeleteObjectARB(shader);
}

void glsl_destroy_program(gl_program_t program)
{
    glDeleteObjectARB(program);
}

static inline GLint glsl_get_uniform_location_prim(gl_program_t program, const char *name)
{
    return glGetUniformLocationARB(program, name);
}

static inline void glsl_set_uniform_int_prim(GLint location, GLint value)
{
    glUniform1iARB(location,value);
}

#elif HAVE_OPENGL_SHADER

gboolean glsl_is_supported()
{
    return isGLExtensionSupported("GL_ARB_fragment_shader") &&
    isGLExtensionSupported("GL_ARB_vertex_shader") &&
    isGLExtensionSupported("GL_ARB_shading_language_100");
}

const char *glsl_get_version()
{
    return glGetString(GL_SHADING_LANGUAGE_VERSION);
}

gboolean glsl_check_shader_error( char *msg, GLuint shader )
{
    GLint value;

    glGetShaderiv( shader, GL_COMPILE_STATUS, &value );
    if( value == 0 ) {
        char buf[MAX_ERROR_BUF];
        GLsizei length;
        glGetShaderInfoLog( shader, sizeof(buf), &length, buf );
        ERROR( "%s: %s", msg, buf );
        return FALSE;
    }
    return TRUE;
}

gboolean glsl_check_program_error( char *msg, GLuint program )
{
    if( glGetError() != GL_NO_ERROR ) {
        char buf[MAX_ERROR_BUF];
        GLsizei length;
        glGetProgramInfoLog( program, sizeof(buf), &length, buf );
        ERROR( "%s: %s", msg, buf );
        return FALSE;
    }
    return TRUE;
}

gl_shader_t glsl_create_vertex_shader( const char *source )
{
    gboolean ok;
    gl_shader_t shader = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource( shader, 1, &source, NULL );
    glCompileShader(shader);
    ok = glsl_check_shader_error( "Failed to compile vertex shader", glsl_vert_shader );
    if( !ok ) {
        glDeleteShader(shader);
        return INVALID_SHADER;
    } else {
        return shader;
    }

}

gl_shader_t glsl_create_fragment_shader( const char *source )
{
    gboolean ok;
    gl_shader_t shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource( shader, 1, &source, NULL );
    glCompileShader(shader);
    ok = glsl_check_shader_error( "Failed to compile fragment shader", glsl_frag_shader );
    if( !ok ) {
        glDeleteShader(shader);
        return INVALID_SHADER;
    } else {
        return shader;
    }
}

gl_program_t glsl_create_program( gl_shader_t *shaderv )
{
    gboolean ok;
    unsigned i;
    gl_program_t program = glCreateProgram();

    for( i=0; shaderv[i] != INVALID_SHADER; i++ ) {
        glAttachShader(program, shaderv[i]);
    }
    glLinkProgram(program);
    ok = glsl_check_program_error( "Failed to link shader program", program );
    if( !ok ) {
        glDeleteProgram(program);
        return INVALID_PROGRAM;
    } else {
        return program;
    }
}

void glsl_use_program(gl_program_t program)
{
    glUseProgram(program);
}

void glsl_destroy_shader(gl_shader_t shader)
{
    glDeleteShader(shader);
}

void glsl_destroy_program(gl_program_t program)
{
    glDeleteProgram(program);
}

static inline GLint glsl_get_uniform_location_prim(gl_program_t program, const char *name)
{
    return glGetUniformLocation(program, name);
}
static inline void glsl_set_uniform_int_prim(GLint location, GLint value)
{
    glUniform1i(location, value);
}

#else
gboolean glsl_is_supported()
{
    return FALSE;
}

int glsl_get_version()
{
    return 0;
}

gl_shader_t glsl_create_vertex_shader( const char *source )
{
    return 0;
}

gl_shader_t glsl_create_fragment_shader( const char *source )
{
    return 0;
}

gl_program_t glsl_create_program( gl_shader_t vertex, gl_shader_t fragment )
{
    return 0;
}

void glsl_use_program(gl_program_t program)
{
}

void glsl_destroy_shader(gl_shader_t shader)
{
}

void glsl_destroy_program(gl_program_t program)
{
}

static inline GLint glsl_get_uniform_location_prim(gl_program_t program, const char *name)
{
    return 0;
}

static inline void glsl_set_uniform_int_prim(GLint location, GLint value)
{
}
#endif

/****************************************************************************/

/* Pull in the auto-generated shader definitions */

#include "pvr2/shaders.def"

static gl_program_t program_array[GLSL_NUM_PROGRAMS];

gboolean glsl_load_shaders()
{
    gl_shader_t shader_array[GLSL_NUM_SHADERS];
    gboolean ok = TRUE;
    unsigned i, j;
    for( i=0; i<GLSL_NUM_SHADERS; i++ )
        shader_array[i] = INVALID_SHADER;
    for( i=0; i<GLSL_NUM_PROGRAMS; i++ )
        program_array[i] = INVALID_PROGRAM;

    /* Compile the shader fragments */
    for( i=0; shader_source[i].type != GLSL_NO_SHADER; i++ ) {
        gl_shader_t shader = INVALID_SHADER;
        switch(shader_source[i].type) {
        case GLSL_VERTEX_SHADER:
            shader = glsl_create_vertex_shader(shader_source[i].source);
            break;
        case GLSL_FRAGMENT_SHADER:
            shader = glsl_create_fragment_shader(shader_source[i].source);
            break;
        }
        if( shader == INVALID_SHADER ) {
            ok = FALSE;
            break;
        } else {
            shader_array[i] = shader;
        }
    }

    /* Link the programs */
    if(ok) for( i=0; program_list[i][0] != GLSL_NO_SHADER; i++ ) {
        gl_shader_t shaderv[GLSL_NUM_SHADERS+1];
        for( j=0; program_list[i][j] != GLSL_NO_SHADER; j++ ) {
            shaderv[j] = shader_array[program_list[i][j]];
        }
        shaderv[j] = INVALID_SHADER;
        gl_program_t program = glsl_create_program(shaderv);
        if( program == INVALID_PROGRAM ) {
            ok = FALSE;
            break;
        } else {
            /* Check that we can actually use the program (can this really fail?) */
            glsl_use_program(program);
            if( !glsl_check_program_error( "Failed to activate shader program", program ) ) {
                ok = FALSE;
            }
            program_array[i] = program;
        }
    }

    /**
     * Destroy the compiled fragments (the linked programs don't need them
     * anymore)
     */
    for( i=0; i<GLSL_NUM_SHADERS; i++ ) {
        if( shader_array[i] != INVALID_SHADER )
            glsl_destroy_shader(shader_array[i]);
    }

    /**
     * If we errored, delete the programs. It's all or nothing.
     */
    if( !ok ) {
        glsl_unload_shaders();
        return FALSE;
    }
    
    glsl_use_program(0);
    return TRUE;
}

void glsl_unload_shaders()
{
    unsigned i;
    for( i=0; i<GLSL_NUM_PROGRAMS; i++ ) {
        if( program_array[i] != INVALID_PROGRAM ) {
            glsl_destroy_program(program_array[i]);
            program_array[i] = INVALID_PROGRAM;
        }
    }
}

gboolean glsl_set_shader(unsigned i)
{
    assert( i >= 0 && i <= GLSL_LAST_PROGRAM );

    if( program_array[i] != INVALID_PROGRAM ) {
        glsl_use_program(program_array[i]);
        return TRUE;
    } else {
        return FALSE;
    }
}

GLint glsl_get_uniform_location( unsigned program, const char *name )
{
    assert( program >= 0 && program <= GLSL_LAST_PROGRAM );

    return glsl_get_uniform_location_prim(program_array[program], name);
}

void glsl_set_uniform_int( unsigned program, const char *name, GLint value )
{
    assert( program >= 0 && program <= GLSL_LAST_PROGRAM );
    GLint location = glsl_get_uniform_location_prim(program_array[program], name);
    glsl_set_uniform_int_prim(location, value);
}

void glsl_clear_shader()
{
    glsl_use_program(0);
}
