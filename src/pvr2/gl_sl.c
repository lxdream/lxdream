/**
 * $Id$
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

#include "lxdream.h"
#include "display.h"
#include "pvr2/glutil.h"

#define MAX_ERROR_BUF 4096

gboolean glsl_is_supported()
{
    return isGLExtensionSupported("GL_ARB_fragment_shader") &&
	isGLExtensionSupported("GL_ARB_vertex_shader") &&
	isGLExtensionSupported("GL_ARB_shading_language_100");
}

#ifdef GL_ARB_shader_objects
static GLhandleARB glsl_program = 0, glsl_vert_shader = 0, glsl_frag_shader = 0;

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
    gboolean vsok = TRUE, fsok = TRUE, pok = FALSE;

    if( vertex_src == NULL && fragment_src == NULL ) {
	return TRUE; // nothing to do
    }

    glsl_program = glCreateProgramObjectARB();

    if( vertex_src != NULL ) {
	glsl_vert_shader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	glShaderSourceARB( glsl_vert_shader, 1, &vertex_src, NULL );
	glCompileShaderARB(glsl_vert_shader);
	vsok = glsl_check_shader_error("Failed to compile vertex shader", glsl_vert_shader);
    }
    if( fragment_src != NULL ) {
	glsl_frag_shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	glShaderSourceARB( glsl_frag_shader, 1, &fragment_src, NULL );
	glCompileShaderARB(glsl_frag_shader);
	fsok = glsl_check_shader_error("Failed to compile fragment shader", glsl_frag_shader);
    }

    if( vsok && fsok ) {
	if( vertex_src != NULL ) {
	    glAttachObjectARB(glsl_program, glsl_vert_shader);
	}
	if( fragment_src != NULL ) {
	    glAttachObjectARB(glsl_program, glsl_frag_shader);
	}
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

void glsl_enable_shaders(gboolean en)
{
    if( glsl_program != 0 ) {
	if( en ) {
	    glUseProgramObjectARB(glsl_program);
	} else {
	    glUseProgramObjectARB(0);
	}
    }
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

#elif HAVE_OPENGL_SHADER
static GLuint glsl_program = 0, glsl_vert_shader = 0, glsl_frag_shader = 0;

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
    gboolean vsok = TRUE, fsok = TRUE, pok = FALSE;

    if( vertex_src == NULL && fragment_src == NULL ) {
	return TRUE;
    }

    glsl_program = glCreateProgram();

    if( vertex_src != NULL ) {
	glsl_vert_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource( glsl_vert_shader, 1, &vertex_src, NULL );
	glCompileShader(glsl_vert_shader);
	vsok = glsl_check_shader_error( "Failed to compile vertex shader", glsl_vert_shader );
    }
    if( fragment_src != NULL ) {
	glsl_frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource( glsl_frag_shader, 1, &fragment_src, NULL );
	glCompileShader(glsl_frag_shader);
	fsok = glsl_check_shader_error( "Failed to compile fragment shader", glsl_frag_shader );
    }

    if( vsok && fsok ) {
	if( vertex_src != NULL ) {
	    glAttachShader(glsl_program, glsl_vert_shader);
	}
	if( fragment_src != NULL ) {
	    glAttachShader(glsl_program, glsl_frag_shader);
	}
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


void glsl_enable_shaders(gboolean en)
{
    if( glsl_program != 0 ) {
	if( en ) {
	    glUseProgram(glsl_program);
	} else {
	    glUseProgram(0);
	}
    }
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

#else
gboolean glsl_load_shaders( const char *vertex_src, const char *fragment_src )
{
    return FALSE;
}

void glsl_unload_shaders()
{
}

void glsl_enable_shaders( gboolean enable )
{
}
#endif
