/**
 * $Id$
 *
 * Core GTK-based user interface
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

#ifndef lxdream_gtkui_H
#define lxdream_gtkui_H 1

#include "lxdream.h"
#include <gtk/gtk.h>
#include "gettext.h"
#include "gui.h"
#include "cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************* Top-level windows *********************/

typedef struct main_window_info *main_window_t;
typedef struct debug_window_info *debug_window_t;
typedef struct mmio_window_info *mmio_window_t;
typedef struct dump_window_info *dump_window_t;

/**
 * Construct and show the main window, returning an 
 * opaque pointer to the window.
 */
main_window_t main_window_new( const gchar *title, GtkWidget *menubar, 
                               GtkWidget *toolbar, GtkAccelGroup *accel );
GtkWindow *main_window_get_frame( main_window_t win );
GtkWidget *main_window_get_renderarea( main_window_t win );
void main_window_set_running( main_window_t win, gboolean running );
void main_window_set_framerate( main_window_t win, float rate );
void main_window_set_speed( main_window_t win, double speed );
void main_window_set_fullscreen( main_window_t win, gboolean fullscreen );
void main_window_set_use_grab( main_window_t win, gboolean grab );

debug_window_t debug_window_new( const gchar *title, GtkWidget *menubar,
                                 GtkWidget *toolbar, GtkAccelGroup *accel );
void debug_window_show( debug_window_t win, gboolean show );
void debug_window_set_running( debug_window_t win, gboolean running );
void debug_window_update(debug_window_t win);
void debug_window_single_step( debug_window_t data );
void debug_window_set_oneshot_breakpoint( debug_window_t data, int row );
void debug_window_toggle_breakpoint( debug_window_t data, int row );


mmio_window_t mmio_window_new( const gchar *title );
void mmio_window_show( mmio_window_t win, gboolean show );
void mmio_window_update(mmio_window_t win);

dump_window_t dump_window_new( const gchar *title );
void dump_window_update_all();

void maple_dialog_run();
void path_dialog_run();

void gtk_gui_update( void );
main_window_t gtk_gui_get_main();
debug_window_t gtk_gui_get_debugger();
mmio_window_t gtk_gui_get_mmio();
void gtk_gui_show_mmio();
void gtk_gui_show_debugger();

/********************* Helper functions **********************/

typedef void (*gtk_dialog_done_fn)(GtkWidget *panel, gboolean isOK);
void gtk_gui_enable_action( const gchar *action, gboolean enabled );
gint gtk_gui_run_property_dialog( const gchar *title, GtkWidget *panel, gtk_dialog_done_fn fn );


typedef gboolean (*file_callback_t)( const gchar *filename );
void open_file_dialog( char *title, file_callback_t action, char *pattern, char *patname,
                       gchar const *initial_dir );
/**
 * Extract the keyval of the key event if no modifier keys were pressed -
 * in other words get the keyval of the key by itself. The other way around
 * would be to use the hardware keysyms directly rather than the keyvals,
 * but the mapping looks to be messier.
 */
uint16_t gtk_get_unmodified_keyval( GdkEventKey *event );

/**
 * Map a hardware keycode (not keyval) to a modifier state mask.
 * @param display The display (containing the modifier map)
 * @param keycde The hardware keycode to map
 * @return The modifier mask (eg GDK_CONTROL_MASK) or 0 if the keycode
 * is not recognized as a modifier key.
 */
guint gdk_keycode_to_modifier( GdkDisplay *display, guint keycode );

/**
 * Return an absolute path for the given input path, as a newly allocated
 * string. If the input path is already absolute, the returned string will
 * be identical to the input string.
 */
gchar *get_absolute_path( const gchar *path );

/**
 * Construct a new pixbuf that takes ownership of the frame buffer
 */
GdkPixbuf *gdk_pixbuf_new_from_frame_buffer( frame_buffer_t buffer );

void gdrom_menu_init();
GtkWidget *gdrom_menu_new();

/******************** Video driver hook *********************/

GtkWidget *video_gtk_create_drawable();

/******************* Callback declarations *******************/

void load_binary_action_callback( GtkAction *action, gpointer user_data);
void mount_action_callback( GtkAction *action, gpointer user_data);
void reset_action_callback( GtkAction *action, gpointer user_data);
void pause_action_callback( GtkAction *action, gpointer user_data);
void resume_action_callback( GtkAction *action, gpointer user_data);
void load_state_action_callback( GtkAction *action, gpointer user_data);
void save_state_action_callback( GtkAction *action, gpointer user_data);
void about_action_callback( GtkAction *action, gpointer user_data);
void exit_action_callback( GtkAction *action, gpointer user_data);

void path_settings_callback( GtkAction *action, gpointer user_data);
void audio_settings_callback( GtkAction *action, gpointer user_data);
void maple_settings_callback( GtkAction *action, gpointer user_data);
void network_settings_callback( GtkAction *action, gpointer user_data);
void video_settings_callback( GtkAction *action, gpointer user_data);
void fullscreen_toggle_callback( GtkToggleAction *action, gpointer user_data);

void debugger_action_callback( GtkAction *action, gpointer user_data);
void debug_memory_action_callback( GtkAction *action, gpointer user_data);
void debug_mmio_action_callback( GtkAction *action, gpointer user_data);
void save_scene_action_callback( GtkAction *action, gpointer user_data);
void debug_step_action_callback( GtkAction *action, gpointer user_data);
void debug_runto_action_callback( GtkAction *action, gpointer user_data);
void debug_breakpoint_action_callback( GtkAction *action, gpointer user_data);

void gdrom_open_direct_callback( GtkWidget *widget, gpointer user_data );

/*************** Constant colour/font values *****************/
extern PangoFontDescription *gui_fixed_font;
extern GdkColor gui_colour_normal, gui_colour_changed, gui_colour_error;
extern GdkColor gui_colour_warn, gui_colour_pc, gui_colour_debug;
extern GdkColor gui_colour_trace, gui_colour_break, gui_colour_temp_break;
extern GdkColor gui_colour_white;

#ifdef __cplusplus
}
#endif

#endif /* lxdream_gtkui_H */
