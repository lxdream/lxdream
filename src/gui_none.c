/**
 * $Id$
 *
 * Dummy GUI implementation for headless systems.
 *
 * Copyright (c) 2012 Nathan Keynes.
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

#include "gui.h"

gboolean gui_parse_cmdline( int *argc, char **argv[] )
{
    return TRUE;
}

gboolean gui_init( gboolean debug, gboolean fullscreen )
{
    return TRUE;
}

void gui_main_loop( gboolean run ) {
    if( run ) {
        dreamcast_run();
    }
}

gboolean gui_error_dialog( const char *fmt, ... )
{
    return TRUE;
}

void gui_update_state()
{
}

void gui_set_use_grab( gboolean grab )
{
}

void gui_update_io_activity( io_activity_type activity, gboolean active )
{
}

void gui_do_later( do_later_callback_t func )
{
    func();
}

