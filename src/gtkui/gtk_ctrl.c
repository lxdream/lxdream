/**
 * $Id$
 *
 * Define the main (emu) GTK window, along with its menubars,
 * toolbars, etc.
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

#include <assert.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "lxdream.h"
#include "display.h"
#include "gtkui/gtkui.h"
#include "maple/maple.h"

#define MAX_DEVICES 4

static void controller_device_configure(maple_device_t device);

struct maple_config_class {
    const char *name;
    void (*config_func)(maple_device_t device);
};

typedef struct maple_slot_data {
    maple_device_t old_device;
    maple_device_t new_device;
    GtkWidget *button;
    GtkWidget *combo;
} *maple_slot_data_t;

static struct maple_config_class maple_device_config[] = {
        { "Sega Controller", controller_device_configure },
        { NULL, NULL } };

static struct maple_slot_data maple_data[MAX_DEVICES];

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

static void controller_config_done( GtkWidget *panel, gboolean isOK )
{
    if( isOK ) {
        maple_device_t device = (maple_device_t)gtk_object_get_data( GTK_OBJECT(panel), "maple_device" );
        lxdream_config_entry_t conf = device->get_config(device);
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
    }
    input_set_keysym_hook(NULL, NULL);
}

static void controller_device_configure( maple_device_t device )
{
    lxdream_config_entry_t conf = maple_get_device_config(device);
    int count, i;
    for( count=0; conf[count].key != NULL; count++ );

    GtkWidget *table = gtk_table_new( (count+1)>>1, 6, FALSE );
    GList *focus_chain = NULL;
    gtk_object_set_data( GTK_OBJECT(table), "maple_device", device );
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
    gtk_gui_run_property_dialog( _("Controller Configuration"), table, controller_config_done );
}

gboolean maple_properties_activated( GtkButton *button, gpointer user_data )
{
    maple_slot_data_t data = (maple_slot_data_t)user_data;
    if( data->new_device != NULL ) {
        int i;
        for( i=0; maple_device_config[i].name != NULL; i++ ) {
            if( strcmp(data->new_device->device_class->name, maple_device_config[i].name) == 0 ) {
                if( data->new_device == data->old_device ) {
                    // Make a copy at this point if we haven't already
                    data->new_device = data->old_device->clone(data->old_device);
                }
                maple_device_config[i].config_func(data->new_device);
                break;
            }
        }
        if( maple_device_config[i].name == NULL ) {
            gui_error_dialog( _("No configuration page available for device type") );
        }
    }
    return TRUE;
}

gboolean maple_device_changed( GtkComboBox *combo, gpointer user_data )
{
    maple_slot_data_t data = (maple_slot_data_t)user_data;
    int active = gtk_combo_box_get_active(combo);
    gboolean has_config = FALSE;
    if( active != 0 ) {
        gchar *devname = gtk_combo_box_get_active_text(combo);
        const maple_device_class_t devclz = maple_get_device_class(devname);
        assert(devclz != NULL);
        if( data->new_device != NULL ) {
            if( data->new_device->device_class != devclz ) {
                if( data->new_device != data->old_device ) {
                    data->new_device->destroy(data->new_device);
                }
                data->new_device = maple_new_device(devname);
            }
        } else {
            data->new_device = maple_new_device(devname);
        }
        has_config = data->new_device != NULL && data->new_device->get_config != NULL;
    } else {
        if( data->new_device != NULL && data->new_device != data->old_device ) {
            data->new_device->destroy(data->new_device);
        }
        data->new_device = NULL;
    }
    gtk_widget_set_sensitive(data->button, has_config);
    return TRUE;
}

void maple_dialog_done( GtkWidget *panel, gboolean isOK )
{
    if( isOK ) {
        int i;
        for( i=0; i<MAX_DEVICES; i++ ) {
            if( maple_data[i].new_device != maple_data[i].old_device ) {
                if( maple_data[i].old_device != NULL ) {
                    maple_detach_device(i,0);
                }
                if( maple_data[i].new_device != NULL ) {
                    maple_attach_device(maple_data[i].new_device, i, 0 );
                }
            }
        }
        lxdream_save_config();
    } else {
        int i;
        for( i=0; i<MAX_DEVICES; i++ ) {
            if( maple_data[i].new_device != NULL && 
                    maple_data[i].new_device != maple_data[i].old_device ) {
                maple_data[i].new_device->destroy(maple_data[i].new_device);
            }
        }
    }

}

GtkWidget *maple_panel_new()
{
    GtkWidget *table = gtk_table_new(4, 3, TRUE);
    int i,j;
    const struct maple_device_class **devices = maple_get_device_classes();

    for( i=0; i< MAX_DEVICES; i++ ) {
        char buf[16];
        GtkWidget *combo, *button;
        int active = 0;
        maple_device_t device = maple_get_device(i,0);
        snprintf( buf, sizeof(buf), _("Slot %d."), i );
        gtk_table_attach_defaults( GTK_TABLE(table), gtk_label_new(buf), 0, 1, i, i+1 );
        combo = gtk_combo_box_new_text();
        gtk_combo_box_append_text( GTK_COMBO_BOX(combo), _("<empty>") );
        for( j=0; devices[j] != NULL; j++ ) {
            gtk_combo_box_append_text(GTK_COMBO_BOX(combo), devices[j]->name);
            if( device != NULL && device->device_class == devices[j] ) {
                active = j+1;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
        gtk_table_attach_defaults( GTK_TABLE(table), combo, 1, 2, i, i+1 );
        button = gtk_button_new_from_stock( GTK_STOCK_PROPERTIES );
        gtk_widget_set_sensitive(button, active != 0 && device->get_config != NULL);
        gtk_table_attach_defaults( GTK_TABLE(table), button, 2, 3, i, i+1 );

        maple_data[i].old_device = device;
        maple_data[i].new_device = device;
        maple_data[i].combo = combo;
        maple_data[i].button = button;
        g_signal_connect( button, "clicked", 
                          G_CALLBACK( maple_properties_activated ), &maple_data[i] );
        g_signal_connect( combo, "changed", 
                          G_CALLBACK( maple_device_changed ), &maple_data[i] );

    }
    return table;
}

void maple_dialog_run( )
{
    gtk_gui_run_property_dialog( _("Controller Settings"), maple_panel_new(), maple_dialog_done );
}
