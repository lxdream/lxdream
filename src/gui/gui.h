/*
 * Gui related code
 */
#ifndef dream_gui_H
#define dream_gui_H 1

#include <gnome.h>
#include "dream.h"

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

void init_gui(void);
void update_gui(void);

void init_debug_win(GtkWidget *);
void open_file_dialog( void );
void update_mmr_win( void );
void init_mmr_win( void );
void update_registers( void );
void update_icount( void );
void dump_win_update_all( void );
void set_disassembly_region( unsigned int page );
void set_disassembly_pc( unsigned int pc, gboolean select );
void jump_to_disassembly( unsigned int addr, gboolean select );

extern PangoFontDescription *fixed_list_font;
extern GdkColor clrNormal, clrChanged, clrError, clrWarn,
    clrPC, clrDebug, clrTrace;

void mmr_open_win( void );
void mmr_close_win( void );
uint32_t gtk_entry_get_hex_value( GtkEntry *entry, uint32_t defaultValue );
void gtk_entry_set_hex_value( GtkEntry *entry, uint32_t value );

#ifdef __cplusplus
}
#endif
#endif
