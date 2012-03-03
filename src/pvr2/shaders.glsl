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
#include "../config.h"

#vertex DEFAULT_VERTEX_SHADER
uniform mat4 view_matrix;
attribute vec4 in_vertex;
attribute vec4 in_colour;
attribute vec4 in_colour2; /* rgb = colour, a = fog */
attribute vec4 in_texcoord; /* uv = coord, z = palette, w = mode */

varying vec4 frag_colour;
varying vec4 frag_colour2;
varying vec4 frag_texcoord;
void main()
{
    vec4 tmp = view_matrix * in_vertex;
    float w = in_vertex.z;
    gl_Position  = tmp * w;
    frag_colour = in_colour;
    frag_colour2 = in_colour2;
    frag_texcoord = in_texcoord;
}

#fragment DEFAULT_FRAGMENT_SHADER
precision mediump float;
uniform float alpha_ref;
uniform sampler2D primary_texture;
uniform sampler2D palette_texture;
uniform vec3 fog_colour1;
uniform vec3 fog_colour2;
varying vec4 frag_colour;
varying vec4 frag_colour2;
varying vec4 frag_texcoord;

void main()
{
	vec4 tex = texture2D( primary_texture, frag_texcoord.xy );
	if( frag_texcoord.z >= 0.0 ) {
	    tex = texture2D( palette_texture, vec2(frag_texcoord.z + (tex.a*0.249023), 0.5) );
	}
	/* HACK: unfortunately we have to maintain compatibility with GLSL 1.20,
	 * which only supports varying float. So since we're propagating texcoord
	 * anyway, overload the last component to indicate texture mode. 
	 */
        vec3 main_colour;
	if( frag_texcoord.w == 0.0 ) {
            main_colour = frag_colour.rgb * tex.rgb + frag_colour2.rgb;
	    gl_FragColor.a = frag_colour.a * tex.a;
	} else if( frag_texcoord.w >= 1.5 ) {
            main_colour = frag_colour.rgb;
	    gl_FragColor.a = frag_colour.a;
	} else {
	    main_colour =  mix(frag_colour.rgb,tex.rgb,tex.a) + frag_colour2.rgb;
	    gl_FragColor.a = frag_colour.a;
	}
        if( gl_FragColor.a < alpha_ref ) {
            discard;
        } else { 
   	    if( frag_colour2.a >= 0.0 ) {
                gl_FragColor.rgb = mix( main_colour, fog_colour1, frag_colour2.a );
            } else {
                gl_FragColor.rgb = mix( main_colour, fog_colour2, -frag_colour2.a );
            }
        } 
}

#program pvr2_shader = DEFAULT_VERTEX_SHADER DEFAULT_FRAGMENT_SHADER

#ifndef HAVE_OPENGL_FIXEDFUNC
/* In this case we also need a basic shader to actually display the output */
#vertex BASIC_VERTEX_SHADER
uniform mat4 view_matrix;
attribute vec2 in_vertex;
attribute vec4 in_colour;
attribute vec2 in_texcoord; /* uv = coord, z = palette, w = mode */

varying vec4 frag_colour;
varying vec2 frag_texcoord;
void main()
{
    gl_Position = view_matrix * vec4(in_vertex.x,in_vertex.y,0.0,1.0);
    frag_colour = in_colour;
    frag_texcoord = in_texcoord;
}

#fragment BASIC_FRAGMENT_SHADER
precision mediump float;
uniform sampler2D primary_texture;
varying vec4 frag_colour;
varying vec2 frag_texcoord;

void main()
{
	vec4 tex = texture2D( primary_texture, frag_texcoord.xy );
        gl_FragColor.rgb = mix( frag_colour.rgb, tex.rgb, frag_colour.a );
}

#program basic_shader = BASIC_VERTEX_SHADER BASIC_FRAGMENT_SHADER
#endif
