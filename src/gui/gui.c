#include <stdlib.h>
#include <stdarg.h>
#include <gnome.h>
#include <math.h>
#include "gui.h"
#include "mem.h"
#include "sh4dasm.h"
#include "sh4core.h"

#define REGISTER_FONT "-*-fixed-medium-r-normal--12-*-*-*-*-*-iso8859-1"

GdkColor clrNormal, clrChanged, clrError, clrWarn, clrPC, clrDebug, clrTrace;
PangoFontDescription *fixed_list_font;

void open_file_callback(GtkWidget *btn, gpointer user_data);
void open_file_canceled(GtkWidget *btn, gpointer user_data);
void open_file( char *filename );

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

void update_gui(void) {
    update_registers();
    update_icount();
    update_mmr_win();
    dump_win_update_all();
}


void open_file_callback(GtkWidget *btn, gpointer user_data) {
    GtkFileSelection *file = GTK_FILE_SELECTION(user_data);
    gchar *filename = strdup( gtk_file_selection_get_filename(
        GTK_FILE_SELECTION(file) ) );
    gtk_widget_destroy(GTK_WIDGET(file));
    open_file( filename );
    free(filename);
}

void open_file_canceled(GtkWidget *btn, gpointer user_data) {
    gtk_widget_destroy(GTK_WIDGET(user_data));
}

void open_file_dialog( void )
{
    GtkWidget *file;

    file = gtk_file_selection_new( "Open..." );
    gtk_signal_connect( GTK_OBJECT(GTK_FILE_SELECTION(file)->ok_button),
                        "clicked", GTK_SIGNAL_FUNC(open_file_callback), file );
    gtk_signal_connect( GTK_OBJECT(GTK_FILE_SELECTION(file)->cancel_button),
                        "clicked", GTK_SIGNAL_FUNC(open_file_canceled), file );
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
