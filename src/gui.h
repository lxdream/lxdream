/**
 * $Id: gui.h,v 1.2 2007-10-16 12:36:29 nkeynes Exp $
 *
 * Public GUI declarations (used from elsewhere in the system)
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

#ifndef __lxdream_gui_H
#define __lxdream_gui_H

#include <glib/gtypes.h>

/**
 * GUI-provided method to scan the command line for standard arguments,
 * invoked prior to regular command line processing. The command line
 * is modified to remove any arguments handled by the UI.
 * @return TRUE on success, FALSE on failure.
 */
gboolean gui_parse_cmdline( int *argc, char **argv[] );

/**
 * Initialize the GUI system and create any windows needed. This method
 * should also register the GUI module with the module manager (if the
 * GUI has one).
 *
 * @param debug TRUE if the system should start in debugging mode.
 */
gboolean gui_init( gboolean debug );

/**
 * Enter the GUI main loop. If this method ever returns, the system will
 * exit normally.
 */
void gui_main_loop(void);

gboolean gui_error_dialog( const char *fmt, ... );

typedef enum { IO_IDE, IO_NETWORK } io_activity_type;

/**
 * Notify the GUI of I/O activity. 
 * @param activity the type of IO activity being reported.
 * @param active TRUE if the I/O device is becoming active, FALSE if inactive.
 */
void gui_update_io_activity( io_activity_type activity, gboolean active );

#endif /* __lxdream_gui_H */
