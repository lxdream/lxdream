/**
 * $Id$
 *
 * Implements the MMIO register viewing window
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

#include <stdint.h>
#include <string.h>
#include <glib/gi18n.h>
#include "gtkui/gtkui.h"
#include "mem.h"
#include "mmio.h"


struct mmio_window_info {
    GtkWidget *window;
    GtkWidget *notebook;
};

static void printbits( char *out, int nbits, uint32_t value )
{
    if( nbits < 32 ) {
        int i;
        for( i=32; i>nbits; i-- ) {
            if( !(i % 8) ) *out++ = ' ';
            *out++ = ' ';
        }
    }
    while( nbits > 0 ) {
        *out++ = (value&(1<<--nbits) ? '1' : '0');
        if( !(nbits % 8) ) *out++ = ' ';
    }
    *out = '\0';
}

static void printhex( char *out, int nbits, uint32_t value )
{
    char tmp[10], *p = tmp;
    int i;

    sprintf( tmp, "%08X", value );
    for( i=32; i>0; i-=4, p++ ) {
        if( i <= nbits ) *out++ = *p;
        else *out++ = ' ';
    }
    *out = '\0';
}




gboolean
on_mmio_delete_event                (GtkWidget       *widget,
                                     GdkEvent        *event,
                                     gpointer         user_data)
{
    gtk_widget_hide(widget);
    return TRUE;
}


void on_mmio_close_clicked( GtkButton *button, gpointer user_data)
{
    gtk_widget_hide( ((mmio_window_t)user_data)->window );
}


void on_trace_button_toggled           (GtkToggleButton *button,
                                        gpointer user_data)
{
    struct mmio_region *io_rgn = (struct mmio_region *)user_data;
    gboolean isActive = gtk_toggle_button_get_active(button);
    if( io_rgn != NULL ) {
        io_rgn->trace_flag = isActive ? 1 : 0;
    }
}

static GtkCList *mmio_window_add_page( mmio_window_t mmio, char *name, struct mmio_region *io_rgn )
{
    GtkCList *list;
    GtkWidget *scroll;
    GtkWidget *tab;
    GtkCheckButton *trace_button;
    GtkVBox *vbox;

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );
    list = GTK_CLIST(gtk_clist_new(5));
    gtk_clist_set_column_width(list, 0, 70);
    gtk_clist_set_column_width(list, 1, 75);
    gtk_clist_set_column_width(list, 2, 70);
    gtk_clist_set_column_width(list, 3, 280);
    gtk_clist_set_column_width(list, 4, 160);
    gtk_clist_set_column_justification(list, 0, GTK_JUSTIFY_CENTER );
    gtk_clist_set_column_justification(list, 2, GTK_JUSTIFY_CENTER );
    gtk_clist_set_column_justification(list, 3, GTK_JUSTIFY_CENTER );
    gtk_clist_set_column_title(list, 0, _("Address"));
    gtk_clist_set_column_title(list, 1, _("Register"));
    gtk_clist_set_column_title(list, 2, _("Value"));
    gtk_clist_set_column_title(list, 3, _("Bit Pattern"));
    gtk_clist_set_column_title(list, 4, _("Description"));
    gtk_clist_column_titles_show(list);
    gtk_widget_modify_font( GTK_WIDGET(list), gui_fixed_font );
    tab = gtk_label_new(_(name));
    gtk_container_add( GTK_CONTAINER(scroll), GTK_WIDGET(list) );

    vbox = GTK_VBOX(gtk_vbox_new( FALSE, 0 ));
    gtk_container_add( GTK_CONTAINER(vbox), GTK_WIDGET(scroll) );

    trace_button = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(_("Trace access")));
    if( io_rgn != NULL ) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(trace_button), 
                io_rgn->trace_flag ? TRUE : FALSE);
    }
    gtk_container_add( GTK_CONTAINER(vbox), GTK_WIDGET(trace_button) );
    gtk_box_set_child_packing( GTK_BOX(vbox), GTK_WIDGET(trace_button), 
                               FALSE, FALSE, 0, GTK_PACK_START );
    gtk_notebook_append_page( GTK_NOTEBOOK(mmio->notebook), GTK_WIDGET(vbox), tab );
    gtk_object_set_data( GTK_OBJECT(mmio->window), name, list );
    g_signal_connect ((gpointer) trace_button, "toggled",
                      G_CALLBACK (on_trace_button_toggled),
                      io_rgn);
    return list;
}



mmio_window_t mmio_window_new( const gchar *title )
{
    mmio_window_t mmio = g_malloc0( sizeof(struct mmio_window_info) );

    int i, j;
    GtkCList *all_list;
    GtkWidget *vbox1;
    GtkWidget *hbuttonbox1;
    GtkWidget *mmr_close;

    mmio->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (mmio->window), title);
    gtk_window_set_default_size (GTK_WINDOW (mmio->window), 600, 600);

    vbox1 = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (mmio->window), vbox1);

    mmio->notebook = gtk_notebook_new ();
    gtk_box_pack_start (GTK_BOX (vbox1), mmio->notebook, TRUE, TRUE, 0);
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (mmio->notebook), GTK_POS_LEFT);

    hbuttonbox1 = gtk_hbutton_box_new ();
    gtk_box_pack_start (GTK_BOX (vbox1), hbuttonbox1, FALSE, TRUE, 0);
    gtk_box_set_spacing (GTK_BOX (hbuttonbox1), 30);

    mmr_close = gtk_button_new_with_mnemonic (_("Close"));
    gtk_container_add (GTK_CONTAINER (hbuttonbox1), mmr_close);
    GTK_WIDGET_SET_FLAGS (mmr_close, GTK_CAN_DEFAULT);

    /* Add the mmio register data */
    all_list = mmio_window_add_page( mmio, "All", NULL );
    for( i=0; i < num_io_rgns; i++ ) {
        GtkCList *list = mmio_window_add_page( mmio, io_rgn[i]->id, io_rgn[i] );
        for( j=0; io_rgn[i]->ports[j].id != NULL; j++ ) {
            int sz = io_rgn[i]->ports[j].width;
            char addr[10], data[10], bits[40];
            char *arr[] = { addr, io_rgn[i]->ports[j].id, data, bits,
                    io_rgn[i]->ports[j].desc };
            sprintf( addr, "%08X",
                     io_rgn[i]->base + io_rgn[i]->ports[j].offset );
            printhex( data, sz, *io_rgn[i]->ports[j].val );
            printbits( bits, io_rgn[i]->ports[j].width,
                       *io_rgn[i]->ports[j].val );
            gtk_clist_append( list, arr );
            gtk_clist_append( all_list, arr );
        }
    }

    g_signal_connect ((gpointer) mmio->window, "delete_event",
                      G_CALLBACK (on_mmio_delete_event),
                      NULL);
    g_signal_connect ((gpointer) mmr_close, "clicked",
                      G_CALLBACK (on_mmio_close_clicked),
                      mmio);

    gtk_widget_show_all( mmio->window );
    return mmio;
}

void mmio_window_update( mmio_window_t mmio )
{
    int i,j, count = 0;
    GtkCList *page, *all_page;
    char data[10], bits[40];

    all_page = GTK_CLIST(gtk_object_get_data( GTK_OBJECT(mmio->window), "All" ));

    for( i=0; i < num_io_rgns; i++ ) {
        page = GTK_CLIST(gtk_object_get_data( GTK_OBJECT(mmio->window),
                io_rgn[i]->id ));
        for( j=0; io_rgn[i]->ports[j].id != NULL; j++ ) {
            if( *io_rgn[i]->ports[j].val !=
                *(uint32_t *)(io_rgn[i]->save_mem+io_rgn[i]->ports[j].offset)){
                int sz = io_rgn[i]->ports[j].width;
                /* Changed */
                printhex( data, sz, *io_rgn[i]->ports[j].val );
                printbits( bits, sz, *io_rgn[i]->ports[j].val );

                gtk_clist_set_text( page, j, 2, data );
                gtk_clist_set_text( page, j, 3, bits );
                gtk_clist_set_foreground( page, j, &gui_colour_changed );

                gtk_clist_set_text( all_page, count, 2, data );
                gtk_clist_set_text( all_page, count, 3, bits );
                gtk_clist_set_foreground( all_page, count, &gui_colour_changed );

            } else {
                gtk_clist_set_foreground( page, j, &gui_colour_normal );
                gtk_clist_set_foreground( all_page, count, &gui_colour_normal );
            }
            count++;
        }
        memcpy( io_rgn[i]->save_mem, io_rgn[i]->mem, PAGE_SIZE );
    }
}

void mmio_window_show( mmio_window_t mmio, gboolean show )
{
    if( show ) {
        gtk_widget_show( mmio->window );
    } else {
        gtk_widget_hide( mmio->window );
    }
}

