/**
 * $Id: gtkui.c,v 1.4 2007-10-17 11:26:45 nkeynes Exp $
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
 * Count of running nanoseconds - used to cut back on the GUI runtime
 */
static uint32_t gtk_gui_nanos = 0;
static struct timeval gtk_gui_lasttv;

gboolean gui_parse_cmdline( int *argc, char **argv[] )
{
    return gtk_init_check( argc, argv );
}

gboolean gui_init( gboolean withDebug )
{
    dreamcast_register_module( &gtk_gui_module );
    gtk_gui_alloc_resources();
    if( withDebug ) {
	debug_win = debug_window_new();
    }
    main_win = main_window_new( APP_NAME " " APP_VERSION );
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
	debug_win = debug_window_new();
    }
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
	mmio_win_update(mmio_win);
    }
    dump_win_update_all();
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

gint gtk_gui_run_property_dialog( const gchar *title, GtkWidget *panel )
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
    gtk_widget_destroy( dialog );
    return result;
}
