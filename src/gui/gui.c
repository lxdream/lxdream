#include <stdlib.h>
#include <stdarg.h>
#include <gnome.h>
#include <math.h>
#include "dreamcast.h"
#include "gui.h"
#include "mem.h"
#include "sh4dasm.h"
#include "sh4core.h"

#define REGISTER_FONT "-*-fixed-medium-r-normal--12-*-*-*-*-*-iso8859-1"

GdkColor clrNormal, clrChanged, clrError, clrWarn, clrPC, clrDebug, clrTrace;
PangoFontDescription *fixed_list_font;

void open_file_callback(GtkWidget *btn, gint result, gpointer user_data);

void init_gui() {
    GdkColormap *map;
    
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
}

void gui_run_slice( int millisecs ) 
{
    while( gtk_events_pending() )
	gtk_main_iteration();
    update_icount(main_debug);
}

void update_gui(void) {
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
