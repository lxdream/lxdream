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
#include "pvr2/shaders.h"

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

typedef void (*program_cleanup_fn_t)();
static void glsl_set_cleanup_fn( program_cleanup_fn_t );

#ifdef HAVE_OPENGL_SHADER_ARB

gboolean glsl_is_supported()
{
    return isOpenGLES2() || (isGLExtensionSupported("GL_ARB_fragment_shader") &&
    isGLExtensionSupported("GL_ARB_vertex_shader") &&
    isGLExtensionSupported("GL_ARB_shading_language_100"));
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

static inline GLint glsl_get_uniform_location(gl_program_t program, const char *name)
{
    return glGetUniformLocationARB(program, name);
}

static inline GLint glsl_get_attrib_location(gl_program_t program, const char *name)
{
    return glGetAttribLocationARB(program, name);
}

#define glsl_set_uniform_sampler1D(id,v) glUniform1iARB(id,v)
#define glsl_set_uniform_sampler2D(id,v) glUniform1iARB(id,v)
#define glsl_set_uniform_float(id,v) glUniform1fARB(id,v)
#define glsl_set_uniform_vec2(id,v) glUniform2fvARB(id,1,v)
#define glsl_set_uniform_vec3(id,v) glUniform3fvARB(id,1,v)
#define glsl_set_uniform_vec4(id,v) glUniform4fvARB(id,1,v)
#define glsl_set_uniform_mat4(id,v) glUniformMatrix4fvARB(id,1,GL_FALSE,v)
#define glsl_set_attrib_vec2(id,stride,v) glVertexAttribPointerARB(id, 2, GL_FLOAT, GL_FALSE, stride, v)
#define glsl_set_attrib_vec3(id,stride,v) glVertexAttribPointerARB(id, 3, GL_FLOAT, GL_FALSE, stride, v)
#define glsl_set_attrib_vec4(id,stride,v) glVertexAttribPointerARB(id, 4, GL_FLOAT, GL_FALSE, stride, v)
#define glsl_enable_attrib(id) glEnableVertexAttribArrayARB(id)
#define glsl_disable_attrib(id) glDisableVertexAttribArrayARB(id)

#elif HAVE_OPENGL_SHADER

gboolean glsl_is_supported()
{
    return isOpenGLES2() || (isGLExtensionSupported("GL_ARB_fragment_shader") &&
    isGLExtensionSupported("GL_ARB_vertex_shader") &&
    isGLExtensionSupported("GL_ARB_shading_language_100"));
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
    ok = glsl_check_shader_error( "Failed to compile vertex shader", shader );
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
    ok = glsl_check_shader_error( "Failed to compile fragment shader", shader );
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

static inline GLint glsl_get_uniform_location(gl_program_t program, const char *name)
{
    return glGetUniformLocation(program, name);
}
static inline GLint glsl_get_attrib_location(gl_program_t program, const char *name)
{
    return glGetAttribLocation(program, name);
}

#define glsl_set_uniform_sampler1D(id,v) glUniform1i(id,v)
#define glsl_set_uniform_sampler2D(id,v) glUniform1i(id,v)
#define glsl_set_uniform_float(id,v) glUniform1f(id,v)
#define glsl_set_uniform_vec2(id,v) glUniform2fv(id,1,v)
#define glsl_set_uniform_vec3(id,v) glUniform3fv(id,1,v)
#define glsl_set_uniform_vec4(id,v) glUniform4fv(id,1,v)
#define glsl_set_uniform_mat4(id,v) glUniformMatrix4fv(id,1,GL_FALSE,v)
#define glsl_set_attrib_vec2(id,stride,v) glVertexAttribPointer(id, 2, GL_FLOAT, GL_FALSE, stride, v)
#define glsl_set_attrib_vec3(id,stride,v) glVertexAttribPointer(id, 3, GL_FLOAT, GL_FALSE, stride, v)
#define glsl_set_attrib_vec4(id,stride,v) glVertexAttribPointer(id, 4, GL_FLOAT, GL_FALSE, stride, v)
#define glsl_enable_attrib(id) glEnableVertexAttribArray(id)
#define glsl_disable_attrib(id) glDisableVertexAttribArray(id)


#else
gboolean glsl_is_supported()
{
    return FALSE;
}

const char *glsl_get_version()
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

gl_program_t glsl_create_program( gl_shader_t *shaderv )
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

static inline GLint glsl_get_uniform_location(gl_program_t program, const char *name)
{
    return 0;
}

static inline GLint glsl_get_attrib_location(gl_program_t program, const char *name)
{
    return 0;
}

#define glsl_set_uniform_sampler1D(id,v)
#define glsl_set_uniform_sampler2D(id,v)
#define glsl_set_uniform_float(id,v)
#define glsl_set_uniform_vec2(id,v)
#define glsl_set_uniform_vec3(id,v)
#define glsl_set_uniform_vec4(id,v)
#define glsl_set_uniform_mat4(id,v)
#define glsl_set_attrib_vec2(id,stride,v)
#define glsl_set_attrib_vec3(id,stride,v)
#define glsl_set_attrib_vec4(id,stride,v)
#define glsl_enable_attrib(id)
#define glsl_disable_attrib(id)


#endif

/****************************************************************************/

program_cleanup_fn_t current_cleanup_fn = NULL;

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
    
    glsl_init_programs(program_array);
    glsl_use_program(0);
    return TRUE;
}

static void glsl_set_cleanup_fn( program_cleanup_fn_t fn )
{
    if( fn != current_cleanup_fn ) {
        if( current_cleanup_fn != NULL ) {
            current_cleanup_fn();
        }
        current_cleanup_fn = fn;
    }
}

static void glsl_run_cleanup_fn()
{
    if( current_cleanup_fn ) {
        current_cleanup_fn();
    }
    current_cleanup_fn = NULL;
}

void glsl_unload_shaders()
{
    unsigned i;
    glsl_run_cleanup_fn();
    for( i=0; i<GLSL_NUM_PROGRAMS; i++ ) {
        if( program_array[i] != INVALID_PROGRAM ) {
            glsl_destroy_program(program_array[i]);
            program_array[i] = INVALID_PROGRAM;
        }
    }
}

void glsl_clear_shader()
{
    glsl_run_cleanup_fn();
    glsl_use_program(0);
}

