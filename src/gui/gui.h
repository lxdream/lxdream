/*
 * Gui related code
 */
#ifndef dream_gui_H
#define dream_gui_H 1

#include <gnome.h>
#include "dream.h"
#include "disasm.h"

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

void init_gui(void);
void update_gui(void);

typedef struct debug_info_struct *debug_info_t;
extern debug_info_t main_debug;

typedef int (*file_callback_t)( const gchar *filename );
void open_file_dialog( char *title, file_callback_t file_handler, char *pattern, char *patname );
void save_file_dialog( char *title, file_callback_t file_handler, char *pattern, char *patname );

debug_info_t init_debug_win(GtkWidget *, cpu_desc_t *cpu );
debug_info_t get_debug_info(GtkWidget *widget);
void update_mmr_win( void );
void init_mmr_win( void );
void update_registers( debug_info_t debug );
void update_icount( debug_info_t debug );
void dump_win_update_all();
void set_disassembly_region( debug_info_t debug, unsigned int page );
void set_disassembly_pc( debug_info_t debug, unsigned int pc, gboolean select );
void set_disassembly_cpu( debug_info_t debug, char *cpu_name );
void jump_to_disassembly( debug_info_t debug, unsigned int addr, gboolean select );
void jump_to_pc( debug_info_t debug, gboolean select );
uint32_t row_to_address( debug_info_t debug, int row );
int address_to_row( debug_info_t debug, uint32_t address );

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
