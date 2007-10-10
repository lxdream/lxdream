/**
 * $Id: dump_win.c,v 1.4 2007-10-10 11:02:04 nkeynes Exp $
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
#include "gui/debugif.h"

#define MAX_DUMP_SIZE 4096

#define DUMP_DATA_TAG 0xD4B9DA7A

typedef struct dump_data {
    uint32_t _tag;
    uint32_t start;
    uint32_t end;
    int flags;
    unsigned char *data;

    GtkEntry *fromInput, *toInput;
    GtkTextView *textArea;
    GtkTextTag *changedTag;
    GtkTextBuffer *textBuffer;
    struct dump_data *next;
} *dump_data_t;

static dump_data_t dump_list_head = NULL;

gboolean on_dump_win_delete_event( GtkWidget *widget, GdkEvent *event,
                                   gpointer user_data );
void on_dump_win_button_view_clicked( GtkWidget *widget, gpointer user_data );
void dump_win_set_text( dump_data_t data, unsigned char *old_data, unsigned char *new_data );


void dump_window_new( void ) {
    GtkWidget *win = create_dump_win();
    GtkWidget *dump_view_button = (GtkWidget *)g_object_get_data(G_OBJECT(win), "dump_view_button");
    dump_data_t data = malloc( sizeof(struct dump_data) );
    data->_tag = DUMP_DATA_TAG;
    data->fromInput = (GtkEntry *)g_object_get_data(G_OBJECT(win), "dump_from");
    data->toInput = (GtkEntry *)g_object_get_data(G_OBJECT(win), "dump_to");
    data->textArea = (GtkTextView *)g_object_get_data(G_OBJECT(win), "dump_text");
    data->next = dump_list_head;
    dump_list_head = data;
    data->data = NULL;
    data->start = 0;
    data->end = 0;
    gtk_entry_set_text( data->fromInput, "" );
    gtk_entry_set_text( data->toInput, "" );
    data->textBuffer = gtk_text_buffer_new(NULL);
    data->changedTag = gtk_text_buffer_create_tag(data->textBuffer, "changed",
                                                 "foreground", "blue",
                                                  NULL);
    gtk_text_view_set_buffer(data->textArea, data->textBuffer);
    gtk_text_view_set_editable(data->textArea, FALSE);
    gtk_widget_modify_font(GTK_WIDGET(data->textArea),gui_fixed_font);
        
    g_signal_connect ((gpointer) win, "delete_event",
                      G_CALLBACK (on_dump_win_delete_event),
                      data);
    g_signal_connect ((gpointer) dump_view_button, "clicked",
                      G_CALLBACK (on_dump_win_button_view_clicked),
                      data);
    gtk_widget_show( GTK_WIDGET(win) );
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

void gtk_entry_set_hex_value( GtkEntry *entry, uint32_t value )
{
    char buf[10];
    sprintf( buf, "%08X", value );
    gtk_entry_set_text( entry, buf );
}

        
gboolean on_dump_win_delete_event( GtkWidget *widget, GdkEvent *event,
                                   gpointer user_data )
{
    dump_data_t data = (dump_data_t)user_data;
    if( data->data != NULL )
        free( data->data );
    dump_data_t node = dump_list_head;
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

void on_dump_win_button_view_clicked( GtkWidget *widget, gpointer user_data )
{
    dump_data_t data = (dump_data_t)user_data;
    uint32_t startVal, endVal;

    assert( data != NULL );
    assert( data->_tag == DUMP_DATA_TAG );
    
    startVal = gtk_entry_get_hex_value(data->fromInput, data->start);
    endVal = gtk_entry_get_hex_value(data->toInput, data->end);
    if( startVal != data->start || endVal != data->end ) {
        if( startVal > endVal ) {
            int tmp = endVal;
            endVal = startVal;
            startVal = tmp;
        }
        if( endVal > startVal + MAX_DUMP_SIZE )
            endVal = startVal + MAX_DUMP_SIZE;

        gtk_entry_set_hex_value(data->fromInput,startVal);
        gtk_entry_set_hex_value(data->toInput,endVal);
        data->start = startVal;
        data->end = endVal;

        if( data->data != NULL ) {
            free( data->data );
            data->data = NULL;
        }
        if( startVal != endVal ) {
            data->data = malloc( endVal - startVal );
            mem_copy_from_sh4( data->data, startVal, endVal-startVal );
            dump_win_set_text( data, data->data, data->data );
        }
    }
}

void dump_win_update( dump_data_t data )
{
    if( data->data == NULL )
        return;
    unsigned char tmp[data->end-data->start];
    int length = data->end-data->start;
    memcpy( tmp, data->data, length );
    mem_copy_from_sh4( data->data, data->start, length );
    dump_win_set_text( data, tmp, data->data );
}

void dump_win_update_all( )
{
    dump_data_t node = dump_list_head;
    while( node != NULL ) {
        dump_win_update(node);
        node = node->next;
    }
}

void dump_win_set_text( dump_data_t data, unsigned char *old_data, unsigned char *new_data )
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
