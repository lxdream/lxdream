/**
 * $Id: debug_win.c,v 1.21 2007-10-03 09:32:09 nkeynes Exp $
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
#include <gnome.h>
#include <math.h>
#include "sh4/sh4dasm.h"
#include "gui/gui.h"
#include "mem.h"
#include "cpu.h"
#include "display.h"

GdkColor *msg_colors[] = { &clrError, &clrError, &clrWarn, &clrNormal,
                           &clrDebug, &clrTrace };
char *msg_levels[] = { "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE" };
int global_msg_level = EMIT_WARN;

void init_register_list( debug_info_t data );

struct debug_info_struct {
    int disasm_from;
    int disasm_to;
    int disasm_pc;
    struct cpu_desc_struct *cpu;
    const cpu_desc_t *cpu_list;
    GtkCList *msgs_list;
    GtkCList *regs_list;
    GtkCList *disasm_list;
    GtkEntry *page_field;
    GtkWidget *win;
    GtkProgressBar *icounter;
    char icounter_text[16];
    char saved_regs[0];
};

debug_info_t init_debug_win(GtkWidget *win, const cpu_desc_t *cpu_list )
{
    GnomeAppBar *appbar;
    
    debug_info_t data = g_malloc0( sizeof(struct debug_info_struct) + cpu_list[0]->regs_size );
    data->disasm_from = -1;
    data->disasm_to = -1;
    data->disasm_pc = -1;
    data->cpu = cpu_list[0];
    data->cpu_list = cpu_list;
    
    data->regs_list= gtk_object_get_data(GTK_OBJECT(win), "reg_list");
    data->win = win;
    gtk_widget_modify_font( GTK_WIDGET(data->regs_list), fixed_list_font );
    init_register_list( data );
    data->msgs_list = gtk_object_get_data(GTK_OBJECT(win), "output_list");
    data->disasm_list = gtk_object_get_data(GTK_OBJECT(win), "disasm_list");
    gtk_clist_set_column_width( data->disasm_list, 1, 16 );
    data->page_field = gtk_object_get_data(GTK_OBJECT(win), "page_field");

    appbar = gtk_object_get_data(GTK_OBJECT(win), "debug_appbar");
    data->icounter = gnome_appbar_get_progress( appbar );
    gtk_progress_bar_set_text(data->icounter, "1");
    
    gtk_object_set_data( GTK_OBJECT(win), "debug_data", data );
    set_disassembly_pc( data, *data->cpu->pc, FALSE );
    debug_win_set_running( data, FALSE );
    return data;
}

void init_register_list( debug_info_t data ) 
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
void update_registers( debug_info_t data )
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
                gtk_clist_set_foreground( data->regs_list, i, &clrChanged );
            } else {
                gtk_clist_set_foreground( data->regs_list, i, &clrNormal );
            }
        } else {
            if( *((float *)data->cpu->regs_info[i].value) !=
                *((float *)((char *)data->saved_regs + ((char *)data->cpu->regs_info[i].value - (char *)data->cpu->regs))) ) {
                char buf[20];
                sprintf( buf, "%f", *((float *)data->cpu->regs_info[i].value) );
                gtk_clist_set_text( data->regs_list, i, 1, buf );
                gtk_clist_set_foreground( data->regs_list, i, &clrChanged );
            } else {
                gtk_clist_set_foreground( data->regs_list, i, &clrNormal );
            }
        }
    }

    set_disassembly_pc( data, *data->cpu->pc, FALSE );
    memcpy( data->saved_regs, data->cpu->regs, data->cpu->regs_size );
}

void update_icount( debug_info_t data )
{
    if( data != NULL ) {
	//    sprintf( data->icounter_text, "%d", *data->cpu->icount );
	sprintf( data->icounter_text, "%d", pvr2_get_frame_count() );
	gtk_progress_bar_set_text( data->icounter, data->icounter_text );
    }
}

void set_disassembly_region( debug_info_t data, unsigned int page )
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
        arr[3] = "This page is currently unmapped";
        gtk_clist_append( data->disasm_list, arr );
        gtk_clist_set_foreground( data->disasm_list, 0, &clrError );
    } else {
        for( i=from; i<to; i = next ) {
	    next = data->cpu->disasm_func( i, buf, sizeof(buf), opcode );
            sprintf( addr, "%08X", i );
            posn = gtk_clist_append( data->disasm_list, arr );
            if( buf[0] == '?' )
                gtk_clist_set_foreground( data->disasm_list, posn, &clrWarn );
	    if( data->cpu->get_breakpoint != NULL ) {
		int type = data->cpu->get_breakpoint( i );
		switch(type) {
		case BREAK_ONESHOT:
		    gtk_clist_set_background( data->disasm_list, posn, &clrTempBreak );
		    break;
		case BREAK_KEEP:
		    gtk_clist_set_background( data->disasm_list, posn, &clrBreak );
		    break;
		}
	    }
        }
        if( data->disasm_pc != -1 && data->disasm_pc >= from && data->disasm_pc < to )
            gtk_clist_set_foreground( data->disasm_list, address_to_row(data, data->disasm_pc),
                                      &clrPC );
    }

    if( page != from ) { /* not a page boundary */
        gtk_clist_moveto( data->disasm_list, (page-from)>>1, 0, 0.5, 0.0 );
    }
    data->disasm_from = from;
    data->disasm_to = to;
}

void jump_to_disassembly( debug_info_t data, unsigned int addr, gboolean select )
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

void jump_to_pc( debug_info_t data, gboolean select )
{
    jump_to_disassembly( data, *data->cpu->pc, select );
}

void set_disassembly_pc( debug_info_t data, unsigned int pc, gboolean select )
{
    int row;
    
    jump_to_disassembly( data, pc, select );
    if( data->disasm_pc != -1 && data->disasm_pc >= data->disasm_from && 
	data->disasm_pc < data->disasm_to )
        gtk_clist_set_foreground( data->disasm_list, 
				  (data->disasm_pc - data->disasm_from) / data->cpu->instr_size,
                                  &clrNormal );
    row = address_to_row( data, pc );
    gtk_clist_set_foreground( data->disasm_list, row, &clrPC );
    data->disasm_pc = pc;
}

void set_disassembly_cpu( debug_info_t data, const gchar *cpu )
{
    int i;
    for( i=0; data->cpu_list[i] != NULL; i++ ) {
	if( strcmp( data->cpu_list[i]->name, cpu ) == 0 ) {
	    if( data->cpu != data->cpu_list[i] ) {
		data->cpu = data->cpu_list[i];
		data->disasm_from = data->disasm_to = -1; /* Force reload */
		set_disassembly_pc( data, *data->cpu->pc, FALSE );
		init_register_list( data );
		update_icount( data );
	    }
	    return;
	}
    }
}

void debug_win_toggle_breakpoint( debug_info_t data, int row )
{
    uint32_t pc = row_to_address( data, row );
    int oldType = data->cpu->get_breakpoint( pc );
    if( oldType != BREAK_NONE ) {
	data->cpu->clear_breakpoint( pc, oldType );
	gtk_clist_set_background( data->disasm_list, row, &clrWhite );
    } else {
	data->cpu->set_breakpoint( pc, BREAK_KEEP );
	gtk_clist_set_background( data->disasm_list, row, &clrBreak );
    }
}

void debug_win_set_oneshot_breakpoint( debug_info_t data, int row )
{
    uint32_t pc = row_to_address( data, row );
    data->cpu->clear_breakpoint( pc, BREAK_ONESHOT );
    data->cpu->set_breakpoint( pc, BREAK_ONESHOT );
    gtk_clist_set_background( data->disasm_list, row, &clrTempBreak );
}

/**
 * Execute a single instruction using the current CPU mode.
 */
void debug_win_single_step( debug_info_t data )
{
    data->cpu->step_func();
    gtk_gui_update();
}

uint32_t row_to_address( debug_info_t data, int row ) {
    return data->cpu->instr_size * row + data->disasm_from;
}

int address_to_row( debug_info_t data, uint32_t address ) {
    if( data->disasm_from > address || data->disasm_to <= address )
	return -1;
    return (address - data->disasm_from) / data->cpu->instr_size;
}


void emit( void *ptr, int level, const gchar *source, const char *msg, ... )
{
    char buf[20], addr[10] = "", *p;
    const char *arr[4] = {buf, source, addr};
    int posn;
    time_t tm = time(NULL);
    va_list ap;
    debug_info_t data;
    if( ptr == NULL )
	data = main_debug;
    else data = (debug_info_t)ptr;

    if( level > global_msg_level ) {
	return; // ignored
    }
    va_start(ap, msg);

    strftime( buf, sizeof(buf), "%H:%M:%S", localtime(&tm) );

    if( data == NULL ) {
	fprintf( stderr, "%s %08X %-5s ", buf, *sh4_cpu_desc.pc, msg_levels[level] );
	vfprintf( stderr, msg, ap );
	fprintf( stderr, "\n" );
	va_end(ap);
	return;
    }

    p = g_strdup_vprintf( msg, ap );
    sprintf( addr, "%08X", *data->cpu->pc );
    arr[3] = p;
    posn = gtk_clist_append(data->msgs_list, arr);
    free(p);
    va_end(ap);

    gtk_clist_set_foreground( data->msgs_list, posn, msg_colors[level] );
    gtk_clist_moveto( data->msgs_list, posn, 0, 1.0, 0.0 );

    /* emit _really_ slows down the emu, to the point where the gui can be
     * completely unresponsive if I don't include this:
     */
    while( gtk_events_pending() )
        gtk_main_iteration();
}

debug_info_t get_debug_info( GtkWidget *widget ) {
    
    GtkWidget *win = gtk_widget_get_toplevel(widget);
    debug_info_t data = (debug_info_t)gtk_object_get_data( GTK_OBJECT(win), "debug_data" );
    return data;
}

void debug_win_enable_widget( debug_info_t data, const char *name, 
			      gboolean enabled )
{
    GtkWidget *widget = GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(data->win), name));
    gtk_widget_set_sensitive( widget, enabled );
}    

void debug_win_set_running( debug_info_t data, gboolean isRunning ) 
{
    if( data != NULL ) {
	debug_win_enable_widget( data, "stop_btn", isRunning );
	debug_win_enable_widget( data, "step_btn", !isRunning );
	debug_win_enable_widget( data, "run_btn", !isRunning );
	debug_win_enable_widget( data, "runto_btn", !isRunning );
    }
}
