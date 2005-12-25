/**
 * $Id: gui.c,v 1.8 2005-12-25 05:57:00 nkeynes Exp $
 * 
 * Top-level GUI (GTK2) module.
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

#include <stdlib.h>
#include <stdarg.h>
#include <gnome.h>
#include <math.h>
#include "dream.h"
#include "dreamcast.h"
#include "mem.h"
#include "sh4/sh4dasm.h"
#include "aica/armdasm.h"
#include "gui/gui.h"

#define REGISTER_FONT "-*-fixed-medium-r-normal--12-*-*-*-*-*-iso8859-1"

GdkColor clrNormal, clrChanged, clrError, clrWarn, clrPC, clrDebug, clrTrace;
PangoFontDescription *fixed_list_font;

debug_info_t main_debug;


void open_file_callback(GtkWidget *btn, gint result, gpointer user_data);

void gtk_gui_init( void );
void gtk_gui_update( void );
void gtk_gui_start( void );
void gtk_gui_stop( void );
uint32_t gtk_gui_run_slice( uint32_t nanosecs );

struct dreamcast_module gtk_gui_module = { "Debugger", gtk_gui_init,
					   gtk_gui_update, gtk_gui_start, 
					   gtk_gui_run_slice, 
					   gtk_gui_stop, 
					   NULL, NULL };
					   
const cpu_desc_t cpu_descs[4] = { &sh4_cpu_desc, &arm_cpu_desc, &armt_cpu_desc, NULL };


void gtk_gui_init() {
    GdkColormap *map;
    GtkWidget *debug_win;

    clrNormal.red = clrNormal.green = clrNormal.blue = 0;
    clrChanged.red = clrChanged.green = 64*256;
    clrChanged.blue = 154*256;
    clrError.red = 65535;
    clrError.green = clrError.blue = 64*256;
    clrPC.red = 32*256;
    clrPC.green = 170*256;
    clrPC.blue = 52*256;
    clrWarn = clrChanged;
    clrTrace.red = 156*256;
    clrTrace.green = 78*256;
    clrTrace.blue = 201*256;
    clrDebug = clrPC;

    map = gdk_colormap_new(gdk_visual_get_best(), TRUE);
    gdk_colormap_alloc_color(map, &clrNormal, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &clrChanged, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &clrError, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &clrWarn, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &clrPC, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &clrDebug, TRUE, TRUE);
    gdk_colormap_alloc_color(map, &clrTrace, TRUE, TRUE);
    fixed_list_font = pango_font_description_from_string("Courier 10");
    debug_win = create_debug_win ();
    main_debug = init_debug_win(debug_win, cpu_descs);
    init_mmr_win();
    
    gtk_widget_show (debug_win);

}

/**
 * Hook called when DC starts running. Just disables the run/step buttons
 * and enables the stop button.
 */
void gtk_gui_start( void )
{
    debug_win_set_running( main_debug, TRUE );
}

/**
 * Hook called when DC stops running. Enables the run/step buttons
 * and disables the stop button.
 */
void gtk_gui_stop( void )
{
    debug_win_set_running( main_debug, FALSE );
    gtk_gui_update();
}

uint32_t gtk_gui_run_slice( uint32_t nanosecs ) 
{
    while( gtk_events_pending() )
	gtk_main_iteration();
    update_icount(main_debug);
    return nanosecs;
}

void gtk_gui_update(void) {
    update_registers(main_debug);
    update_icount(main_debug);
    update_mmr_win();
    dump_win_update_all();
}

void open_file_callback(GtkWidget *btn, gint result, gpointer user_data) {
    GtkFileChooser *file = GTK_FILE_CHOOSER(user_data);
    if( result == GTK_RESPONSE_ACCEPT ) {
	gchar *filename =gtk_file_chooser_get_filename(
						       GTK_FILE_CHOOSER(file) );
	file_callback_t action = (file_callback_t)gtk_object_get_data( GTK_OBJECT(file), "file_action" );
	gtk_widget_destroy(GTK_WIDGET(file));
	action( filename );
	g_free(filename);
    } else {
	gtk_widget_destroy(GTK_WIDGET(file));
    }
}

static void add_file_pattern( GtkFileChooser *chooser, char *pattern, char *patname )
{
    if( pattern != NULL ) {
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_add_pattern( filter, pattern );
	gtk_file_filter_set_name( filter, patname );
	gtk_file_chooser_add_filter( chooser, filter );
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name( filter, "All files" );
	gtk_file_filter_add_pattern( filter, "*" );
	gtk_file_chooser_add_filter( chooser, filter );
    }
}

void open_file_dialog( char *title, file_callback_t action, char *pattern, char *patname )
{
    GtkWidget *file;

    file = gtk_file_chooser_dialog_new( title, NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL );
    add_file_pattern( GTK_FILE_CHOOSER(file), pattern, patname );
    g_signal_connect( GTK_OBJECT(file), "response", 
		      GTK_SIGNAL_FUNC(open_file_callback), file );
    gtk_object_set_data( GTK_OBJECT(file), "file_action", action );
    gtk_widget_show( file );
}

void save_file_dialog( char *title, file_callback_t action, char *pattern, char *patname )
{
    GtkWidget *file;

    file = gtk_file_chooser_dialog_new( title, NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL );
    add_file_pattern( GTK_FILE_CHOOSER(file), pattern, patname );
    g_signal_connect( GTK_OBJECT(file), "response", 
		      GTK_SIGNAL_FUNC(open_file_callback), file );
    gtk_object_set_data( GTK_OBJECT(file), "file_action", action );
    gtk_widget_show( file );
}

uint32_t gtk_entry_get_hex_value( GtkEntry *entry, uint32_t defaultValue )
{
    gchar *text = gtk_entry_get_text(entry);
    if( text == NULL )
        return defaultValue;
    gchar *endptr;
    uint32_t value = strtoul( text, &endptr, 16 );
    if( text == endptr ) { /* invalid input */
        value = defaultValue;
        gtk_entry_set_hex_value( entry, value );
    }
    return value;
}

void gtk_entry_set_hex_value( GtkEntry *entry, uint32_t value )
{
    char buf[10];
    sprintf( buf, "%08X", value );
    gtk_entry_set_text( entry, buf );
}
