/**
 * $Id$
 *
 * Configuration pane to display a configuration group
 * TODO:
 *
 * Copyright (c) 2009 Nathan Keynes.
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

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "lxdream.h"
#include "config.h"
#include "lxpaths.h"
#include "display.h"
#include "gtkui/gtkui.h"

struct config_data {
    lxdream_config_group_t config;
    GtkWidget *fields[CONFIG_MAX_KEYS][2];
};

/**
 * Update the configuration data for the current value of the given field.
 */
static gboolean config_text_changed( GtkWidget *field, gpointer p )
{
    GtkWidget *panel = field->parent;
    struct config_data *data= (struct config_data *)gtk_object_get_data( GTK_OBJECT(panel), "config_data" );
    int tag = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(field), "tag" ) );

    char buf[64];
    GtkWidget *entry1, *entry2;
    const gchar *key1 = NULL, *key2 = NULL;

    if( data->fields[tag][0] != NULL ) {
        key1 = gtk_entry_get_text(GTK_ENTRY(data->fields[tag][0]));
    }
    if( data->fields[tag][1] != NULL ) {
        key2 = gtk_entry_get_text(GTK_ENTRY(data->fields[tag][1]));
    }

    if( key1 == NULL || key1[0] == '\0') {
        lxdream_set_config_value( data->config, tag, key2 );
    } else if( key2 == NULL || key2[0] == '\0') {
        lxdream_set_config_value( data->config, tag, key1 );
    } else {
        char buf[strlen(key1) + strlen(key2) + 3];
        snprintf( buf, sizeof(buf), "%s, %s", key1, key2 );
        lxdream_set_config_value( data->config, tag, buf );
    }
    return TRUE;
}

/**
 * Reset the fields (identified by one of the widgets in the field) back to it's
 * value in the config group.
 */
static void config_text_reset( GtkWidget *field )
{
    GtkWidget *panel = field->parent;
    struct config_data *data= (struct config_data *)gtk_object_get_data( GTK_OBJECT(panel), "config_data" );
    int tag = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(field), "tag" ) );

    const gchar *value = data->config->params[tag].value;
    if( value == NULL ) {
        value = "";
    }

    if( data->fields[tag][0] == NULL ) {
        if( data->fields[tag][1] != NULL ) {
            gtk_entry_set_text( GTK_ENTRY(data->fields[tag][1]), value );
        }
    } else if( data->fields[tag][1] == NULL ) {
        gtk_entry_set_text( GTK_ENTRY(data->fields[tag][0]), value );
    } else { /* Split between two fields */
        gchar *v1 = "", *v2 = "";
        gchar **parts = g_strsplit(value,",",3);
        if( parts[0] != NULL ) {
            v1 = parts[0];
            if( parts[1] != NULL ) {
                v2 = parts[1];
            }

        }
        gtk_entry_set_text( GTK_ENTRY(data->fields[tag][0]), v1 );
        gtk_entry_set_text( GTK_ENTRY(data->fields[tag][1]), v2 );
        g_strfreev(parts);
    }
}

static void config_set_field( void *p, const gchar *keysym )
{
    GtkWidget *field = GTK_WIDGET(p);
    GtkWidget *panel = field->parent;

    gtk_entry_set_text( GTK_ENTRY(field), keysym );
    g_object_set_data( G_OBJECT(field), "keypress_mode", GINT_TO_POINTER(FALSE) );
    input_set_keysym_hook(NULL, NULL);
    config_text_changed( field, NULL );
}

static gboolean config_key_buttonpress( GtkWidget *widget, GdkEventButton *event, gpointer user_data )
{
    gboolean keypress_mode = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(widget), "keypress_mode"));
    if( keypress_mode ) {
        gchar *keysym = input_keycode_to_keysym( &system_mouse_driver, event->button);
        if( keysym != NULL ) {
            config_set_field( widget, keysym );
            g_free(keysym);
        }
        return TRUE;
    } else {
        gtk_entry_set_text( GTK_ENTRY(widget), _("<press key>") );
        g_object_set_data( G_OBJECT(widget), "keypress_mode", GINT_TO_POINTER(TRUE) );
        input_set_keysym_hook( (display_keysym_callback_t)config_set_field, widget);
        gtk_widget_grab_focus( widget );
    }
    return FALSE;
}

static gboolean config_key_keypress( GtkWidget *widget, GdkEventKey *event, gpointer user_data )
{
    gboolean keypress_mode = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(widget), "keypress_mode"));
    if( keypress_mode ) {
        if( event->keyval == GDK_Escape ) {
            config_text_reset( widget );
            return TRUE;
        }
        GdkKeymap *keymap = gdk_keymap_get_default();
        guint keyval;

        gdk_keymap_translate_keyboard_state( keymap, event->hardware_keycode, 0, 0, &keyval,
                                             NULL, NULL, NULL );
        config_set_field( widget, gdk_keyval_name(keyval) );
        return TRUE;
    } else {
        switch( event->keyval ) {
        case GDK_Return:
        case GDK_KP_Enter:
            gtk_entry_set_text( GTK_ENTRY(widget), _("<press key>") );
            g_object_set_data( G_OBJECT(widget), "keypress_mode", GINT_TO_POINTER(TRUE) );
            input_set_keysym_hook((display_keysym_callback_t)config_set_field, widget);
            return TRUE;
        case GDK_BackSpace:
        case GDK_Delete:
            config_set_field( widget, "" );
            return TRUE;
        }
        return FALSE;
    }
}

static gboolean config_key_unfocus( GtkWidget *widget, gpointer user_data )
{
    gboolean keypress_mode = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(widget), "keypress_mode"));
    if( keypress_mode ) {
        /* We've lost focus while waiting for a key binding - restore the old value */
        config_text_reset(widget);
        g_object_set_data( G_OBJECT(widget), "keypress_mode", GINT_TO_POINTER(FALSE) );
        input_set_keysym_hook(NULL,NULL);
    }
    return TRUE;
}

static gboolean path_file_button_clicked( GtkWidget *button, gpointer user_data )
{
    GtkWidget *entry = GTK_WIDGET(user_data);
    GtkWidget *file = gtk_file_chooser_dialog_new( _("Select file"), NULL,
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
            NULL );
    gchar *filename = get_expanded_path(gtk_entry_get_text(GTK_ENTRY(entry)));
    gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(file), filename );
    gtk_window_set_modal( GTK_WINDOW(file), TRUE );
    gtk_widget_show_all( file );
    gint result = gtk_dialog_run(GTK_DIALOG(file));
    g_free(filename);
    if( result == GTK_RESPONSE_ACCEPT ) {
        filename = get_escaped_path(gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(file) ));
        config_set_field( entry, filename );
        g_free(filename);
    }
    gtk_widget_destroy(file);
    return TRUE;
}

static gboolean path_dir_button_clicked( GtkWidget *button, gpointer user_data )
{
    GtkWidget *entry = GTK_WIDGET(user_data);
    GtkWidget *file = gtk_file_chooser_dialog_new( _("Select file"), NULL,
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
            NULL );
    gchar *filename = get_expanded_path(gtk_entry_get_text(GTK_ENTRY(entry)));
    gtk_file_chooser_set_action( GTK_FILE_CHOOSER(file), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER );
    gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(file), filename );
    gtk_window_set_modal( GTK_WINDOW(file), TRUE );
    gtk_widget_show_all( file );
    gint result = gtk_dialog_run(GTK_DIALOG(file));
    g_free(filename);
    if( result == GTK_RESPONSE_ACCEPT ) {
        filename = get_escaped_path(gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(file) ));
        config_set_field( entry, filename );
        g_free(filename);
    }
    gtk_widget_destroy(file);
    return TRUE;
}


static void lxdream_configuration_panel_destroy( GtkWidget *panel, gpointer data )
{
    input_set_keysym_hook(NULL, NULL);
}

static GtkWidget *gtk_configuration_panel_new( lxdream_config_group_t conf )
{
    int count, i;
    for( count=0; conf->params[count].label != NULL; count++ );

    GtkWidget *table = gtk_table_new( count, 5, FALSE );
    struct config_data *data = g_malloc0( sizeof(struct config_data) );
    data->config = conf;
    GList *focus_chain = NULL;
    gtk_object_set_data_full( GTK_OBJECT(table), "config_data", data, g_free );
    g_signal_connect( table, "destroy_event", G_CALLBACK(lxdream_configuration_panel_destroy), NULL );
    for( i=0; conf->params[i].label != NULL; i++ ) {
        GtkWidget *text, *text2, *button;
        int x=0;
        int y=i;
        gtk_table_attach( GTK_TABLE(table), gtk_label_new(Q_(conf->params[i].label)), x, x+1, y, y+1,
                          GTK_SHRINK, GTK_SHRINK, 0, 0 );
        switch( conf->params[i].type ) {
        case CONFIG_TYPE_KEY:
            data->fields[i][0] = text = gtk_entry_new();
            gtk_entry_set_width_chars( GTK_ENTRY(text), 11 );
            gtk_entry_set_editable( GTK_ENTRY(text), FALSE );
            g_signal_connect( text, "key_press_event",
                              G_CALLBACK(config_key_keypress), NULL );
            g_signal_connect( text, "button_press_event",
                              G_CALLBACK(config_key_buttonpress), NULL );
            g_signal_connect( text, "focus_out_event",
                              G_CALLBACK(config_key_unfocus), NULL);
            g_object_set_data( G_OBJECT(text), "keypress_mode", GINT_TO_POINTER(FALSE) );
            g_object_set_data( G_OBJECT(text), "tag", GINT_TO_POINTER(i) );
            gtk_table_attach_defaults( GTK_TABLE(table), text, x+1, x+2, y, y+1);
            focus_chain = g_list_append( focus_chain, text );

            data->fields[i][1] = text2 = gtk_entry_new();
            gtk_entry_set_width_chars( GTK_ENTRY(text2), 11 );
            gtk_entry_set_editable( GTK_ENTRY(text2), FALSE );
            g_signal_connect( text2, "key_press_event",
                              G_CALLBACK(config_key_keypress), NULL );
            g_signal_connect( text2, "button_press_event",
                              G_CALLBACK(config_key_buttonpress), NULL );
            g_signal_connect( text2, "focus_out_event",
                              G_CALLBACK(config_key_unfocus), NULL);
            g_object_set_data( G_OBJECT(text2), "keypress_mode", GINT_TO_POINTER(FALSE) );
            g_object_set_data( G_OBJECT(text2), "tag", GINT_TO_POINTER(i) );
            gtk_table_attach_defaults( GTK_TABLE(table), text2, x+2, x+3, y, y+1);
            focus_chain = g_list_append( focus_chain, text2 );

            if( conf->params[i].value != NULL ) {
                gchar **parts = g_strsplit(conf->params[i].value,",",3);
                if( parts[0] != NULL ) {
                    gtk_entry_set_text( GTK_ENTRY(text), g_strstrip(parts[0]) );
                    if( parts[1] != NULL ) {
                        gtk_entry_set_text( GTK_ENTRY(text2), g_strstrip(parts[1]) );
                    }
                }
                g_strfreev(parts);
            }
            break;
        case CONFIG_TYPE_FILE:
        case CONFIG_TYPE_PATH:
            data->fields[i][0] = text = gtk_entry_new();
            data->fields[i][1] = NULL;
            button = gtk_button_new();
            gtk_entry_set_text( GTK_ENTRY(text), conf->params[i].value );
            gtk_entry_set_width_chars( GTK_ENTRY(text), 48 );
            g_object_set_data( G_OBJECT(text), "tag", GINT_TO_POINTER(i) );
            gtk_table_attach_defaults( GTK_TABLE(table), text, 1, 2, y, y+1 );
            gtk_table_attach( GTK_TABLE(table), button, 2, 3, y, y+1, GTK_SHRINK, GTK_SHRINK, 0, 0 );
            g_signal_connect( text, "changed", G_CALLBACK(config_text_changed), NULL );
            if( conf->params[i].type == CONFIG_TYPE_FILE ) {
                GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_FILE, GTK_ICON_SIZE_MENU);
                gtk_button_set_image( GTK_BUTTON(button), image );
                g_signal_connect( button, "clicked", G_CALLBACK(path_file_button_clicked), text );
            } else {
                GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU);
                gtk_button_set_image( GTK_BUTTON(button), image );
                g_signal_connect( button, "clicked", G_CALLBACK(path_dir_button_clicked), text );
            }
            break;
        }
    }
    gtk_container_set_focus_chain( GTK_CONTAINER(table), focus_chain );
//    gtk_gui_run_property_dialog( _("Controller Configuration"), table, controller_config_done );
    return table;
}

int gtk_configuration_panel_run( const gchar *title, lxdream_config_group_t group )
{
    struct lxdream_config_group tmp;
    lxdream_clone_config_group( &tmp, group );
    GtkWidget *panel = gtk_configuration_panel_new( &tmp );
    int result = gtk_gui_run_property_dialog( title, panel, NULL );
    if( result == GTK_RESPONSE_ACCEPT ) {
        lxdream_copy_config_group( group, &tmp );
        lxdream_save_config();
    }
    return result;
}
