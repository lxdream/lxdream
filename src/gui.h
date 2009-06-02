/**
 * $Id$
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

#ifndef lxdream_gui_H
#define lxdream_gui_H

#include <glib/gtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Base GUI clock is 10ms */
#define GUI_TICK_PERIOD 10000000

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
 * @param fullscreen TRUE if the system should start in fullscreen mode.
 */
gboolean gui_init( gboolean debug, gboolean fullscreen );

/**
 * Enter the GUI main loop. If this method ever returns, the system will
 * exit normally.
 * 
 * @param run TRUE if the system should start running immediately, otherwise
 */
void gui_main_loop( gboolean run );

gboolean gui_error_dialog( const char *fmt, ... );

typedef enum { IO_IDE, IO_NETWORK } io_activity_type;

/**
 * Notify the GUI of state changes (eg binary was loaded and PC changed)
 */
void gui_update_state();

/**
 * Notify the GUI to enable/disable mouse grabs according to the flag value.
 * If the parameter is FALSE and the grab is currently active, the GUI should
 * immediately cancel the grab.
 */
void gui_set_use_grab( gboolean grab );

/**
 * Notify the GUI of I/O activity. 
 * @param activity the type of IO activity being reported.
 * @param active TRUE if the I/O device is becoming active, FALSE if inactive.
 */
void gui_update_io_activity( io_activity_type activity, gboolean active );

/**
 * Queue an event to call dreamcast_run() at the next opportunity (used to
 * avoid invoking dreamcast_run() directly from the middle of things. 
 */
void gui_run_later(void);

#ifdef __cplusplus
}
#endif

#endif /* lxdream_gui_H */
