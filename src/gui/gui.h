/**
 * $Id: gui.h,v 1.16 2007-09-18 10:48:57 nkeynes Exp $
 * 
 * General GUI definitions
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

#ifndef dream_gui_H
#define dream_gui_H 1

#include <gnome.h>
#include "dream.h"
#include "cpu.h"
#include "gui/interface.h"

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

void gtk_gui_init(void);
void gtk_gui_show_debugger(void);
void gtk_gui_update(void);
extern struct dreamcast_module gtk_gui_module;

typedef struct debug_info_struct *debug_info_t;
extern debug_info_t main_debug;

typedef int (*file_callback_t)( const gchar *filename );
void open_file_dialog( char *title, file_callback_t file_handler, char *pattern, char *patname, const gchar *initial_dir );
void save_file_dialog( char *title, file_callback_t file_handler, char *pattern, char *patname, const gchar *initial_dir );

void update_mmr_win( void );
void init_mmr_win( void );

debug_info_t init_debug_win(GtkWidget *, const cpu_desc_t *cpu );
debug_info_t get_debug_info(GtkWidget *widget);
void update_registers( debug_info_t debug );
void update_icount( debug_info_t debug );
void dump_win_update_all();
void set_disassembly_region( debug_info_t debug, unsigned int page );
void set_disassembly_pc( debug_info_t debug, unsigned int pc, gboolean select );
void set_disassembly_cpu( debug_info_t debug, const gchar *cpu_name );
void jump_to_disassembly( debug_info_t debug, unsigned int addr, gboolean select );
void jump_to_pc( debug_info_t debug, gboolean select );
void debug_win_set_running( debug_info_t debug, gboolean isRunning );
void debug_win_single_step( debug_info_t debug );
void debug_win_toggle_breakpoint( debug_info_t debug, int row );
void debug_win_set_oneshot_breakpoint( debug_info_t debug, int row );
uint32_t row_to_address( debug_info_t debug, int row );
int address_to_row( debug_info_t debug, uint32_t address );

extern PangoFontDescription *fixed_list_font;
extern GdkColor clrNormal, clrChanged, clrError, clrWarn,
    clrPC, clrDebug, clrTrace, clrBreak, clrTempBreak, clrWhite;

void mmr_open_win( void );
void mmr_close_win( void );
uint32_t gtk_entry_get_hex_value( GtkEntry *entry, uint32_t defaultValue );
void gtk_entry_set_hex_value( GtkEntry *entry, uint32_t value );

#ifdef __cplusplus
}
#endif
#endif
