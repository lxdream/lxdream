/**
 * $Id: debug_win.c,v 1.29 2007-11-10 04:45:29 nkeynes Exp $
 * This file is responsible for the main debugger gui frame.
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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include "mem.h"
#include "cpu.h"
#include "gtkui/gtkui.h"
#include "sh4/sh4dasm.h"
#include "aica/armdasm.h"

GdkColor *msg_colors[] = { &gui_colour_error, &gui_colour_error, &gui_colour_warn, 
			   &gui_colour_normal,&gui_colour_debug, &gui_colour_trace };

const cpu_desc_t cpu_list[4] = { &sh4_cpu_desc, &arm_cpu_desc, &armt_cpu_desc, NULL };

void init_register_list( debug_window_t data );
uint32_t row_to_address( debug_window_t data, int row );
int address_to_row( debug_window_t data, uint32_t address );
void set_disassembly_pc( debug_window_t data, unsigned int pc, gboolean select );
void set_disassembly_region( debug_window_t data, unsigned int page );
void set_disassembly_cpu( debug_window_t data, const gchar *cpu );

void on_mode_field_changed ( GtkEditable *editable, gpointer user_data);
gboolean on_page_field_key_press_event( GtkWidget * widget, GdkEventKey *event,
                                        gpointer user_data);
void on_jump_pc_btn_clicked( GtkButton *button, gpointer user_data);
void on_disasm_list_select_row (GtkCList *clist, gint row, gint column,
				GdkEvent *event, gpointer user_data);
void on_disasm_list_unselect_row (GtkCList *clist, gint row, gint column,
				  GdkEvent *event, gpointer user_data);
gboolean on_debug_delete_event(GtkWidget *widget, GdkEvent event, gpointer user_data);

struct debug_window_info {
    int disasm_from;
    int disasm_to;
    int disasm_pc;
    const struct cpu_desc_struct *cpu;
    const cpu_desc_t *cpu_list;
    GtkCList *regs_list;
    GtkCList *disasm_list;
    GtkEntry *page_field;
    GtkWidget *window;
    GtkWidget *statusbar;
    char saved_regs[0];
};

debug_window_t debug_window_new( const gchar *title, GtkWidget *menubar, 
				 GtkWidget *toolbar, GtkAccelGroup *accel_group )
{
    debug_window_t data = g_malloc0( sizeof(struct debug_window_info) + cpu_list[0]->regs_size );
        GtkWidget *vbox;

    data->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (data->window), 640, 480);
    gtk_window_set_title( GTK_WINDOW(data->window), title );
    gtk_window_add_accel_group (GTK_WINDOW (data->window), accel_group);

    gtk_toolbar_set_style( GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS );

    data->statusbar = gtk_statusbar_new();

    GtkWidget *hpaned = gtk_hpaned_new ();
    gtk_paned_set_position (GTK_PANED (hpaned), 800);

    GtkWidget *disasm_box = gtk_vbox_new(FALSE,0);
    gtk_paned_pack1 (GTK_PANED (hpaned), disasm_box, TRUE, TRUE);

    GtkWidget *hbox1 = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (disasm_box), hbox1, FALSE, FALSE, 3);
    gtk_box_pack_start (GTK_BOX (hbox1), gtk_label_new (_("Page")), FALSE, FALSE, 4);
    
    data->page_field = GTK_ENTRY(gtk_entry_new ());
    gtk_box_pack_start (GTK_BOX (hbox1), GTK_WIDGET(data->page_field), FALSE, TRUE, 0);
    
    GtkWidget *jump_pc_btn = gtk_button_new_with_mnemonic (_(" Jump to PC "));
    gtk_box_pack_start (GTK_BOX (hbox1), jump_pc_btn, FALSE, FALSE, 4);
    
    gtk_box_pack_start (GTK_BOX (hbox1), gtk_label_new(_("Mode")), FALSE, FALSE, 5);
    
    GtkWidget *mode_box = gtk_combo_new ();
    gtk_box_pack_start (GTK_BOX (hbox1), mode_box, FALSE, FALSE, 0);
    GList *mode_box_items = NULL;
    mode_box_items = g_list_append (mode_box_items, (gpointer) _("SH4"));
    mode_box_items = g_list_append (mode_box_items, (gpointer) _("ARM7"));
    mode_box_items = g_list_append (mode_box_items, (gpointer) _("ARM7T"));
    gtk_combo_set_popdown_strings (GTK_COMBO (mode_box), mode_box_items);
    g_list_free (mode_box_items);

    GtkWidget *mode_field = GTK_COMBO (mode_box)->entry;
    gtk_editable_set_editable (GTK_EDITABLE (mode_field), FALSE);

    GtkWidget *disasm_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_box_pack_start (GTK_BOX (disasm_box), disasm_scroll, TRUE, TRUE, 0);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (disasm_scroll), GTK_SHADOW_IN);
    data->disasm_list = GTK_CLIST(gtk_clist_new (4));
    gtk_clist_set_column_width (GTK_CLIST (data->disasm_list), 0, 80);
    gtk_clist_set_column_width (GTK_CLIST (data->disasm_list), 2, 80);
    gtk_clist_set_column_width (GTK_CLIST (data->disasm_list), 3, 80);
    gtk_clist_set_column_width( data->disasm_list, 1, 16 );
    gtk_clist_column_titles_hide (GTK_CLIST (data->disasm_list));
    gtk_container_add (GTK_CONTAINER (disasm_scroll), GTK_WIDGET(data->disasm_list));
    
    GtkWidget *reg_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_paned_pack2 (GTK_PANED (hpaned), reg_scroll, FALSE, TRUE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (reg_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (reg_scroll), GTK_SHADOW_IN);
    
    data->regs_list = GTK_CLIST(gtk_clist_new (2));
    gtk_container_add (GTK_CONTAINER (reg_scroll), GTK_WIDGET(data->regs_list));
    gtk_clist_set_column_width (GTK_CLIST (data->regs_list), 0, 80);
    gtk_clist_set_column_width (GTK_CLIST (data->regs_list), 1, 80);
    gtk_clist_column_titles_hide (GTK_CLIST (data->regs_list));
    gtk_widget_modify_font( GTK_WIDGET(data->regs_list), gui_fixed_font );
    
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add( GTK_CONTAINER(data->window), vbox );
    gtk_box_pack_start( GTK_BOX(vbox), menubar, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), toolbar, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), hpaned, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), data->statusbar, FALSE, FALSE, 0 );

    g_signal_connect ((gpointer) data->page_field, "key_press_event",
		      G_CALLBACK (on_page_field_key_press_event),
		      data);
    g_signal_connect ((gpointer) jump_pc_btn, "clicked",
		      G_CALLBACK (on_jump_pc_btn_clicked),
		      data);
    g_signal_connect ((gpointer) mode_field, "changed",
		      G_CALLBACK (on_mode_field_changed),
		      data);
    g_signal_connect ((gpointer) data->disasm_list, "select_row",
		      G_CALLBACK (on_disasm_list_select_row),
		      data);
    g_signal_connect ((gpointer) data->disasm_list, "unselect_row",
		      G_CALLBACK (on_disasm_list_unselect_row),
		      data);
    g_signal_connect ((gpointer) data->window, "delete_event",
		      G_CALLBACK (on_debug_delete_event),
		      data);
    
    data->disasm_from = -1;
    data->disasm_to = -1;
    data->disasm_pc = -1;
    data->cpu = cpu_list[0];
    data->cpu_list = cpu_list;
    
    init_register_list( data );
    gtk_object_set_data( GTK_OBJECT(data->window), "debug_data", data );
    set_disassembly_pc( data, *data->cpu->pc, FALSE );
    debug_window_set_running( data, FALSE );

    gtk_widget_show_all( data->window );
    return data;
}

void debug_window_show( debug_window_t data, gboolean show )
{
    if( show ) {
	gtk_widget_show( data->window );
    } else {
	gtk_widget_hide( data->window );
    }
}

int debug_window_get_selected_row( debug_window_t data )
{
    if( data->disasm_list->selection == NULL ) {
	return -1;
    } else {
	return GPOINTER_TO_INT(data->disasm_list->selection->data);
    }
}

void init_register_list( debug_window_t data ) 
{
    int i;
    char buf[20];
    char *arr[2];

    gtk_clist_clear( data->regs_list );
    arr[1] = buf;
    for( i=0; data->cpu->regs_info[i].name != NULL; i++ ) {
        arr[0] = data->cpu->regs_info[i].name;
        if( data->cpu->regs_info->type == REG_INT )
            sprintf( buf, "%08X", *((uint32_t *)data->cpu->regs_info[i].value) );
        else
            sprintf( buf, "%f", *((float *)data->cpu->regs_info[i].value) );
        gtk_clist_append( data->regs_list, arr );
    }
}

/*
 * Check for changed registers and update the display
 */
void debug_window_update( debug_window_t data )
{
    int i;
    for( i=0; data->cpu->regs_info[i].name != NULL; i++ ) {
        if( data->cpu->regs_info[i].type == REG_INT ) {
            /* Yes this _is_ probably fairly evil */
            if( *((uint32_t *)data->cpu->regs_info[i].value) !=
                *((uint32_t *)((char *)data->saved_regs + ((char *)data->cpu->regs_info[i].value - (char *)data->cpu->regs))) ) {
                char buf[20];
                sprintf( buf, "%08X", *((uint32_t *)data->cpu->regs_info[i].value) );
                gtk_clist_set_text( data->regs_list, i, 1, buf );
                gtk_clist_set_foreground( data->regs_list, i, &gui_colour_changed );
            } else {
                gtk_clist_set_foreground( data->regs_list, i, &gui_colour_normal );
            }
        } else {
            if( *((float *)data->cpu->regs_info[i].value) !=
                *((float *)((char *)data->saved_regs + ((char *)data->cpu->regs_info[i].value - (char *)data->cpu->regs))) ) {
                char buf[20];
                sprintf( buf, "%f", *((float *)data->cpu->regs_info[i].value) );
                gtk_clist_set_text( data->regs_list, i, 1, buf );
                gtk_clist_set_foreground( data->regs_list, i, &gui_colour_changed );
            } else {
                gtk_clist_set_foreground( data->regs_list, i, &gui_colour_normal );
            }
        }
    }

    set_disassembly_pc( data, *data->cpu->pc, TRUE );
    memcpy( data->saved_regs, data->cpu->regs, data->cpu->regs_size );
}

void set_disassembly_region( debug_window_t data, unsigned int page )
{
    uint32_t i, posn, next;
    char buf[80];
    char addr[10];
    char opcode[16] = "";
    char *arr[4] = { addr, " ", opcode, buf };
    unsigned int from = page & 0xFFFFF000;
    unsigned int to = from + 4096;
    
    gtk_clist_clear(data->disasm_list);

    sprintf( addr, "%08X", from );
    gtk_entry_set_text( data->page_field, addr );

    if( !data->cpu->is_valid_page_func( from ) ) {
        arr[3] = _("This page is currently unmapped");
        gtk_clist_append( data->disasm_list, arr );
        gtk_clist_set_foreground( data->disasm_list, 0, &gui_colour_error );
    } else {
        for( i=from; i<to; i = next ) {
	    next = data->cpu->disasm_func( i, buf, sizeof(buf), opcode );
            sprintf( addr, "%08X", i );
            posn = gtk_clist_append( data->disasm_list, arr );
            if( buf[0] == '?' )
                gtk_clist_set_foreground( data->disasm_list, posn, &gui_colour_warn );
	    if( data->cpu->get_breakpoint != NULL ) {
		int type = data->cpu->get_breakpoint( i );
		switch(type) {
		case BREAK_ONESHOT:
		    gtk_clist_set_background( data->disasm_list, posn, &gui_colour_temp_break );
		    break;
		case BREAK_KEEP:
		    gtk_clist_set_background( data->disasm_list, posn, &gui_colour_break );
		    break;
		}
	    }
        }
        if( data->disasm_pc != -1 && data->disasm_pc >= from && data->disasm_pc < to )
            gtk_clist_set_foreground( data->disasm_list, address_to_row(data, data->disasm_pc),
                                      &gui_colour_pc );
    }

    if( page != from ) { /* not a page boundary */
        gtk_clist_moveto( data->disasm_list, (page-from)>>1, 0, 0.5, 0.0 );
    }
    data->disasm_from = from;
    data->disasm_to = to;
}

void jump_to_disassembly( debug_window_t data, unsigned int addr, gboolean select )
{
    int row;
    
    if( addr < data->disasm_from || addr >= data->disasm_to )
        set_disassembly_region(data,addr);

    row = address_to_row( data, addr );
    if(select) {
        gtk_clist_select_row( data->disasm_list, row, 0 );
    }
    if( gtk_clist_row_is_visible( data->disasm_list, row ) != GTK_VISIBILITY_FULL ){
        gtk_clist_moveto( data->disasm_list, row, 0, 0.5, 0.0 );
    }
}

void jump_to_pc( debug_window_t data, gboolean select )
{
    jump_to_disassembly( data, *data->cpu->pc, select );
}

void set_disassembly_pc( debug_window_t data, unsigned int pc, gboolean select )
{
    int row;
    
    jump_to_disassembly( data, pc, select );
    if( data->disasm_pc != -1 && data->disasm_pc >= data->disasm_from && 
	data->disasm_pc < data->disasm_to )
        gtk_clist_set_foreground( data->disasm_list, 
				  (data->disasm_pc - data->disasm_from) / data->cpu->instr_size,
                                  &gui_colour_normal );
    row = address_to_row( data, pc );
    gtk_clist_set_foreground( data->disasm_list, row, &gui_colour_pc );
    data->disasm_pc = pc;
}

void set_disassembly_cpu( debug_window_t data, const gchar *cpu )
{
    int i;
    for( i=0; data->cpu_list[i] != NULL; i++ ) {
	if( strcmp( data->cpu_list[i]->name, cpu ) == 0 ) {
	    if( data->cpu != data->cpu_list[i] ) {
		data->cpu = data->cpu_list[i];
		data->disasm_from = data->disasm_to = -1; /* Force reload */
		set_disassembly_pc( data, *data->cpu->pc, FALSE );
		init_register_list( data );
	    }
	    return;
	}
    }
}

void debug_window_toggle_breakpoint( debug_window_t data, int row )
{
    uint32_t pc = row_to_address( data, row );
    int oldType = data->cpu->get_breakpoint( pc );
    if( oldType != BREAK_NONE ) {
	data->cpu->clear_breakpoint( pc, oldType );
	gtk_clist_set_background( data->disasm_list, row, &gui_colour_white );
    } else {
	data->cpu->set_breakpoint( pc, BREAK_KEEP );
	gtk_clist_set_background( data->disasm_list, row, &gui_colour_break );
    }
}

void debug_window_set_oneshot_breakpoint( debug_window_t data, int row )
{
    uint32_t pc = row_to_address( data, row );
    data->cpu->clear_breakpoint( pc, BREAK_ONESHOT );
    data->cpu->set_breakpoint( pc, BREAK_ONESHOT );
    gtk_clist_set_background( data->disasm_list, row, &gui_colour_temp_break );
}

/**
 * Execute a single instruction using the current CPU mode.
 */
void debug_window_single_step( debug_window_t data )
{
    data->cpu->step_func();
    gtk_gui_update();
}

uint32_t row_to_address( debug_window_t data, int row ) {
    return data->cpu->instr_size * row + data->disasm_from;
}

int address_to_row( debug_window_t data, uint32_t address ) {
    if( data->disasm_from > address || data->disasm_to <= address )
	return -1;
    return (address - data->disasm_from) / data->cpu->instr_size;
}

debug_window_t get_debug_info( GtkWidget *widget ) {
    
    GtkWidget *win = gtk_widget_get_toplevel(widget);
    debug_window_t data = (debug_window_t)gtk_object_get_data( GTK_OBJECT(win), "debug_data" );
    return data;
}

void debug_window_set_running( debug_window_t data, gboolean isRunning ) 
{
    if( data != NULL ) {
	gtk_gui_enable_action( "SingleStep", !isRunning );
	gtk_gui_enable_action( "RunTo", !isRunning && dreamcast_can_run() );
    }
}

void on_mode_field_changed ( GtkEditable *editable, gpointer user_data)
{
    const gchar *text = gtk_entry_get_text( GTK_ENTRY(editable) );
    set_disassembly_cpu( gtk_gui_get_debugger(), text );
}


gboolean on_page_field_key_press_event( GtkWidget * widget, GdkEventKey *event,
                                        gpointer user_data)
{
    if( event->keyval == GDK_Return || event->keyval == GDK_Linefeed ) {
	debug_window_t data = get_debug_info(widget);
        const gchar *text = gtk_entry_get_text( GTK_ENTRY(widget) );
        gchar *endptr;
        unsigned int val = strtoul( text, &endptr, 16 );
        if( text == endptr ) { /* invalid input */
            char buf[10];
            sprintf( buf, "%08X", row_to_address(data,0) );
            gtk_entry_set_text( GTK_ENTRY(widget), buf );
        } else {
            set_disassembly_region(data, val);
        }
    }
    return FALSE;
}


void on_jump_pc_btn_clicked( GtkButton *button, gpointer user_data)
{
    debug_window_t data = get_debug_info( GTK_WIDGET(button) );
    jump_to_pc( data, TRUE );
}

void on_disasm_list_select_row (GtkCList *clist, gint row, gint column,
				GdkEvent *event, gpointer user_data)
{
    gtk_gui_enable_action( "SetBreakpoint", TRUE );
    gtk_gui_enable_action( "RunTo", dreamcast_can_run() );
}

void on_disasm_list_unselect_row (GtkCList *clist, gint row, gint column,
				  GdkEvent *event, gpointer user_data)
{
    gtk_gui_enable_action( "SetBreakpoint", FALSE );
    gtk_gui_enable_action( "RunTo", FALSE );
}

gboolean on_debug_delete_event(GtkWidget *widget, GdkEvent event, gpointer user_data)
{
    gtk_widget_hide( widget );
    return TRUE;
}
