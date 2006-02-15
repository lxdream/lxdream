/**
 * $Id: render.c,v 1.1 2006-02-15 13:11:46 nkeynes Exp $
 *
 * PVR2 Renderer support. This is where the real work happens.
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
#include "dream.h"

extern uint32_t pvr2_frame_counter;

/**
 * Render a complete scene into an OpenGL buffer.
 * Note: this may need to be broken up eventually once timings are
 * determined.
 */
void pvr2_render_scene( void )
{
    /* Actual rendering goes here :) */

    /* End of render event */
    asic_event( EVENT_PVR_RENDER_DONE );
    DEBUG( "Rendered frame %d", pvr2_frame_counter );
}
