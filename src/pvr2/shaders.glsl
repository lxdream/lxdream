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

/**
 * Quick reference for predefined variables
 
 * Vertex shader input variables:
 *   vec4 gl_Color;
 *   vec4 gl_SecondaryColor;
 *   vec3 gl_Normal;
 *   vec4 gl_Vertex;
 *   vec4 gl_MultiTexCoord0;
 *   vec4 gl_MultiTexCoord1; 
 *   vec4 gl_MultiTexCoord2;
 *   vec4 gl_MultiTexCoord3; 
 *   vec4 gl_MultiTexCoord4;
 *   vec4 gl_MultiTexCoord5;
 *   vec4 gl_MultiTexCoord6;
 *   vec4 gl_MultiTexCoord7;
 *   float gl_FogCoord;
 *
 * Vertex shader output variables:
 *   vec4 gl_Position;    // must be written to
 *   float gl_PointSize;  // may be written to
 *   vec4 gl_ClipVertex;  // may be written to
 *   varying vec4 gl_FrontColor; 
 *   varying vec4 gl_BackColor; 
 *   varying vec4 gl_FrontSecondaryColor; 
 *   varying vec4 gl_BackSecondaryColor; 
 *   varying vec4 gl_TexCoord[]; // at most will be gl_MaxTextureCoords
 *   varying float gl_FogFragCoord;
 *
 * Fragment shader input variables:
 *   varying vec4 gl_Color; 
 *   varying vec4 gl_SecondaryColor; 
 *   varying vec4 gl_TexCoord[]; // at most will be gl_MaxTextureCoords
 *   varying float gl_FogFragCoord;
 *   varying vec2 gl_PointCoord;
 *
 * Fragme shader output variables:
 *   vec4 gl_FragCoord; 
 *   bool gl_FrontFacing; 
 *   vec4 gl_FragColor; 
 *   vec4 gl_FragData[gl_MaxDrawBuffers]; 
 *   float gl_FragDepth;

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

uniform sampler2D primary_texture;
uniform sampler1D palette_texture;

void main()
{
	vec4 tex = texture2D( primary_texture, gl_TexCoord[0].xy );
	if( gl_TexCoord[0].z >= 0.0 ) {
	    tex = texture1D( palette_texture, gl_TexCoord[0].z + (tex.a*0.249023) );
	}
	/* HACK: unfortunately we have to maintain compatibility with GLSL 1.20,
	 * which only supports varying float. So since we're propagating texcoord
	 * anyway, overload the last component to indicate texture mode. 
	 */
	if( gl_TexCoord[0].w == 0.0 ) {
	    gl_FragColor.rgb = mix( gl_Color.rgb * tex.rgb + gl_SecondaryColor.rgb, gl_Fog.color.rgb, gl_FogFragCoord );
	    gl_FragColor.a = gl_Color.a * tex.a;
	} else if( gl_TexCoord[0].w >= 1.5 ) {
	    gl_FragColor.rgb = mix( gl_Color.rgb, gl_Fog.color.rgb, gl_FogFragCoord );
	    gl_FragColor.a = gl_Color.a;
	} else {
	    gl_FragColor.rgb = mix( mix(gl_Color.rgb,tex.rgb,tex.a) + gl_SecondaryColor.rgb, gl_Fog.color.rgb, gl_FogFragCoord);
	    gl_FragColor.a = gl_Color.a;
	}
	gl_FragDepth = gl_FragCoord.z;
}

#program DEFAULT_PROGRAM = DEFAULT_VERTEX_SHADER DEFAULT_FRAGMENT_SHADER

