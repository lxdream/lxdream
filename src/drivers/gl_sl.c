/**
 * $Id: gl_sl.c,v 1.3 2007-10-31 09:10:23 nkeynes Exp $
 *
 * GLSL shader loader/unloader. Current version assumes there's exactly
 * 1 shader program that's used globally. This may turn out not to be the
 * most efficient approach.
 *
 * Copyright (c) 2007 Nathan Keynes.
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

#define GL_GLEXT_PROTOTYPES 1

#include "lxdream.h"
#include "display.h"
#include "drivers/gl_common.h"

#define MAX_ERROR_BUF 4096

gboolean glsl_is_supported()
{
    return isGLExtensionSupported("GL_ARB_fragment_shader") &&
	isGLExtensionSupported("GL_ARB_vertex_shader") &&
	isGLExtensionSupported("GL_ARB_shading_language_100");
}

#ifdef GL_ARB_shader_objects
static GLhandleARB glsl_program, glsl_vert_shader, glsl_frag_shader;

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


gboolean glsl_load_shaders( const char *vertex_src, const char *fragment_src )
{
    gboolean vsok, fsok, pok = FALSE;
    glsl_program = glCreateProgramObjectARB();

    glsl_vert_shader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    glsl_frag_shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
    glShaderSourceARB( glsl_vert_shader, 1, &vertex_src, NULL );
    glCompileShaderARB(glsl_vert_shader);
    vsok = glsl_check_shader_error("Failed to compile vertex shader", glsl_vert_shader);
    glShaderSourceARB( glsl_frag_shader, 1, &fragment_src, NULL );
    glCompileShaderARB(glsl_frag_shader);
    fsok = glsl_check_shader_error("Failed to compile fragment shader", glsl_frag_shader);

    if( vsok && fsok ) {
	glAttachObjectARB(glsl_program, glsl_vert_shader);
	glAttachObjectARB(glsl_program, glsl_frag_shader);
	glLinkProgramARB(glsl_program);
	pok = glsl_check_program_error( "Failed to link shader program", glsl_program );
    }
    if( pok ) {
	glUseProgramObjectARB(glsl_program);
	pok = glsl_check_program_error( "Failed to apply shader program", glsl_program );
    } else {
	glsl_unload_shaders();
    }
    return pok;
}

void glsl_unload_shaders(void)
{
    glUseProgramObjectARB(0);
    glDetachObjectARB(glsl_program, glsl_vert_shader);
    glDetachObjectARB(glsl_program, glsl_frag_shader);
    glDeleteObjectARB(glsl_program);
    glDeleteObjectARB(glsl_vert_shader);
    glDeleteObjectARB(glsl_frag_shader);
}

#else
static GLuint glsl_program, glsl_vert_shader, glsl_frag_shader;

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

gboolean glsl_load_shaders( const char *vertex_src, const char *fragment_src )
{
    gboolean vsok, fsok, pok = FALSE;
    glsl_program = glCreateProgram();
    glsl_vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glsl_frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource( glsl_vert_shader, 1, &vertex_src, NULL );
    glCompileShader(glsl_vert_shader);
    vsok = glsl_check_shader_error( "Failed to compile vertex shader", glsl_vert_shader );
    glShaderSource( glsl_frag_shader, 1, &fragment_src, NULL );
    glCompileShader(glsl_frag_shader);
    fsok = glsl_check_shader_error( "Failed to compile fragment shader", glsl_frag_shader );

    if( vsok && fsok ) {
	glAttachShader(glsl_program, glsl_vert_shader);
	glAttachShader(glsl_program, glsl_frag_shader);
	glLinkProgram(glsl_program);
	pok = glsl_check_program_error( "Failed to link shader program", glsl_program );
    }

    if( pok ) {
	glUseProgram(glsl_program);
    } else {
	glsl_unload_shaders();
    }
    return pok;
}

void glsl_unload_shaders(void)
{
    glUseProgram(0);
    glDetachShader(glsl_program, glsl_vert_shader);
    glDetachShader(glsl_program, glsl_frag_shader);
    glDeleteProgram(glsl_program);
    glDeleteShader(glsl_vert_shader);
    glDeleteShader(glsl_frag_shader);
}
#endif
