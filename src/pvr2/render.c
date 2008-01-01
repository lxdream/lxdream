/**
 * $Id$
 *
 * PVR2 Renderer support. This part is primarily
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

#include <sys/time.h>
#include <time.h>
#include "pvr2/pvr2.h"
#include "asic.h"


int pvr2_render_trace = 0;

#if 0
int pvr2_render_font_list = -1;
int glPrintf( int x, int y, const char *fmt, ... )
{
    va_list ap;     /* our argument pointer */
    char buf[256];
    int len;
    if (fmt == NULL)    /* if there is no string to draw do nothing */
        return 0;
    va_start(ap, fmt); 
    len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);


    glPushAttrib(GL_LIST_BIT);
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_BLEND );
    glDisable( GL_TEXTURE_2D );
    glDisable( GL_ALPHA_TEST );
    glDisable( GL_CULL_FACE );
    glListBase(pvr2_render_font_list - 32);
    glColor3f( 1.0, 1.0, 1.0 );
    glRasterPos2i( x, y );
    glCallLists(len, GL_UNSIGNED_BYTE, buf);
    glPopAttrib();

    return len;
}
#endif

void glDrawGrid( int width, int height )
{
    int i;
    glDisable( GL_DEPTH_TEST );
    glLineWidth(1);
    
    glBegin( GL_LINES );
    glColor4f( 1.0, 1.0, 1.0, 1.0 );
    for( i=32; i<width; i+=32 ) {
	glVertex3f( i, 0.0, 3.0 );
	glVertex3f( i,height-1, 3.0 );
    }

    for( i=32; i<height; i+=32 ) {
	glVertex3f( 0.0, i, 3.0 );
	glVertex3f( width, i, 3.0 );
    }
    glEnd();
	
}

/**
 * Prepare the OpenGL context to receive instructions for a new frame.
 */
static void pvr2_render_prepare_context( render_buffer_t buffer,
					 float bgplanez, float nearz ) 
{
    /* Select and initialize the render context */
    display_driver->set_render_target(buffer);
#if 0
    if( pvr2_render_font_list == -1 ) {
	pvr2_render_font_list = video_glx_load_font( "-*-helvetica-*-r-normal--16-*-*-*-p-*-iso8859-1");
    }
#endif
    pvr2_check_palette_changed();

    /* Setup the display model */
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( 0, buffer->width, buffer->height, 0, -bgplanez, -nearz );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glCullFace( GL_BACK );
    glEnable( GL_BLEND );

    /* Clear out the buffers */
    glDisable( GL_SCISSOR_TEST );
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(0);
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glEnableClientState( GL_COLOR_ARRAY );
    glEnableClientState( GL_VERTEX_ARRAY );
}

/**
 * Render a complete scene into the OpenGL back buffer.
 * Note: this will probably need to be broken up eventually once timings are
 * determined.
 */
void pvr2_render_scene( render_buffer_t buffer )
{
    struct timeval tva, tvb;

    gettimeofday(&tva, NULL);

    float bgplanez = 1/MMIO_READF( PVR2, RENDER_FARCLIP );
    pvr2_render_prepare_context( buffer, bgplanez, 0 );

    int clip_x = MMIO_READ( PVR2, RENDER_HCLIP ) & 0x03FF;
    int clip_y = MMIO_READ( PVR2, RENDER_VCLIP ) & 0x03FF;
    int clip_width = ((MMIO_READ( PVR2, RENDER_HCLIP ) >> 16) & 0x03FF) - clip_x + 1;
    int clip_height= ((MMIO_READ( PVR2, RENDER_VCLIP ) >> 16) & 0x03FF) - clip_y + 1;

    /* Fog setup goes here? */

    /* Render the background plane */

    uint32_t bgplane_mode = MMIO_READ(PVR2, RENDER_BGPLANE);
    uint32_t *display_list = 
	(uint32_t *)mem_get_region(PVR2_RAM_BASE + MMIO_READ( PVR2, RENDER_POLYBASE ));

    uint32_t *bgplane = display_list + (((bgplane_mode & 0x00FFFFFF)) >> 3) ;
    render_backplane( bgplane, buffer->width, buffer->height, bgplane_mode );
    
    pvr2_render_tilebuffer( buffer->width, buffer->height, clip_x, clip_y, 
			    clip_x + clip_width, clip_y + clip_height );

    gettimeofday( &tvb, NULL );
    uint32_t ms = (tvb.tv_sec - tva.tv_sec) * 1000 + 
	(tvb.tv_usec - tva.tv_usec)/1000;
    DEBUG( "Rendered frame %d to %08X in %dms", pvr2_get_frame_count(), buffer->address, ms );
}
