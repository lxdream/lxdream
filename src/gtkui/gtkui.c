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

#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <glib/gi18n.h>
#include <gtk/gtkversion.h>
#include "lxdream.h"
#include "dreamcast.h"
#include "dream.h"
#include "display.h"
#include "gdrom/gdrom.h"
#include "gtkui/gtkui.h"

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
static GtkActionGroup *global_action_group;

/**
 * Count of running nanoseconds - used to cut back on the GUI runtime
 */
static uint32_t gtk_gui_nanos = 0;
static uint32_t gtk_gui_ticks = 0;
static struct timeval gtk_gui_lasttv;

static gboolean gtk_gui_init_ok = FALSE;

#define ENABLE_ACTION(win,name) SET_ACTION_ENABLED(win,name,TRUE)
#define DISABLE_ACTION(win,name) SET_ACTION_ENABLED(win,name,FALSE)

// UI Actions
static const GtkActionEntry ui_actions[] = {
        { "FileMenu", NULL, N_("_File") },
        { "SettingsMenu", NULL, N_("_Settings") },
        { "HelpMenu", NULL, N_("_Help") },
        { "LoadBinary", NULL, N_("Load _Binary..."), NULL, N_("Load and run a program binary"), G_CALLBACK(load_binary_action_callback) },
        { "Reset", GTK_STOCK_REFRESH, N_("_Reset"), "<control>R", N_("Reset dreamcast"), G_CALLBACK(reset_action_callback) },
        { "Pause", GTK_STOCK_MEDIA_PAUSE, N_("_Pause"), NULL, N_("Pause dreamcast"), G_CALLBACK(pause_action_callback) },
        { "Run", GTK_STOCK_MEDIA_PLAY, N_("Resume"), NULL, N_("Resume"), G_CALLBACK(resume_action_callback) },
        { "LoadState", GTK_STOCK_REVERT_TO_SAVED, N_("_Load state..."), "F4", N_("Load an lxdream save state"), G_CALLBACK(load_state_action_callback) },
        { "SaveState", GTK_STOCK_SAVE_AS, N_("_Save state..."), "F3", N_("Create an lxdream save state"), G_CALLBACK(save_state_action_callback) },
        { "Exit", GTK_STOCK_QUIT, N_("E_xit"), NULL, N_("Exit lxdream"), G_CALLBACK(exit_action_callback) },
        { "GdromSettings", NULL, N_("_GD-Rom...") },
        { "GdromUnmount", NULL, N_("_Empty") },
        { "GdromMount", GTK_STOCK_CDROM, N_("_Open Image..."), "<control>O", N_("Mount a cdrom disc"), G_CALLBACK(mount_action_callback) },
        { "PathSettings", NULL, N_("_Paths..."), NULL, N_("Configure files and paths"), G_CALLBACK(path_settings_callback) }, 
        { "AudioSettings", NULL, N_("_Audio..."), NULL, N_("Configure audio output"), G_CALLBACK(audio_settings_callback) },
        { "ControllerSettings", NULL, N_("_Controllers..."), NULL, N_("Configure controllers"), G_CALLBACK(maple_settings_callback) },
        { "NetworkSettings", NULL, N_("_Network..."), NULL, N_("Configure network settings"), G_CALLBACK(network_settings_callback) },
        { "VideoSettings", NULL, N_("_Video..."), NULL,N_( "Configure video output"), G_CALLBACK(video_settings_callback) },
        { "About", GTK_STOCK_ABOUT, N_("_About..."), NULL, N_("About lxdream"), G_CALLBACK(about_action_callback) },
        { "DebugMenu", NULL, N_("_Debug") },
        { "Debugger", NULL, N_("_Debugger"), NULL, N_("Open debugger window"), G_CALLBACK(debugger_action_callback) },
        { "DebugMem", NULL, N_("View _Memory"), NULL, N_("View memory dump"), G_CALLBACK(debug_memory_action_callback) },
        { "DebugMmio", NULL, N_("View IO _Registers"), NULL, N_("View MMIO Registers"), G_CALLBACK(debug_mmio_action_callback) },
        { "SaveScene", NULL, N_("_Save Scene"), NULL, N_("Save next rendered scene"), G_CALLBACK(save_scene_action_callback) },
        { "SingleStep", GTK_STOCK_REDO, N_("_Single Step"), NULL, N_("Single step"), G_CALLBACK(debug_step_action_callback) },
        { "RunTo", GTK_STOCK_GOTO_LAST, N_("Run _To"), NULL, N_("Run to"), G_CALLBACK( debug_runto_action_callback) },
        { "SetBreakpoint", GTK_STOCK_CLOSE, N_("_Breakpoint"), NULL, N_("Toggle breakpoint"), G_CALLBACK( debug_breakpoint_action_callback) }
};
static const GtkToggleActionEntry ui_toggle_actions[] = {
        { "FullScreen", NULL, "_Full Screen", "<alt>Return", "Toggle full screen video", G_CALLBACK(fullscreen_toggle_callback), 0 },
};

// Menus and toolbars
static const char *ui_description =
    "<ui>"
    " <menubar name='MainMenu'>"
    "  <menu action='FileMenu'>"
    "   <menuitem action='LoadBinary'/>"
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
    gtk_gui_init_ok = gtk_init_check( argc, argv );
    return gtk_gui_init_ok;
}

gboolean gui_init( gboolean withDebug )
{
    if( gtk_gui_init_ok ) {
        GError *error = NULL;
        dreamcast_register_module( &gtk_gui_module );
        gtk_gui_alloc_resources();

        global_action_group = gtk_action_group_new("MenuActions");
        gtk_action_group_set_translation_domain( global_action_group, NULL );
        gtk_action_group_add_actions( global_action_group, ui_actions, G_N_ELEMENTS(ui_actions), NULL );
        gtk_action_group_add_toggle_actions( global_action_group, ui_toggle_actions, G_N_ELEMENTS(ui_toggle_actions), NULL );
        gtk_gui_enable_action("AudioSettings", FALSE);
        gtk_gui_enable_action("NetworkSettings", FALSE);
        gtk_gui_enable_action("VideoSettings", FALSE);

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
        GtkWidget *gdrommenu = gdrom_menu_new();
        gtk_menu_item_set_submenu( GTK_MENU_ITEM(gdrommenuitem), gdrommenu );
        main_win = main_window_new( lxdream_package_name, menubar, toolbar, accel_group  );
        main_window_set_use_grab(main_win, TRUE);
        if( withDebug ) {
            gtk_gui_show_debugger();
        }

        return TRUE;
    } else {
        return FALSE;
    }
}

void gui_main_loop( gboolean run )
{
    gtk_gui_update();
    if( run ) {
        dreamcast_run();
        gtk_main();
    } else {
        gtk_main();
    }
}

void gui_update_state(void)
{
    gtk_gui_update();
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
        gchar *title = g_strdup_printf( "%s :: %s", lxdream_package_name, _("Debugger"));
        debug_win = debug_window_new( title, menubar, toolbar, accel_group  );
        g_free(title);
    }
}

void gtk_gui_show_mmio()
{
    if( mmio_win ) {
        mmio_window_show(mmio_win, TRUE);
    } else {
        gchar *title = g_strdup_printf( "%s :: %s", lxdream_package_name, _("MMIO Registers"));
        mmio_win = mmio_window_new( title );
        g_free(title);
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
    if( global_action_group ) {
        gtk_gui_enable_action("Run", dreamcast_can_run() && !dreamcast_is_running() );
        gtk_gui_enable_action("Pause", dreamcast_is_running() );
    }
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
 * Module run-slice. Run the event loop 100 times/second (doesn't really need to be
 * any more often than this), and update the speed display 10 times/second. 
 *
 * Also detect if we're running too fast here and yield for a bit
 */
uint32_t gtk_gui_run_slice( uint32_t nanosecs ) 
{
    gtk_gui_nanos += nanosecs;
    if( gtk_gui_nanos > GUI_TICK_PERIOD ) { /* 10 ms */
        gtk_gui_nanos -= GUI_TICK_PERIOD;
        gtk_gui_ticks ++;
        uint32_t current_period = gtk_gui_ticks * GUI_TICK_PERIOD;

        // Run the event loop
        while( gtk_events_pending() )
            gtk_main_iteration();	

        struct timeval tv;
        gettimeofday(&tv,NULL);
        uint32_t ns = ((tv.tv_sec - gtk_gui_lasttv.tv_sec) * 1000000000) + 
        (tv.tv_usec - gtk_gui_lasttv.tv_usec)*1000;
        if( (ns * 1.05) < current_period ) {
            // We've gotten ahead - sleep for a little bit
            struct timespec tv;
            tv.tv_sec = 0;
            tv.tv_nsec = current_period - ns;
            nanosleep(&tv, &tv);
        }

        /* Update the display every 10 ticks (ie 10 times a second) and 
         * save the current tv value */
        if( gtk_gui_ticks > 10 ) {
            gtk_gui_ticks -= 10;

            double speed = (float)( (double)current_period * 100.0 / ns );
            gtk_gui_lasttv.tv_sec = tv.tv_sec;
            gtk_gui_lasttv.tv_usec = tv.tv_usec;
            main_window_set_speed( main_win, speed );
        }
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
    return gdk_pixbuf_new_from_data( (unsigned char *)buffer->data, 
            GDK_COLORSPACE_RGB,
            (buffer->colour_format == COLFMT_BGRA8888),
            8,
            buffer->width,
            buffer->height,
            buffer->rowstride,
            delete_frame_buffer,
            buffer );
}

/**
 * Extract the keyval of the key event if no modifier keys were pressed -
 * in other words get the keyval of the key by itself. The other way around
 * would be to use the hardware keysyms directly rather than the keyvals,
 * but the mapping looks to be messier.
 */
uint16_t gtk_get_unmodified_keyval( GdkEventKey *event )
{
    GdkKeymap *keymap = gdk_keymap_get_default();
    guint keyval;

    gdk_keymap_translate_keyboard_state( keymap, event->hardware_keycode, 0, 0, &keyval, 
                                         NULL, NULL, NULL );
    return keyval;
}

gchar *get_absolute_path( const gchar *in_path )
{
    char tmp[PATH_MAX];
    if( in_path == NULL ) {
        return NULL;
    }
    if( in_path[0] == '/' || in_path[0] == 0 ) {
        return g_strdup(in_path);
    } else {
        getcwd(tmp, sizeof(tmp));
        return g_strdup_printf("%s%c%s", tmp, G_DIR_SEPARATOR, in_path);
    }
}

