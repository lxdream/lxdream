/**
 * $Id$
 *
 * Assorted shader definitions (optionally) used by the PVR2 rendering
 * engine.
 * 
 * This file is preprocessed by genglsl to produce shaders.c and shaders.h.
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

#vertex DEFAULT_VERTEX_SHADER
void main()
{
    vec4 tmp = ftransform();
    float w = gl_Vertex.z;
    gl_Position  = tmp * w;
    gl_FrontColor = gl_Color;
    gl_FrontSecondaryColor = gl_SecondaryColor;
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_FogFragCoord = gl_FogCoord;
}

#fragment DEFAULT_FRAGMENT_SHADER
void main()
{
	gl_FragColor = gl_Color;
	gl_FragDepth = gl_FragCoord.z;
}

#program DEFAULT_PROGRAM = DEFAULT_VERTEX_SHADER

