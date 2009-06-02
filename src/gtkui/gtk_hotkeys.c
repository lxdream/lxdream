/**
 * $Id:  $
 *
 * GTK dialog for defining hotkeys
 *
 * Copyright (c) 2009 wahrhaft.
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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "lxdream.h"
#include "display.h"
#include "gtkui/gtkui.h"
#include "hotkeys.h"



static void config_keysym_hook( void *data, const gchar *keysym )
{
    GtkWidget *widget = (GtkWidget *)data;
    gtk_entry_set_text( GTK_ENTRY(widget), keysym );
    g_object_set_data( G_OBJECT(widget), "keypress_mode", GINT_TO_POINTER(FALSE) );
    input_set_keysym_hook(NULL, NULL);
}

static gboolean config_key_buttonpress( GtkWidget *widget, GdkEventButton *event, gpointer user_data )
{
    gboolean keypress_mode = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(widget), "keypress_mode"));
    if( keypress_mode ) {
        gchar *keysym = input_keycode_to_keysym( &system_mouse_driver, event->button);
        if( keysym != NULL ) {
            config_keysym_hook( widget, keysym );
            g_free(keysym);
        }
        return TRUE;
    } else {
        gtk_entry_set_text( GTK_ENTRY(widget), _("<press key>") );
        g_object_set_data( G_OBJECT(widget), "keypress_mode", GINT_TO_POINTER(TRUE) );
        input_set_keysym_hook(config_keysym_hook, widget);
    }
    return FALSE;
}

static gboolean config_key_keypress( GtkWidget *widget, GdkEventKey *event, gpointer user_data )
{
    gboolean keypress_mode = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(widget), "keypress_mode"));
    if( keypress_mode ) {
        if( event->keyval == GDK_Escape ) {
            gtk_entry_set_text( GTK_ENTRY(widget), "" );
            g_object_set_data( G_OBJECT(widget), "keypress_mode", GINT_TO_POINTER(FALSE) );
            return TRUE;
        }
        GdkKeymap *keymap = gdk_keymap_get_default();
        guint keyval;

        gdk_keymap_translate_keyboard_state( keymap, event->hardware_keycode, 0, 0, &keyval, 
                                             NULL, NULL, NULL );
        gtk_entry_set_text( GTK_ENTRY(widget), gdk_keyval_name(keyval) );
        g_object_set_data( G_OBJECT(widget), "keypress_mode", GINT_TO_POINTER(FALSE) );
        input_set_keysym_hook(NULL, NULL);
        return TRUE;
    } else {
        switch( event->keyval ) {
        case GDK_Return:
        case GDK_KP_Enter:
            gtk_entry_set_text( GTK_ENTRY(widget), _("<press key>") );
            g_object_set_data( G_OBJECT(widget), "keypress_mode", GINT_TO_POINTER(TRUE) );
            input_set_keysym_hook(config_keysym_hook, widget);
            return TRUE;
        case GDK_BackSpace:
        case GDK_Delete:
            gtk_entry_set_text( GTK_ENTRY(widget), "" );
            return TRUE;
        }
        return FALSE;
    }

}

void hotkeys_dialog_done( GtkWidget *panel, gboolean isOK )
{
    if( isOK ) {
        hotkeys_unregister_keys();
        lxdream_config_entry_t conf = hotkeys_get_config();
        int i;
        for( i=0; conf[i].key != NULL; i++ ) {
            char buf[64];
            GtkWidget *entry1, *entry2;
            const gchar *key1, *key2;
            snprintf( buf, sizeof(buf), "%s.1", conf[i].key );
            entry1 = GTK_WIDGET(g_object_get_qdata( G_OBJECT(panel), g_quark_from_string(buf)));
            key1 = gtk_entry_get_text(GTK_ENTRY(entry1));
            snprintf( buf, sizeof(buf), "%s.2", conf[i].key );
            entry2 = GTK_WIDGET(g_object_get_qdata( G_OBJECT(panel), g_quark_from_string(buf)));
            key2 = gtk_entry_get_text(GTK_ENTRY(entry2));
            if( key1 == NULL || key1[0] == '\0') {
                lxdream_set_config_value( &conf[i], key2 );
            } else if( key2 == NULL || key2[0] == '\0') {
                lxdream_set_config_value( &conf[i], key1 );
            } else {
                char buf[64];
                snprintf( buf, sizeof(buf), "%s, %s", key1, key2 );
                lxdream_set_config_value( &conf[i], buf );
            }
        }
        lxdream_save_config();
        hotkeys_register_keys();
    }
}

GtkWidget *hotkeys_panel_new()
{
    lxdream_config_entry_t conf = hotkeys_get_config();
    int count, i;
    for( count=0; conf[count].key != NULL; count++ );

    GtkWidget *table = gtk_table_new( (count+1)>>1, 6, FALSE );
    GList *focus_chain = NULL;
    //gtk_object_set_data( GTK_OBJECT(table), "maple_device", device );
    for( i=0; i<count; i++ ) {
        GtkWidget *text, *text2;
        char buf[64];
        int x=0;
        int y=i;
        if( i >= (count+1)>>1 ) {
            x = 3;
            y -= (count+1)>>1;
        }
        gtk_table_attach( GTK_TABLE(table), gtk_label_new(gettext(conf[i].label)), x, x+1, y, y+1, 
                          GTK_SHRINK, GTK_SHRINK, 0, 0 );
        text = gtk_entry_new();
        gtk_entry_set_width_chars( GTK_ENTRY(text), 11 );
        gtk_entry_set_editable( GTK_ENTRY(text), FALSE );
        g_signal_connect( text, "key_press_event", 
                          G_CALLBACK(config_key_keypress), NULL );
        g_signal_connect( text, "button_press_event",
                          G_CALLBACK(config_key_buttonpress), NULL );
        snprintf( buf, sizeof(buf), "%s.1", conf[i].key );
        g_object_set_data( G_OBJECT(text), "keypress_mode", GINT_TO_POINTER(FALSE) );
        g_object_set_qdata( G_OBJECT(table), g_quark_from_string(buf), text );
        gtk_table_attach_defaults( GTK_TABLE(table), text, x+1, x+2, y, y+1);
        focus_chain = g_list_append( focus_chain, text );
        text2 = gtk_entry_new();
        gtk_entry_set_width_chars( GTK_ENTRY(text2), 11 );
        gtk_entry_set_editable( GTK_ENTRY(text2), FALSE );
        g_signal_connect( text2, "key_press_event", 
                          G_CALLBACK(config_key_keypress), NULL );
        g_signal_connect( text2, "button_press_event",
                          G_CALLBACK(config_key_buttonpress), NULL );
        snprintf( buf, sizeof(buf), "%s.2", conf[i].key );
        g_object_set_data( G_OBJECT(text2), "keypress_mode", GINT_TO_POINTER(FALSE) );
        g_object_set_qdata( G_OBJECT(table), g_quark_from_string(buf), text2 );
        gtk_table_attach_defaults( GTK_TABLE(table), text2, x+2, x+3, y, y+1);
        focus_chain = g_list_append( focus_chain, text2 );
        if( conf[i].value != NULL ) {
            gchar **parts = g_strsplit(conf[i].value,",",3);
            if( parts[0] != NULL ) {
                gtk_entry_set_text( GTK_ENTRY(text), g_strstrip(parts[0]) );
                if( parts[1] != NULL ) {
                    gtk_entry_set_text( GTK_ENTRY(text2), g_strstrip(parts[1]) );
                }
            }
            g_strfreev(parts);
        }
    }
    gtk_container_set_focus_chain( GTK_CONTAINER(table), focus_chain );

    return table;
}

void hotkeys_dialog_run( )
{
    gtk_gui_run_property_dialog( _("Hotkey Settings"), hotkeys_panel_new(), hotkeys_dialog_done );
}
