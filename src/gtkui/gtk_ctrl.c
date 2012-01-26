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
#include <errno.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "lxdream.h"
#include "display.h"
#include "gtkui/gtkui.h"
#include "maple/maple.h"
#include "vmu/vmulist.h"

#define MAX_DEVICES 4

#define LOAD_VMU_TAG ((void *)-1)
#define CREATE_VMU_TAG ((void *)-2)

static void maple_set_device_selection( GtkWidget *combo, maple_device_t device );

struct maple_config_class {
    const char *name;
    void (*config_func)(maple_device_t device);
};

typedef struct maple_slot_data {
    maple_device_t old_device;
    maple_device_t new_device;
    GtkWidget *button;
    GtkWidget *combo;
    gboolean primarySlot;
} *maple_slot_data_t;


static struct maple_slot_data maple_data[MAPLE_MAX_DEVICES];

/**
 * Flag set when changing the selection on one of the combo boxes manually -
 * avoids the followup changed event.
 */
static gboolean maple_device_adjusting = FALSE;

static gboolean maple_properties_activated( GtkButton *button, gpointer user_data )
{
    maple_slot_data_t data = (maple_slot_data_t)user_data;
    if( data->new_device != NULL ) {
        int i;
        lxdream_config_group_t config = data->new_device->get_config(data->new_device);
        if( config != NULL ) {

            if( data->new_device == data->old_device ) {
            // Make a copy at this point if we haven't already
                data->new_device = data->old_device->clone(data->old_device);
            }

            gtk_configuration_panel_run(_("Controller Configuration"), config);
        }
    }
    return TRUE;
}

static gboolean maple_device_changed( GtkComboBox *combo, gpointer user_data )
{
    if( maple_device_adjusting ) {
        return TRUE;
    }
    
    maple_slot_data_t data = (maple_slot_data_t)user_data;
    int active = gtk_combo_box_get_active(combo), i;
    gboolean has_config = FALSE;
    gboolean set_selection = FALSE;
    int has_slots = 0;
    if( active != 0 ) {
        GtkTreeIter iter;
        maple_device_class_t devclz;
        const gchar *vmu_filename;
        
        GtkTreeModel *model = gtk_combo_box_get_model(combo);
        gtk_combo_box_get_active_iter(combo, &iter);
        gtk_tree_model_get(model, &iter, 1, &devclz, 2, &vmu_filename, -1 );
        
        if( devclz == LOAD_VMU_TAG ) {
            devclz = NULL;
            vmu_filename = open_file_dialog( _("Load VMU"), "*.vmu", "VMU Files",
                    CONFIG_VMU_PATH );
            if( vmu_filename != NULL ) {
                vmu_volume_t vol = vmu_volume_load( vmu_filename );
                if( vol != NULL ) {
                    devclz = &vmu_class;
                    vmulist_add_vmu(vmu_filename, vol);
                    set_selection = TRUE;
                } else {
                    ERROR( "Unable to load VMU file (not a valid VMU)" );
                }
            }
        } else if( devclz == CREATE_VMU_TAG ) {
            devclz = NULL;
            vmu_filename = save_file_dialog( _("Create VMU"), "*.vmu", "VMU Files", ".vmu",
                    CONFIG_VMU_PATH );
            if( vmu_filename != NULL ) {
                devclz = &vmu_class;
                set_selection = TRUE;
                int idx = vmulist_create_vmu( vmu_filename, FALSE );
                if( idx == -1 ) {
                    ERROR( "Unable to save VMU file: %s", strerror(errno) );
                }
            }
        } else if( vmu_filename != NULL ) {
            devclz = &vmu_class;
        }

        if( devclz == NULL ) {
            maple_set_device_selection(data->combo, data->new_device);
            return TRUE;
        }

        if( data->new_device != NULL ) {
            if( data->new_device->device_class != devclz ) {
                if( data->new_device != data->old_device ) {
                    data->new_device->destroy(data->new_device);
                }
                data->new_device = devclz->new_device();
            }
        } else {
            data->new_device = devclz->new_device();
        }
        has_config = data->new_device != NULL && data->new_device->get_config != NULL && !MAPLE_IS_VMU(data->new_device);
        has_slots = data->new_device == NULL ? 0 : MAPLE_SLOTS(devclz);
        if( MAPLE_IS_VMU(data->new_device) ) {
            for( i=0; i<MAPLE_MAX_DEVICES; i++ ) {
                if( maple_data[i].new_device != NULL && MAPLE_IS_VMU(maple_data[i].new_device) &&
                        MAPLE_VMU_HAS_NAME(maple_data[i].new_device, vmu_filename) ) {
                    maple_data[i].new_device->destroy(maple_data[i].new_device);
                    maple_data[i].new_device = NULL;
                    gtk_combo_box_set_active(GTK_COMBO_BOX(maple_data[i].combo),0);
                }
            }
            MAPLE_SET_VMU_NAME(data->new_device,vmu_filename);
        }
        
        if( set_selection ) {
            maple_set_device_selection(data->combo, data->new_device);
        }
    } else {
        if( data->new_device != NULL && data->new_device != data->old_device ) {
            data->new_device->destroy(data->new_device);
        }
        data->new_device = NULL;
    }
    gtk_widget_set_sensitive(data->button, has_config);

    if( data->primarySlot ) {
        for( i=0; i<MAPLE_USER_SLOTS; i++ ) {
            /* This is a little morally dubious... */
            maple_slot_data_t subdata = data + MAPLE_DEVID(0,(i+1));
            gtk_widget_set_sensitive(subdata->combo, i < has_slots );
            gtk_widget_set_sensitive(subdata->button, i < has_slots && subdata->new_device != NULL && subdata->new_device->get_config != NULL && !MAPLE_IS_VMU(subdata->new_device) );
        }
    }
    return TRUE;
}

static void maple_build_device_model( GtkListStore *dev_model )
{
    const struct maple_device_class **devices = maple_get_device_classes();
    int i;
    
    gtk_list_store_clear(dev_model);
    gtk_list_store_insert_with_values( dev_model, NULL, 0, 0, _("<empty>"), 1, NULL, 2, NULL, -1 );
    for( i=0; devices[i] != NULL; i++ ) {
        if( devices[i]->flags & MAPLE_TYPE_PRIMARY ) {
            gtk_list_store_insert_with_values( dev_model, NULL, i+1, 0, devices[i]->name, 1, devices[i], 2, NULL, -1 );
        }
    }
    
}

/**
 * (Re)build the subdevice combo-box model. 
 */
static void maple_build_subdevice_model( GtkListStore *subdev_model )
{
    int i, j;
    const struct maple_device_class **devices = maple_get_device_classes();
    
    gtk_list_store_clear(subdev_model);
    gtk_list_store_insert_with_values( subdev_model, NULL, 0, 0, _("<empty>"), 1, NULL, 2, NULL, -1 );
    for( i=0; devices[i] != NULL; i++ ) {
        if( !(devices[i]->flags & MAPLE_TYPE_PRIMARY) && !MAPLE_IS_VMU_CLASS(devices[i]) ) {
            gtk_list_store_insert_with_values( subdev_model, NULL, i+1, 0, devices[i]->name, 1, devices[i], 2, NULL, -1 );
        }
    }
    for( j=0; j < vmulist_get_size(); j++ ) {
        gtk_list_store_insert_with_values( subdev_model, NULL, i+j+1, 0, vmulist_get_name(j), 1, NULL, 2, vmulist_get_filename(j), -1 );
    }
    gtk_list_store_insert_with_values( subdev_model, NULL, i+j+1, 0, _("Load VMU..."), 1, LOAD_VMU_TAG, 2, NULL, -1 );
    gtk_list_store_insert_with_values( subdev_model, NULL, i+j+2, 0, _("Create VMU..."), 1, CREATE_VMU_TAG, 2, NULL, -1 );
}

static gboolean maple_vmulist_changed( vmulist_change_type_t type, int idx, void *data )
{
    GtkListStore *list = (GtkListStore *)data;
    GtkTreeIter iter;
    int i,j;
    
    /* Search for the row and update accordingly. There's probably better ways
     * to do this 
     */
    
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &iter);
    while( valid ) {
        gchar *vmu_filename;
        gpointer devclz;
        gtk_tree_model_get(GTK_TREE_MODEL(list), &iter, 1, &devclz, 2, &vmu_filename, -1 );
        if( vmu_filename != NULL || devclz == LOAD_VMU_TAG || devclz == CREATE_VMU_TAG )
            break;
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &iter);
    }
    if( valid ) {
        for( i=0; i<idx && valid; i++ ) {
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &iter);
        }
    }
    if( valid ) {
        if( type == VMU_ADDED ) {
            GtkTreeIter newiter;
            gtk_list_store_insert_before(list, &newiter, &iter);
            gtk_list_store_set(list, &newiter, 0, vmulist_get_name(idx), 1, NULL, 2, vmulist_get_filename(idx), -1 );
        } else if( type == VMU_REMOVED ) {
            gtk_list_store_remove(list, &iter );
        }
    }
    return TRUE;
}

/**
 * Set the device popup selection based on the device (works for both primary
 * and secondary devices)
 */
static void maple_set_device_selection( GtkWidget *combo, maple_device_t device )
{
    GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
    GtkTreeIter iter;

    maple_device_adjusting = TRUE;
    if( device == NULL ) {
        gtk_combo_box_set_active( GTK_COMBO_BOX(combo), 0 );
    } else {
        gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
        while( valid ) {
            const struct maple_device_class *clz;
            const gchar *vmu_filename;

            gtk_tree_model_get(model, &iter, 1, &clz, 2, &vmu_filename, -1 );

            if( device->device_class == clz ) {
                gtk_combo_box_set_active_iter( GTK_COMBO_BOX(combo), &iter );
                break;
            } else if( vmu_filename != NULL && MAPLE_IS_VMU(device) && 
                    MAPLE_VMU_HAS_NAME(device, vmu_filename) ) {
                gtk_combo_box_set_active_iter( GTK_COMBO_BOX(combo), &iter );
                break;
            }

            valid = gtk_tree_model_iter_next(model, &iter);
        }
        if( !valid ) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
        }
    }
    maple_device_adjusting = FALSE;
}

static void maple_dialog_done( GtkWidget *panel, gboolean isOK )
{
    void *p = g_object_get_data( G_OBJECT(panel), "subdev_model" );
    unregister_vmulist_change_hook( maple_vmulist_changed, p );
    
    if( isOK ) {
        int i;
        for( i=0; i<MAPLE_MAX_DEVICES; i++ ) {
            if( maple_data[i].new_device != maple_data[i].old_device ) {
                if( maple_data[i].old_device != NULL ) {
                    maple_detach_device(MAPLE_DEVID_PORT(i),MAPLE_DEVID_SLOT(i));
                }
                if( maple_data[i].new_device != NULL ) {
                    maple_attach_device(maple_data[i].new_device, MAPLE_DEVID_PORT(i), MAPLE_DEVID_SLOT(i) );
                }
            }
        }
        lxdream_save_config();
    } else {
        int i;
        for( i=0; i<MAPLE_MAX_DEVICES; i++ ) {
            if( maple_data[i].new_device != NULL && 
                    maple_data[i].new_device != maple_data[i].old_device ) {
                maple_data[i].new_device->destroy(maple_data[i].new_device);
            }
        }
    }

}

static GtkWidget *maple_panel_new()
{
    GtkWidget *table = gtk_table_new( MAPLE_PORTS * (MAPLE_USER_SLOTS+1), 3, TRUE);
    int i,j,k;
    const struct maple_device_class **devices = maple_get_device_classes();
    
    gtk_table_set_row_spacings(GTK_TABLE(table), 3);
    gtk_table_set_col_spacings(GTK_TABLE(table), 5);
    maple_device_adjusting = FALSE;
    
    /* Device models */
    GtkListStore *dev_model = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING);
    maple_build_device_model(dev_model);
    
    GtkListStore *subdev_model = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING);
    maple_build_subdevice_model(subdev_model);
    g_object_set_data( G_OBJECT(table), "subdev_model", subdev_model ); 
    register_vmulist_change_hook( maple_vmulist_changed, subdev_model );
    
    int y =0;
    for( i=0; i< MAPLE_PORTS; i++ ) {
        char buf[16];
        GtkWidget *combo, *button;
        int length = 1;
        maple_device_t device = maple_get_device(i,0);
        int has_slots = device == NULL ? 0 : MAPLE_SLOTS(device->device_class);

        snprintf( buf, sizeof(buf), _("Port %c."), 'A'+i );
        gtk_table_attach_defaults( GTK_TABLE(table), gtk_label_new(buf), 0, 1, y, y+1 );

        combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(dev_model));
        GtkCellRenderer *rend = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(combo), rend, TRUE);
        gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT(combo), rend, "text", 0 );
        maple_set_device_selection(combo,device);
        gtk_table_attach_defaults( GTK_TABLE(table), combo, 1, 2, y, y+1 );

        button = gtk_button_new_from_stock( GTK_STOCK_PROPERTIES );
        gtk_widget_set_sensitive(button, device != NULL && device->get_config != NULL);
        gtk_table_attach_defaults( GTK_TABLE(table), button, 2, 3, y, y+1 );

        maple_data[MAPLE_DEVID(i,0)].old_device = device;
        maple_data[MAPLE_DEVID(i,0)].new_device = device;
        maple_data[MAPLE_DEVID(i,0)].combo = combo;
        maple_data[MAPLE_DEVID(i,0)].button = button;
        maple_data[MAPLE_DEVID(i,0)].primarySlot = TRUE;
        g_signal_connect( button, "clicked", 
                          G_CALLBACK( maple_properties_activated ), &maple_data[MAPLE_DEVID(i,0)] );
        g_signal_connect( combo, "changed", 
                          G_CALLBACK( maple_device_changed ), &maple_data[MAPLE_DEVID(i,0)] );
        y++;
        
        for( k=0; k< MAPLE_USER_SLOTS; k++ ) {
            char tmp[32] = "        ";
            device = maple_get_device(i,k+1);
            snprintf( tmp+8, sizeof(tmp)-8, _("VMU %d."), (k+1) );
            gtk_table_attach_defaults( GTK_TABLE(table), gtk_label_new(tmp), 0, 1, y, y+1 );
            combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(subdev_model));
            GtkCellRenderer *rend = gtk_cell_renderer_text_new();
            gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(combo), rend, TRUE);
            gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT(combo), rend, "text", 0 );
            maple_set_device_selection(combo, device);
            
            gtk_table_attach_defaults( GTK_TABLE(table), combo, 1, 2, y, y+1 );
            button = gtk_button_new_from_stock( GTK_STOCK_PROPERTIES );
            gtk_table_attach_defaults( GTK_TABLE(table), button, 2, 3, y, y+1 );
            if( k >= has_slots ) {
                gtk_widget_set_sensitive(combo, FALSE);
                gtk_widget_set_sensitive(button, FALSE);
            } else {
                gtk_widget_set_sensitive(button, device != NULL && device->get_config != NULL && !MAPLE_IS_VMU(device));
            }            
            
            maple_data[MAPLE_DEVID(i,k+1)].old_device = device;
            maple_data[MAPLE_DEVID(i,k+1)].new_device = device;
            maple_data[MAPLE_DEVID(i,k+1)].combo = combo;
            maple_data[MAPLE_DEVID(i,k+1)].button = button;
            maple_data[MAPLE_DEVID(i,k+1)].primarySlot = FALSE;
            g_signal_connect( button, "clicked", 
                              G_CALLBACK( maple_properties_activated ), &maple_data[MAPLE_DEVID(i,k+1)] );
            g_signal_connect( combo, "changed", 
                              G_CALLBACK( maple_device_changed ), &maple_data[MAPLE_DEVID(i,k+1)] );
            y++;
        }
        gtk_table_set_row_spacing( GTK_TABLE(table), y-1, 10 );
    }
    return table;
}

void maple_dialog_run( )
{
    gtk_gui_run_property_dialog( _("Controller Settings"), maple_panel_new(), maple_dialog_done );
}
