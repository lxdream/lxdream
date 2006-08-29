/**
 * $Id: render.c,v 1.13 2006-08-29 08:12:13 nkeynes Exp $
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

#include "pvr2/pvr2.h"
#include "asic.h"

static int pvr2_render_colour_format[8] = {
    COLFMT_ARGB1555, COLFMT_RGB565, COLFMT_ARGB4444, COLFMT_ARGB1555,
    COLFMT_RGB888, COLFMT_ARGB8888, COLFMT_ARGB8888, COLFMT_ARGB4444 };


/**
 * Describes a rendering buffer that's actually held in GL, for when we need
 * to fetch the bits back to vram.
 */
typedef struct pvr2_render_buffer {
    sh4addr_t render_addr; /* The actual address rendered to in pvr ram */
    uint32_t size; /* Length of rendering region in bytes */
    int width, height;
    int colour_format;
} *pvr2_render_buffer_t;

struct pvr2_render_buffer front_buffer;
struct pvr2_render_buffer back_buffer;

typedef struct pvr2_bgplane_packed {
        uint32_t        poly_cfg, poly_mode;
        uint32_t        texture_mode;
        float           x1, y1, z1;
        uint32_t          colour1;
        float           x2, y2, z2;
        uint32_t          colour2;
        float           x3, y3, z3;
        uint32_t          colour3;
} *pvr2_bgplane_packed_t;



void pvr2_render_copy_to_sh4( pvr2_render_buffer_t buffer, 
			      gboolean backBuffer );

int pvr2_render_font_list = -1;
int pvr2_render_trace = 0;

int glPrintf( int x, int y, const char *fmt, ... )
{
    va_list ap;     /* our argument pointer */
    char buf[256];
    int len;
    if (fmt == NULL)    /* if there is no string to draw do nothing */
        return;
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


gboolean pvr2_render_init( void )
{
    front_buffer.render_addr = -1;
    back_buffer.render_addr = -1;
}

/**
 * Invalidate any caching on the supplied address. Specifically, if it falls
 * within either the front buffer or back buffer, flush the buffer back to
 * PVR2 ram (note that front buffer flush may be corrupt under some
 * circumstances).
 */
gboolean pvr2_render_invalidate( sh4addr_t address )
{
    address = address & 0x1FFFFFFF;
    if( front_buffer.render_addr != -1 &&
	front_buffer.render_addr <= address &&
	(front_buffer.render_addr + front_buffer.size) > address ) {
	pvr2_render_copy_to_sh4( &front_buffer, FALSE );
	front_buffer.render_addr = -1;
	return TRUE;
    } else if( back_buffer.render_addr != -1 &&
	       back_buffer.render_addr <= address &&
	       (back_buffer.render_addr + back_buffer.size) > address ) {
	pvr2_render_copy_to_sh4( &back_buffer, TRUE );
	back_buffer.render_addr = -1;
	return TRUE;
    }
    return FALSE;
}

/**
 * Display a rendered frame if one is available.
 * @param address An address in PVR ram (0500000 range).
 * @return TRUE if a frame was available to be displayed, otherwise false.
 */
gboolean pvr2_render_display_frame( uint32_t address )
{
    if( front_buffer.render_addr == address ) {
	/* Current front buffer is already displayed, so do nothing
	 * and tell the caller that all is well.
	 */
	return TRUE;
    }
    if( back_buffer.render_addr == address ) {
	/* The more useful case - back buffer is to be displayed. Swap
	 * the buffers 
	 */
	display_driver->display_back_buffer();
	front_buffer = back_buffer;
	back_buffer.render_addr = -1;
	return TRUE;
    }
    return FALSE;
}	

/**
 * Prepare the OpenGL context to receive instructions for a new frame.
 */
static void pvr2_render_prepare_context( sh4addr_t render_addr, 
					 uint32_t width, uint32_t height,
					 uint32_t colour_format, 
					 float bgplanez,
					 gboolean texture_target )
{
    /* Select and initialize the render context */
    display_driver->set_render_format( width, height, colour_format, texture_target );

    if( pvr2_render_font_list == -1 ) {
	pvr2_render_font_list = video_glx_load_font( "-*-helvetica-*-r-normal--16-*-*-*-p-*-iso8859-1");
    }

    if( back_buffer.render_addr != -1 && 
	back_buffer.render_addr != render_addr ) {
	/* There's a current back buffer, and we're rendering somewhere else -
	 * flush the back buffer back to vram and start a new back buffer
	 */
	pvr2_render_copy_to_sh4( &back_buffer, TRUE );
    }

    if( front_buffer.render_addr == render_addr ) {
	/* In case we've been asked to render to the current front buffer -
	 * invalidate the front buffer and render to the back buffer, ensuring
	 * we swap at the next frame display.
	 */
	front_buffer.render_addr = -1;
    }
    back_buffer.render_addr = render_addr;
    back_buffer.width = width;
    back_buffer.height = height;
    back_buffer.colour_format = colour_format;
    back_buffer.size = width * height * colour_format_bytes[colour_format];

    /* Setup the display model */
    glDrawBuffer(GL_BACK);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glViewport( 0, 0, width, height );
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( 0, width, height, 0, bgplanez, -4 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glCullFace( GL_BACK );

    /* Clear out the buffers */
    glDisable( GL_SCISSOR_TEST );
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(bgplanez);
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
}


#define MIN3( a,b,c ) ((a) < (b) ? ( (a) < (c) ? (a) : (c) ) : ((b) < (c) ? (b) : (c)) )
#define MAX3( a,b,c ) ((a) > (b) ? ( (a) > (c) ? (a) : (c) ) : ((b) > (c) ? (b) : (c)) )

/**
 * Render a complete scene into the OpenGL back buffer.
 * Note: this will probably need to be broken up eventually once timings are
 * determined.
 */
void pvr2_render_scene( )
{
    struct tile_descriptor *tile_desc =
	(struct tile_descriptor *)mem_get_region(PVR2_RAM_BASE + MMIO_READ( PVR2, RENDER_TILEBASE ));

    uint32_t render_addr = MMIO_READ( PVR2, RENDER_ADDR1 );
    gboolean render_to_tex;
    if( render_addr & 0x01000000 ) {
	render_addr = (render_addr & 0x00FFFFFF) + PVR2_RAM_BASE_INT;
	/* Heuristic - if we're rendering to the interlaced region we're
	 * probably creating a texture rather than rendering actual output.
	 * We can optimise for this case a little
	 */
	render_to_tex = TRUE;
	WARN( "Render to texture not supported properly yet" );
    } else {
	render_addr = (render_addr & 0x00FFFFFF) + PVR2_RAM_BASE;
	render_to_tex = FALSE;
    }
    
    float bgplanez = MMIO_READF( PVR2, RENDER_FARCLIP );
    uint32_t render_mode = MMIO_READ( PVR2, RENDER_MODE );
    int width = 640; /* FIXME - get this from the tile buffer */
    int height = 480;
    int colour_format = pvr2_render_colour_format[render_mode&0x07];
    pvr2_render_prepare_context( render_addr, width, height, colour_format, 
				 bgplanez, render_to_tex );

    int clip_x = MMIO_READ( PVR2, RENDER_HCLIP ) & 0x03FF;
    int clip_y = MMIO_READ( PVR2, RENDER_VCLIP ) & 0x03FF;
    int clip_width = ((MMIO_READ( PVR2, RENDER_HCLIP ) >> 16) & 0x03FF) - clip_x + 1;
    int clip_height= ((MMIO_READ( PVR2, RENDER_VCLIP ) >> 16) & 0x03FF) - clip_y + 1;

    /* Fog setup goes here */

    /* Render the background plane */
    uint32_t bgplane_mode = MMIO_READ(PVR2, RENDER_BGPLANE);
    uint32_t *display_list = 
	(uint32_t *)mem_get_region(PVR2_RAM_BASE + MMIO_READ( PVR2, RENDER_POLYBASE ));

    uint32_t *bgplane = display_list + (((bgplane_mode & 0x00FFFFFF)) >> 3) ;
    render_backplane( bgplane, width, height, bgplane_mode );

    pvr2_render_tilebuffer( width, height, clip_x, clip_y, 
			    clip_x + clip_width, clip_y + clip_height );

    /* Post-render cleanup and update */

    /* Add frame, fps, etc data */
    //glDrawGrid( width, height );
    glPrintf( 4, 16, "Frame %d", pvr2_get_frame_count() );
    /* Generate end of render event */
    asic_event( EVENT_PVR_RENDER_DONE );
    DEBUG( "Rendered frame %d", pvr2_get_frame_count() );
}


/**
 * Flush the indicated render buffer back to PVR. Caller is responsible for
 * tracking whether there is actually anything in the buffer.
 *
 * @param buffer A render buffer indicating the address to store to, and the
 * format the data needs to be in.
 * @param backBuffer TRUE to flush the back buffer, FALSE for 
 * the front buffer.
 */
void pvr2_render_copy_to_sh4( pvr2_render_buffer_t buffer, 
			      gboolean backBuffer )
{
    if( buffer->render_addr == -1 )
	return;
    GLenum type, format = GL_RGBA;
    int line_size = buffer->width, size;

    switch( buffer->colour_format ) {
    case COLFMT_RGB565: 
	type = GL_UNSIGNED_SHORT_5_6_5; 
	format = GL_RGB; 
	line_size <<= 1;
	break;
    case COLFMT_RGB888: 
	type = GL_UNSIGNED_INT; 
	format = GL_RGB;
	line_size = (line_size<<1)+line_size;
	break;
    case COLFMT_ARGB1555: 
	type = GL_UNSIGNED_SHORT_5_5_5_1; 
	line_size <<= 1;
	break;
    case COLFMT_ARGB4444: 
	type = GL_UNSIGNED_SHORT_4_4_4_4; 
	line_size <<= 1;
	break;
    case COLFMT_ARGB8888: 
	type = GL_UNSIGNED_INT_8_8_8_8; 
	line_size <<= 2;
	break;
    }
    size = line_size * buffer->height;
    
    if( backBuffer ) {
	glFinish();
	glReadBuffer( GL_BACK );
    } else {
	glReadBuffer( GL_FRONT );
    }

    if( buffer->render_addr & 0xFF000000 == 0x04000000 ) {
	/* Interlaced buffer. Go the double copy... :( */
	char target[size];
	glReadPixels( 0, 0, buffer->width, buffer->height, format, type, target );
	pvr2_vram64_write( buffer->render_addr, target, size );
    } else {
	/* Regular buffer */
	char target[size];
	glReadPixels( 0, 0, buffer->width, buffer->height, format, type, target );
	pvr2_vram_write_invert( buffer->render_addr, target, size, line_size );
    }
}


/**
 * Copy data from PVR ram into the GL render buffer. 
 *
 * @param buffer A render buffer indicating the address to read from, and the
 * format the data is in.
 * @param backBuffer TRUE to write the back buffer, FALSE for 
 * the front buffer.
 */
void pvr2_render_copy_from_sh4( pvr2_render_buffer_t buffer, 
				gboolean backBuffer )
{
    if( buffer->render_addr == -1 )
	return;
    GLenum type, format = GL_RGBA;
    int size = buffer->width * buffer->height;

    switch( buffer->colour_format ) {
    case COLFMT_RGB565: 
	type = GL_UNSIGNED_SHORT_5_6_5; 
	format = GL_RGB; 
	size <<= 1;
	break;
    case COLFMT_RGB888: 
	type = GL_UNSIGNED_INT; 
	format = GL_RGB;
	size = (size<<1)+size;
	break;
    case COLFMT_ARGB1555: 
	type = GL_UNSIGNED_SHORT_5_5_5_1; 
	size <<= 1;
	break;
    case COLFMT_ARGB4444: 
	type = GL_UNSIGNED_SHORT_4_4_4_4; 
	size <<= 1;
	break;
    case COLFMT_ARGB8888: 
	type = GL_UNSIGNED_INT_8_8_8_8; 
	size <<= 2;
	break;
    }
    
    if( backBuffer ) {
	glDrawBuffer( GL_BACK );
    } else {
	glDrawBuffer( GL_FRONT );
    }

    glRasterPos2i( 0, 0 );
    if( buffer->render_addr & 0xFF000000 == 0x04000000 ) {
	/* Interlaced buffer. Go the double copy... :( */
	char target[size];
	pvr2_vram64_read( target, buffer->render_addr, size );
	glDrawPixels( buffer->width, buffer->height, 
		      format, type, target );
    } else {
	/* Regular buffer - go direct */
	char *target = mem_get_region( buffer->render_addr );
	glDrawPixels( buffer->width, buffer->height, 
		      format, type, target );
    }
}
