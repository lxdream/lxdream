/**
 * $Id$
 *
 * GL-based support functions 
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
#include <string.h>
#include <glib/gstrfuncs.h>
#include "pvr2/glutil.h"

gboolean isGLSecondaryColorSupported()
{
	return isGLExtensionSupported("GL_EXT_secondary_color");
}

gboolean isGLVertexBufferSupported()
{
	return isGLExtensionSupported("GL_ARB_vertex_buffer_object");
}

gboolean isGLPixelBufferSupported()
{
	return isGLExtensionSupported("GL_ARB_pixel_buffer_object");
}

gboolean isGLMirroredTextureSupported()
{
	return isGLExtensionSupported("GL_ARB_texture_mirrored_repeat");
}

/**
 * Test if a specific extension is supported. From opengl.org
 * @param extension extension name to check for
 * @return TRUE if supported, otherwise FALSE.
 */
gboolean isGLExtensionSupported( const char *extension )
{
    const GLubyte *extensions = NULL;
    const GLubyte *start;
    GLubyte *where, *terminator;

    /* Extension names should not have spaces. */
    where = (GLubyte *) strchr(extension, ' ');
    if (where || *extension == '\0')
	return 0;
    extensions = glGetString(GL_EXTENSIONS);
    if( extensions == NULL ) {
	/* No GL available, so we're pretty sure the extension isn't
	 * available either. */
	return FALSE;
    }
    /* It takes a bit of care to be fool-proof about parsing the
       OpenGL extensions string. Don't be fooled by sub-strings,
       etc. */
    start = extensions;
    for (;;) {
	where = (GLubyte *) strstr((const char *) start, extension);
	if (!where)
	    break;
	terminator = where + strlen(extension);
	if (where == start || *(where - 1) == ' ')
	    if (*terminator == ' ' || *terminator == '\0')
		return TRUE;
	start = terminator;
    }
    return FALSE;
}

void glPrintInfo( FILE *out )
{
    const GLubyte *extensions = glGetString(GL_EXTENSIONS);
    gchar **ext_split = g_strsplit(extensions, " ", 0);
    unsigned int i;
    
    fprintf( out, "GL Vendor: %s\n", glGetString(GL_VENDOR) );
    fprintf( out, "GL Renderer: %s\n", glGetString(GL_RENDERER) );
    fprintf( out, "GL Version: %s\n", glGetString(GL_VERSION) );
    
    fprintf( out, "Supported GL Extensions:\n" );
    for( i=0; ext_split[i] != NULL; i++ ) {
        fprintf( out, "  %s\n", ext_split[i] );
    }
    g_strfreev(ext_split);
}
