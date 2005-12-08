
#include <stdlib.h>
#include <stdarg.h>
#include <gnome.h>
#include <math.h>
#include "gui.h"
#include "mem.h"
#include "disasm.h"

GdkColor *msg_colors[] = { &clrError, &clrError, &clrWarn, &clrNormal,
                           &clrDebug, &clrTrace };

struct debug_info_struct {
    int disasm_from;
    int disasm_to;
    int disasm_pc;
    struct cpu_desc_struct *cpu;
    GtkCList *msgs_list;
    GtkCList *regs_list;
    GtkCList *disasm_list;
    GtkEntry *page_field;
    GtkProgressBar *icounter;
    char icounter_text[16];
    char saved_regs[0];
};

debug_info_t init_debug_win(GtkWidget *win, struct cpu_desc_struct *cpu )
{
    int i;
    char buf[20];
    char *arr[2];
    GnomeAppBar *appbar;
    
    debug_info_t data = g_malloc0( sizeof(struct debug_info_struct) + cpu->regs_size );
    data->disasm_from = -1;
    data->disasm_to = -1;
    data->disasm_pc = -1;
    data->cpu = cpu;
    
    data->regs_list= gtk_object_get_data(GTK_OBJECT(win), "reg_list");
    arr[1] = buf;
    for( i=0; data->cpu->regs_info[i].name != NULL; i++ ) {
        arr[0] = data->cpu->regs_info[i].name;
        if( data->cpu->regs_info->type == REG_INT )
            sprintf( buf, "%08X", *((uint32_t *)data->cpu->regs_info->value) );
        else
            sprintf( buf, "%f", *((float *)data->cpu->regs_info->value) );
        gtk_clist_append( data->regs_list, arr );
    }
    gtk_widget_modify_font( GTK_WIDGET(data->regs_list), fixed_list_font );

    data->msgs_list = gtk_object_get_data(GTK_OBJECT(win), "output_list");
    data->disasm_list = gtk_object_get_data(GTK_OBJECT(win), "disasm_list");
    gtk_clist_set_column_width( data->disasm_list, 1, 16 );
    data->page_field = gtk_object_get_data(GTK_OBJECT(win), "page_field");

    appbar = gtk_object_get_data(GTK_OBJECT(win), "debug_appbar");
    data->icounter = gnome_appbar_get_progress( appbar );
    gtk_progress_bar_set_text(data->icounter, "1");

    gtk_object_set_data( GTK_OBJECT(win), "debug_data", data );
    return data;
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
    sprintf( data->icounter_text, "%d", *data->cpu->icount );
    gtk_progress_bar_set_text( data->icounter, data->icounter_text );
}

void set_disassembly_region( debug_info_t data, unsigned int page )
{
    uint32_t i, posn;
    uint16_t op;
    char buf[80];
    char addr[10];
    char opcode[6] = "";
    char *arr[4] = { addr, " ", opcode, buf };
    unsigned int from = page & 0xFFFFF000;
    unsigned int to = from + 4096;
    
    gtk_clist_clear(data->disasm_list);

    sprintf( addr, "%08X", from );
    gtk_entry_set_text( data->page_field, addr );

    if( !mem_has_page( from ) ) {
        arr[3] = "This page is currently unmapped";
        gtk_clist_append( data->disasm_list, arr );
        gtk_clist_set_foreground( data->disasm_list, 0, &clrError );
    } else {
        for( i=from; i<to; ) {
	    i = data->cpu->disasm_func( i, buf, sizeof(buf) );
            sprintf( addr, "%08X", i );
            op = mem_read_phys_word(i);
            sprintf( opcode, "%02X %02X", op&0xFF, op>>8 );
            posn = gtk_clist_append( data->disasm_list, arr );
            if( buf[0] == '?' )
                gtk_clist_set_foreground( data->disasm_list, posn, &clrWarn );
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

uint32_t row_to_address( debug_info_t data, int row ) {
    return data->cpu->instr_size * row + data->disasm_from;
}

int address_to_row( debug_info_t data, uint32_t address ) {
    if( data->disasm_from > address || data->disasm_to <= address )
	return -1;
    return (address - data->disasm_from) / data->cpu->instr_size;
}

void emit( void *ptr, int level, int source, char *msg, ... )
{
    char buf[20], addr[10] = "", *p;
    char *arr[3] = {buf, addr};
    int posn;
    time_t tm = time(NULL);
    va_list ap;
    debug_info_t data;
    if( ptr == NULL )
	data = main_debug;
    else data = (debug_info_t)ptr;

    va_start(ap, msg);
    p = g_strdup_vprintf( msg, ap );
    strftime( buf, sizeof(buf), "%H:%M:%S", localtime(&tm) );
    if( source != -1 )
        sprintf( addr, "%08X", *data->cpu->pc );
    arr[2] = p;
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

