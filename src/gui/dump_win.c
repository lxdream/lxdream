/**
 * $Id: dump_win.c,v 1.6 2007-10-21 11:38:02 nkeynes Exp $
 *
 * Implements the memory dump window.
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

#include <gnome.h>
#include <ctype.h>
#include <assert.h>
#include "mem.h"
#include "gui/gtkui.h"

#define MAX_DUMP_SIZE 4096

#define DUMP_WINDOW_TAG 0xD4B9DA7A

struct dump_window_info {
    uint32_t _tag;
    uint32_t start;
    uint32_t end;
    int flags;
    unsigned char *data;

    GtkWidget *window;
    GtkWidget *fromInput, *toInput;
    GtkWidget *textArea;
    GtkTextTag *changedTag;
    GtkTextBuffer *textBuffer;
    struct dump_window_info *next;
};

static dump_window_t dump_list_head = NULL;

gboolean on_dump_window_delete_event( GtkWidget *widget, GdkEvent *event,
                                   gpointer user_data );
void on_dump_window_button_view_clicked( GtkWidget *widget, gpointer user_data );
void dump_window_set_text( dump_window_t data, unsigned char *old_data, unsigned char *new_data );


dump_window_t dump_window_new( const gchar *title )
{
    GtkWidget *vbox3;
    GtkWidget *hbox2;
    GtkWidget *dump_view_button;
    GtkWidget *scrolledwindow9;

    dump_window_t dump = g_malloc0( sizeof( struct dump_window_info ) );

    dump->_tag = DUMP_WINDOW_TAG;
    dump->next = dump_list_head;
    dump_list_head = dump;
    dump->data = NULL;
    dump->start = 0;
    dump->end = 0;
    dump->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (dump->window), _("Memory dump"));

    vbox3 = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (dump->window), vbox3);

    hbox2 = gtk_hbox_new (FALSE, 0);
    dump->fromInput = gtk_entry_new ();
    gtk_entry_set_text( GTK_ENTRY(dump->fromInput), "" );
    dump->toInput = gtk_entry_new ();
    gtk_entry_set_text( GTK_ENTRY(dump->toInput), "" );
    dump_view_button = gtk_button_new_with_mnemonic (_("View"));
  
    gtk_box_pack_start (GTK_BOX (hbox2), gtk_label_new(_(" From ")), FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), dump->fromInput, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), gtk_label_new(_(" To ")), FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), dump->toInput, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), dump_view_button, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), gtk_label_new (_("   ")), TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox3), hbox2, FALSE, TRUE, 3);

    dump->textArea = gtk_text_view_new ();
    dump->textBuffer = gtk_text_buffer_new(NULL);
    dump->changedTag = gtk_text_buffer_create_tag(dump->textBuffer, "changed",
						  "foreground", "blue", NULL);
    gtk_widget_modify_font(GTK_WIDGET(dump->textArea),gui_fixed_font);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(dump->textArea), FALSE);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(dump->textArea), dump->textBuffer);
    scrolledwindow9 = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow9), GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER (scrolledwindow9), dump->textArea);
    gtk_box_pack_start (GTK_BOX (vbox3), scrolledwindow9, TRUE, TRUE, 0);
        
    g_signal_connect (dump->window, "delete_event",
                      G_CALLBACK (on_dump_window_delete_event),
                      dump);
    g_signal_connect (dump_view_button, "clicked",
                      G_CALLBACK (on_dump_window_button_view_clicked),
                      dump);
    gtk_widget_show_all( dump->window );

    return dump;
}

void gtk_entry_set_hex_value( GtkEntry *entry, uint32_t value )
{
    char buf[10];
    sprintf( buf, "%08X", value );
    gtk_entry_set_text( entry, buf );
}

uint32_t gtk_entry_get_hex_value( GtkEntry *entry, uint32_t defaultValue )
{
    const gchar *text = gtk_entry_get_text(entry);
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

gboolean on_dump_window_delete_event( GtkWidget *widget, GdkEvent *event,
                                   gpointer user_data )
{
    dump_window_t data = (dump_window_t)user_data;
    if( data->data != NULL )
        free( data->data );
    dump_window_t node = dump_list_head;
    if( node == data )
        dump_list_head = data->next;
    else {
        while( node->next != data ) {
            node = node->next;
            assert( node != NULL );
        }
        node->next = data->next;
    }
    free( data );
    return FALSE;
}

void on_dump_window_button_view_clicked( GtkWidget *widget, gpointer user_data )
{
    dump_window_t data = (dump_window_t)user_data;
    uint32_t startVal, endVal;

    assert( data != NULL );
    assert( data->_tag == DUMP_WINDOW_TAG );
    
    startVal = gtk_entry_get_hex_value(GTK_ENTRY(data->fromInput), data->start);
    endVal = gtk_entry_get_hex_value(GTK_ENTRY(data->toInput), data->end);
    if( startVal != data->start || endVal != data->end ) {
        if( startVal > endVal ) {
            int tmp = endVal;
            endVal = startVal;
            startVal = tmp;
        }
        if( endVal > startVal + MAX_DUMP_SIZE )
            endVal = startVal + MAX_DUMP_SIZE;

        gtk_entry_set_hex_value(GTK_ENTRY(data->fromInput),startVal);
	gtk_entry_set_hex_value(GTK_ENTRY(data->toInput),endVal);
        data->start = startVal;
        data->end = endVal;

        if( data->data != NULL ) {
            free( data->data );
            data->data = NULL;
        }
        if( startVal != endVal ) {
            data->data = malloc( endVal - startVal );
            mem_copy_from_sh4( data->data, startVal, endVal-startVal );
            dump_window_set_text( data, data->data, data->data );
        }
    }
}

void dump_window_update( dump_window_t data )
{
    if( data->data == NULL )
        return;
    unsigned char tmp[data->end-data->start];
    int length = data->end-data->start;
    memcpy( tmp, data->data, length );
    mem_copy_from_sh4( data->data, data->start, length );
    dump_window_set_text( data, tmp, data->data );
}

void dump_window_update_all( )
{
    dump_window_t node = dump_list_head;
    while( node != NULL ) {
        dump_window_update(node);
        node = node->next;
    }
}

void dump_window_set_text( dump_window_t data, unsigned char *old_data, unsigned char *new_data )
{
    GtkTextBuffer *buf = data->textBuffer;
    GtkTextTag *changedTag = data->changedTag;
    GtkTextIter iter, endIter;
    int i, j, offset;
    /* Clear out the buffer */
    gtk_text_buffer_get_start_iter(buf,&iter);
    gtk_text_buffer_get_end_iter(buf,&endIter);
    gtk_text_buffer_delete(buf,&iter,&endIter);
    gtk_text_buffer_get_start_iter(buf,&iter);
    
    for( offset = 0, i=data->start; i<data->end; i+=16, offset+=16 ) {
        char text[80];
        sprintf(text, "%08X:", i );
        gtk_text_buffer_insert( buf, &iter, text, 9 );
        for( j=0; j<16; j++ ) {
            if( j%4 == 0 )
                gtk_text_buffer_insert( buf, &iter, " ", 1 );
            if( i+j < data->end ) {
                int oldVal = ((int)old_data[offset+j])&0xFF;
                int newVal = ((int)new_data[offset+j])&0xFF;
                sprintf(text, "%02X ", newVal);
                if( oldVal == newVal )
                    gtk_text_buffer_insert( buf, &iter, text, 3 );
                else
                    gtk_text_buffer_insert_with_tags( buf, &iter, text, 3,
                                                      changedTag, NULL );
            } else {
                gtk_text_buffer_insert( buf, &iter, "   ", 3 );
            }
        }
        gtk_text_buffer_insert( buf, &iter, "  ", 2 );
        for( j=0; j<16 && i+j < data->end; j++ ) {
            int oldVal = ((int)old_data[offset+j])&0xFF;
            int newVal = ((int)new_data[offset+j])&0xFF;
            if( isprint(newVal) )
                sprintf( text, "%c", newVal );
            else strcpy( text, "." );
            if( oldVal == newVal )
                gtk_text_buffer_insert( buf, &iter, text, 1 );
            else
                gtk_text_buffer_insert_with_tags( buf, &iter, text, 1,
                                                  changedTag, NULL );
        }
        gtk_text_buffer_insert( buf, &iter, "\n", 1 );
    }
}
