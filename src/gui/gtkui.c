/**
 * $Id: gtkui.c,v 1.9 2007-10-31 09:10:23 nkeynes Exp $
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

#include <sys/time.h>
#include <time.h>
#include "dream.h"
#include "dreamcast.h"
#include "display.h"
#include "gdrom/gdrom.h"
#include "gui/gtkui.h"


void gtk_gui_update( void );
void gtk_gui_start( void );
void gtk_gui_stop( void );
void gtk_gui_alloc_resources ( void );
uint32_t gtk_gui_run_slice( uint32_t nanosecs );

struct dreamcast_module gtk_gui_module = { "gui", NULL,
					   gtk_gui_update, 
					   gtk_gui_start, 
					   gtk_gui_run_slice, 
					   gtk_gui_stop, 
					   NULL, NULL };

/**
 * Single-instance windows (at most one)
 */
static main_window_t main_win = NULL;
static debug_window_t debug_win = NULL;
static mmio_window_t mmio_win = NULL;

/**
 * UIManager and action helpers
 */
static GtkUIManager *global_ui_manager;
static GtkAccelGroup *global_accel_group;
static GtkActionGroup *global_action_group;

/**
 * Count of running nanoseconds - used to cut back on the GUI runtime
 */
static uint32_t gtk_gui_nanos = 0;
static struct timeval gtk_gui_lasttv;

#define ENABLE_ACTION(win,name) SET_ACTION_ENABLED(win,name,TRUE)
#define DISABLE_ACTION(win,name) SET_ACTION_ENABLED(win,name,FALSE)

// UI Actions
static const GtkActionEntry ui_actions[] = {
    { "FileMenu", NULL, "_File" },
    { "SettingsMenu", NULL, "_Settings" },
    { "HelpMenu", NULL, "_Help" },
    { "Reset", GTK_STOCK_REFRESH, "_Reset", "<control>R", "Reset dreamcast", G_CALLBACK(reset_action_callback) },
    { "Pause", GTK_STOCK_MEDIA_PAUSE, "_Pause", NULL, "Pause dreamcast", G_CALLBACK(pause_action_callback) },
    { "Run", GTK_STOCK_MEDIA_PLAY, "Resume", NULL, "Resume", G_CALLBACK(resume_action_callback) },
    { "LoadState", GTK_STOCK_REVERT_TO_SAVED, "_Load state...", "F4", "Load an lxdream save state", G_CALLBACK(load_state_action_callback) },
    { "SaveState", GTK_STOCK_SAVE_AS, "_Save state...", "F3", "Create an lxdream save state", G_CALLBACK(save_state_action_callback) },
    { "Exit", GTK_STOCK_QUIT, "E_xit", NULL, "Exit lxdream", G_CALLBACK(exit_action_callback) },
    { "GdromSettings", NULL, "_GD-Rom..." },
    { "GdromUnmount", NULL, "_Empty" },
    { "GdromMount", GTK_STOCK_CDROM, "_Open Image...", "<control>O", "Mount a cdrom disc", G_CALLBACK(mount_action_callback) },
    { "PathSettings", NULL, "_Paths...", NULL, "Configure files and paths", G_CALLBACK(path_settings_callback) }, 
    { "AudioSettings", NULL, "_Audio...", NULL, "Configure audio output", G_CALLBACK(audio_settings_callback) },
    { "ControllerSettings", NULL, "_Controllers...", NULL, "Configure controllers", G_CALLBACK(maple_settings_callback) },
    { "NetworkSettings", NULL, "_Network...", NULL, "Configure network settings", G_CALLBACK(network_settings_callback) },
    { "VideoSettings", NULL, "_Video...", NULL, "Configure video output", G_CALLBACK(video_settings_callback) },
    { "About", GTK_STOCK_ABOUT, "_About...", NULL, "About lxdream", G_CALLBACK(about_action_callback) },
    { "DebugMenu", NULL, "_Debug" },
    { "Debugger", NULL, "_Debugger", NULL, "Open debugger window", G_CALLBACK(debugger_action_callback) },
    { "DebugMem", NULL, "View _Memory", NULL, "View memory dump", G_CALLBACK(debug_memory_action_callback) },
    { "DebugMmio", NULL, "View IO _Registers", NULL, "View MMIO Registers", G_CALLBACK(debug_mmio_action_callback) },
    { "SaveScene", NULL, "_Save Scene", NULL, "Save next rendered scene", G_CALLBACK(save_scene_action_callback) },
    { "SingleStep", GTK_STOCK_REDO, "_Single Step", NULL, "Single step", G_CALLBACK(debug_step_action_callback) },
    { "RunTo", GTK_STOCK_GOTO_LAST, "Run _To", NULL, "Run to", G_CALLBACK( debug_runto_action_callback) },
    { "SetBreakpoint", GTK_STOCK_CLOSE, "_Breakpoint", NULL, "Toggle breakpoint", G_CALLBACK( debug_breakpoint_action_callback) }
};
static const GtkToggleActionEntry ui_toggle_actions[] = {
    { "FullScreen", NULL, "_Full Screen", "<alt>Return", "Toggle full screen video", G_CALLBACK(fullscreen_toggle_callback), 0 },
};
    
// Menus and toolbars
static const char *ui_description =
    "<ui>"
    " <menubar name='MainMenu'>"
    "  <menu action='FileMenu'>"
    "   <menuitem action='GdromSettings'/>"
    "   <separator/>"
    "   <menuitem action='Reset'/>"
    "   <menuitem action='Pause'/>"
    "   <menuitem action='Run'/>"
    "   <menuitem action='Debugger'/>"
    "   <separator/>"
    "   <menuitem action='LoadState'/>"
    "   <menuitem action='SaveState'/>"
    "   <separator/>"
    "   <menuitem action='Exit'/>"
    "  </menu>"
    "  <menu action='SettingsMenu'>"
    "   <menuitem action='PathSettings'/>"
    "   <menuitem action='AudioSettings'/>"
    "   <menuitem action='ControllerSettings'/>"
    "   <menuitem action='NetworkSettings'/>"
    "   <menuitem action='VideoSettings'/>"
    "   <separator/>"
    "   <menuitem action='FullScreen'/>"
    "  </menu>"
    "  <menu action='HelpMenu'>"
    "   <menuitem action='About'/>"
    "  </menu>"
    " </menubar>"
    " <toolbar name='MainToolbar'>"
    "  <toolitem action='GdromMount'/>"
    "  <toolitem action='Reset'/>"
    "  <toolitem action='Pause'/>"
    "  <toolitem action='Run'/>"
    "  <separator/>"
    "  <toolitem action='LoadState'/>"
    "  <toolitem action='SaveState'/>"
    " </toolbar>"
    " <menubar name='DebugMenu'>"
    "  <menu action='FileMenu'>"
    "   <menuitem action='GdromSettings'/>"
    "   <separator/>"
    "   <menuitem action='Reset'/>"
    "   <separator/>"
    "   <menuitem action='LoadState'/>"
    "   <menuitem action='SaveState'/>"
    "   <separator/>"
    "   <menuitem action='Exit'/>"
    "  </menu>"
    "  <menu action='DebugMenu'>"
    "   <menuitem action='DebugMem'/>"
    "   <menuitem action='DebugMmio'/>"
    "   <menuitem action='SaveScene'/>"
    "   <separator/>"
    "   <menuitem action='SetBreakpoint'/>"
    "   <menuitem action='Pause'/>"
    "   <menuitem action='SingleStep'/>"
    "   <menuitem action='RunTo'/>"
    "   <menuitem action='Run'/>"
    "  </menu>"
    "  <menu action='SettingsMenu'>"
    "   <menuitem action='PathSettings'/>"
    "   <menuitem action='AudioSettings'/>"
    "   <menuitem action='ControllerSettings'/>"
    "   <menuitem action='NetworkSettings'/>"
    "   <menuitem action='VideoSettings'/>"
    "   <separator/>"
    "   <menuitem action='FullScreen'/>"
    "  </menu>"
    "  <menu action='HelpMenu'>"
    "   <menuitem action='About'/>"
    "  </menu>"
    " </menubar>"
    " <toolbar name='DebugToolbar'>"
    "  <toolitem action='GdromMount'/>"
    "  <toolitem action='Reset'/>"
    "  <toolitem action='Pause'/>"
    "  <separator/>"
    "  <toolitem action='SingleStep'/>"
    "  <toolitem action='RunTo'/>"
    "  <toolitem action='Run'/>"
    "  <toolitem action='SetBreakpoint'/>"
    "  <separator/>"
    "  <toolitem action='LoadState'/>"
    "  <toolitem action='SaveState'/>"
    " </toolbar>"
    "</ui>";

gboolean gui_parse_cmdline( int *argc, char **argv[] )
{
    return gtk_init_check( argc, argv );
}

gboolean gui_init( gboolean withDebug )
{
    GError *error = NULL;
    dreamcast_register_module( &gtk_gui_module );
    gtk_gui_alloc_resources();
    
    global_action_group = gtk_action_group_new("MenuActions");
    gtk_action_group_add_actions( global_action_group, ui_actions, G_N_ELEMENTS(ui_actions), NULL );
    gtk_action_group_add_toggle_actions( global_action_group, ui_toggle_actions, G_N_ELEMENTS(ui_toggle_actions), NULL );
    gtk_gui_enable_action("AudioSettings", FALSE);
    gtk_gui_enable_action("NetworkSettings", FALSE);
    gtk_gui_enable_action("VideoSettings", FALSE);
    gtk_gui_enable_action("FullScreen", FALSE);

    global_ui_manager = gtk_ui_manager_new();
    gtk_ui_manager_set_add_tearoffs(global_ui_manager, TRUE);
    gtk_ui_manager_insert_action_group( global_ui_manager, global_action_group, 0 );

    if (!gtk_ui_manager_add_ui_from_string (global_ui_manager, ui_description, -1, &error)) {
	g_message ("building menus failed: %s", error->message);
	g_error_free (error);
	exit(1);
    }
    GtkAccelGroup *accel_group = gtk_ui_manager_get_accel_group (global_ui_manager);
    GtkWidget *menubar = gtk_ui_manager_get_widget(global_ui_manager, "/MainMenu");
    GtkWidget *toolbar = gtk_ui_manager_get_widget(global_ui_manager, "/MainToolbar");

    GtkWidget *gdrommenuitem = gtk_ui_manager_get_widget(global_ui_manager, "/MainMenu/FileMenu/GdromSettings");
    gdrom_menu_init();
    GtkWidget *gdrommenu = gdrom_menu_new();
    gtk_menu_item_set_submenu( GTK_MENU_ITEM(gdrommenuitem), gdrommenu );
    main_win = main_window_new( APP_NAME " " APP_VERSION, menubar, toolbar, accel_group  );
    if( withDebug ) {
	gtk_gui_show_debugger();
    }

    return TRUE;
}

void gui_main_loop(void)
{
    gtk_main();
}

gboolean gui_error_dialog( const char *msg, ... )
{
    if( main_win != NULL ) {
	va_list args;
	GtkWidget *dialog = 
	    gtk_message_dialog_new( main_window_get_frame(main_win), GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, NULL );
	va_start(args, msg);
	gchar *markup = g_markup_vprintf_escaped( msg, args );
	va_end( args );
	gtk_message_dialog_set_markup( GTK_MESSAGE_DIALOG(dialog), markup );
	g_free(markup);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return TRUE;
    }
    return FALSE;
}

void gui_update_io_activity( io_activity_type io, gboolean active )
{

}

void gtk_gui_show_debugger()
{
    if( debug_win ) {
	debug_window_show(debug_win, TRUE);
    } else {
	GtkAccelGroup *accel_group = gtk_ui_manager_get_accel_group (global_ui_manager);
	GtkWidget *menubar = gtk_ui_manager_get_widget(global_ui_manager, "/DebugMenu");
	GtkWidget *toolbar = gtk_ui_manager_get_widget(global_ui_manager, "/DebugToolbar");
	GtkWidget *gdrommenuitem = gtk_ui_manager_get_widget(global_ui_manager, "/DebugMenu/FileMenu/GdromSettings");
	GtkWidget *gdrommenu = gdrom_menu_new();
	gtk_menu_item_set_submenu( GTK_MENU_ITEM(gdrommenuitem), gdrommenu );
	debug_win = debug_window_new( APP_NAME " " APP_VERSION " :: Debugger", menubar, toolbar, accel_group  );
    }
}

void gtk_gui_show_mmio()
{
    if( mmio_win ) {
	mmio_window_show(mmio_win, TRUE);
    } else {
	mmio_win = mmio_window_new( APP_NAME " " APP_VERSION " :: MMIO Registers" );
    }
}


main_window_t gtk_gui_get_main()
{
    return main_win;
}

debug_window_t gtk_gui_get_debugger()
{
    return debug_win;
}

mmio_window_t gtk_gui_get_mmio()
{
    return mmio_win;
}

GtkWidget *gtk_gui_get_renderarea()
{
    return main_window_get_renderarea(main_win);
}

/**
 * Hook called when DC starts running. Just disables the run/step buttons
 * and enables the stop button.
 */
void gtk_gui_start( void )
{
    main_window_set_running( main_win, TRUE );
    if( debug_win != NULL ) {
	debug_window_set_running( debug_win, TRUE );
    }
    gtk_gui_nanos = 0;
    gettimeofday(&gtk_gui_lasttv,NULL);
}

/**
 * Hook called when DC stops running. Enables the run/step buttons
 * and disables the stop button.
 */
void gtk_gui_stop( void )
{
    main_window_set_running( main_win, FALSE );
    gtk_gui_update();
}

void gtk_gui_update( void )
{
    if( debug_win ) {
	debug_window_set_running( debug_win, FALSE );
	debug_window_update(debug_win);
    }
    if( mmio_win ) {
	mmio_window_update(mmio_win);
    }
    dump_window_update_all();
}

/**
 * Module run-slice. Because UI processing is fairly expensive, only 
 * run the processing about 10 times a second while we're emulating.
 */
uint32_t gtk_gui_run_slice( uint32_t nanosecs ) 
{
    gtk_gui_nanos += nanosecs;
    if( gtk_gui_nanos > 100000000 ) { 
	struct timeval tv;
	while( gtk_events_pending() )
	    gtk_main_iteration();
	
	gettimeofday(&tv,NULL);
	double ns = ((tv.tv_sec - gtk_gui_lasttv.tv_sec) * 1000000000.0) +
	    ((tv.tv_usec - gtk_gui_lasttv.tv_usec)*1000.0);
	double speed = (float)( (double)gtk_gui_nanos * 100.0 / ns );
	gtk_gui_lasttv.tv_sec = tv.tv_sec;
	gtk_gui_lasttv.tv_usec = tv.tv_usec;
	main_window_set_speed( main_win, speed );
	gtk_gui_nanos = 0;
    }
    return nanosecs;
}


PangoFontDescription *gui_fixed_font;
GdkColor gui_colour_normal, gui_colour_changed, gui_colour_error;
GdkColor gui_colour_warn, gui_colour_pc, gui_colour_debug;
GdkColor gui_colour_trace, gui_colour_break, gui_colour_temp_break;
GdkColor gui_colour_white;

void gtk_gui_alloc_resources() {
    GdkColormap *map;

    gui_colour_normal.red = gui_colour_normal.green = gui_colour_normal.blue = 0;
    gui_colour_changed.red = gui_colour_changed.green = 64*256;
    gui_colour_changed.blue = 154*256;
    gui_colour_error.red = 65535;
    gui_colour_error.green = gui_colour_error.blue = 64*256;
    gui_colour_pc.red = 32*256;
    gui_colour_pc.green = 170*256;
    gui_colour_pc.blue = 52*256;
    gui_colour_warn = gui_colour_changed;
    gui_colour_trace.red = 156*256;
    gui_colour_trace.green = 78*256;
    gui_colour_trace.blue = 201*256;
    gui_colour_debug = gui_colour_pc;
    gui_colour_break.red = 65535;
    gui_colour_break.green = gui_colour_break.blue = 192*256;
    gui_colour_temp_break.red = gui_colour_temp_break.green = 128*256;
    gui_colour_temp_break.blue = 32*256;
    gui_colour_white.red = gui_colour_white.green = gui_colour_white.blue = 65535;

    map = gdk_colormap_new(gdk_visual_get_best(), TRUE);
    gdk_colormap_alloc_color(map, &gui_colour_normal, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &gui_colour_changed, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &gui_colour_error, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &gui_colour_warn, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &gui_colour_pc, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &gui_colour_debug, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &gui_colour_trace, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &gui_colour_break, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &gui_colour_temp_break, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &gui_colour_white, TRUE, TRUE);
    gui_fixed_font = pango_font_description_from_string("Courier 10");
}

gint gtk_gui_run_property_dialog( const gchar *title, GtkWidget *panel, gtk_dialog_done_fn fn )
{
    GtkWidget *dialog =
	gtk_dialog_new_with_buttons(title, main_window_get_frame(main_win), 
				    GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
				    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
				    NULL);
    gint result;
    gtk_widget_show_all(panel);
    gtk_container_add( GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), panel );
    result = gtk_dialog_run( GTK_DIALOG(dialog) );
    if( fn != NULL ) {
	fn(panel, result == GTK_RESPONSE_ACCEPT);
    }
    gtk_widget_destroy( dialog );
    return result;
}

void gtk_gui_enable_action( const gchar *action, gboolean enable )
{
    gtk_action_set_sensitive( gtk_action_group_get_action( global_action_group, action), enable);
}

static void delete_frame_buffer( guchar *pixels, gpointer buffer )
{
    if( buffer != NULL ) {
	g_free(buffer);
    }
}

GdkPixbuf *gdk_pixbuf_new_from_frame_buffer( frame_buffer_t buffer )
{
    return gdk_pixbuf_new_from_data( buffer->data, 
				     GDK_COLORSPACE_RGB,
				     (buffer->colour_format == COLFMT_BGRA8888),
				     8,
				     buffer->width,
				     buffer->height,
				     buffer->rowstride,
				     delete_frame_buffer,
				     buffer );
}
