
#include <stdlib.h>
#include <stdarg.h>
#include <gnome.h>
#include <math.h>
#include "gui.h"
#include "mem.h"
#include "sh4dasm.h"
#include "sh4core.h"

GdkColor *msg_colors[] = { &clrError, &clrError, &clrWarn, &clrNormal,
                           &clrDebug, &clrTrace };

#define REG_INT 0
#define REG_FLT 1
#define REG_SPECIAL 2

struct reg_map_struct {
    char *name;
    int type;
    void *value;
} reg_map[] = { {"R0", REG_INT, &sh4r.r[0]}, {"R1", REG_INT, &sh4r.r[1]},
                {"R2", REG_INT, &sh4r.r[2]}, {"R3", REG_INT, &sh4r.r[3]},
                {"R4", REG_INT, &sh4r.r[4]}, {"R5", REG_INT, &sh4r.r[5]},
                {"R6", REG_INT, &sh4r.r[6]}, {"R7", REG_INT, &sh4r.r[7]},
                {"R8", REG_INT, &sh4r.r[8]}, {"R9", REG_INT, &sh4r.r[9]},
                {"R10",REG_INT, &sh4r.r[10]}, {"R11",REG_INT, &sh4r.r[11]},
                {"R12",REG_INT, &sh4r.r[12]}, {"R13",REG_INT, &sh4r.r[13]},
                {"R14",REG_INT, &sh4r.r[14]}, {"R15",REG_INT, &sh4r.r[15]},
                {"SR", REG_INT, &sh4r.sr}, {"GBR", REG_INT, &sh4r.gbr},
                {"SSR",REG_INT, &sh4r.ssr}, {"SPC", REG_INT, &sh4r.spc},
                {"SGR",REG_INT, &sh4r.sgr}, {"DBR", REG_INT, &sh4r.dbr},
                {"VBR",REG_INT, &sh4r.vbr},
                {"PC", REG_INT, &sh4r.pc}, {"PR", REG_INT, &sh4r.pr},
                {"MACL",REG_INT, &sh4r.mac},{"MACH",REG_INT, ((uint32_t *)&sh4r.mac)+1},
                {"FPUL", REG_INT, &sh4r.fpul}, {"FPSCR", REG_INT, &sh4r.fpscr},
                {NULL, 0, NULL} };

GtkCList *msgs, *regs, *disasm;
GtkEntry *page_field;
GtkProgressBar *icounter;
char icounter_text[16];

struct sh4_registers sh4r_s;
int disasm_from = -1, disasm_to = -1;
int disasm_pc = -1;


void init_debug_win(GtkWidget *win)
{
    int i;
    char buf[20];
    char *arr[2];
    GnomeAppBar *appbar;

    regs = gtk_object_get_data(GTK_OBJECT(win), "reg_list");
    arr[1] = buf;
    for( i=0; reg_map[i].name != NULL; i++ ) {
        arr[0] = reg_map[i].name;
        if( reg_map[i].type == REG_INT )
            sprintf( buf, "%08X", *((uint32_t *)reg_map[i].value) );
        else
            sprintf( buf, "%f", *((float *)reg_map[i].value) );
        gtk_clist_append( regs, arr );
    }
    gtk_widget_modify_font( GTK_WIDGET(regs), fixed_list_font );

    msgs = gtk_object_get_data(GTK_OBJECT(win), "output_list");
    disasm = gtk_object_get_data(GTK_OBJECT(win), "disasm_list");
    gtk_clist_set_column_width( disasm, 1, 16 );
    page_field = gtk_object_get_data(GTK_OBJECT(win), "page_field");

    appbar = gtk_object_get_data(GTK_OBJECT(win), "debug_appbar");
    icounter = gnome_appbar_get_progress( appbar );
    gtk_progress_bar_set_text(icounter, "1");
}

/*
 * Check for changed registers and update the display
 */
void update_registers( void )
{
    int i;
    for( i=0; reg_map[i].name != NULL; i++ ) {
        if( reg_map[i].type == REG_INT ) {
            /* Yes this _is_ probably fairly evil */
            if( *((uint32_t *)reg_map[i].value) !=
                *((uint32_t *)((char *)&sh4r_s + ((char *)reg_map[i].value - (char *)&sh4r))) ) {
                char buf[20];
                sprintf( buf, "%08X", *((uint32_t *)reg_map[i].value) );
                gtk_clist_set_text( regs, i, 1, buf );
                gtk_clist_set_foreground( regs, i, &clrChanged );
            } else {
                gtk_clist_set_foreground( regs, i, &clrNormal );
            }
        } else {
            if( *((float *)reg_map[i].value) !=
                *((float *)((char *)&sh4r_s + ((char *)reg_map[i].value - (char *)&sh4r))) ) {
                char buf[20];
                sprintf( buf, "%f", *((float *)reg_map[i].value) );
                gtk_clist_set_text( regs, i, 1, buf );
                gtk_clist_set_foreground( regs, i, &clrChanged );
            } else {
                gtk_clist_set_foreground( regs, i, &clrNormal );
            }
        }
    }
    if( sh4r.pc != sh4r_s.pc )
        set_disassembly_pc( sh4r.pc, FALSE );
    memcpy( &sh4r_s, &sh4r, sizeof(sh4r) );
}

void update_icount( void )
{
    sprintf( icounter_text, "%d", sh4r.icount );
    gtk_progress_bar_set_text( icounter, icounter_text );
}

void set_disassembly_region( unsigned int page )
{
    uint32_t i, posn;
    uint16_t op;
    char buf[80];
    char addr[10];
    char opcode[6] = "";
    char *arr[4] = { addr, " ", opcode, buf };
    unsigned int from = page & 0xFFFFF000;
    unsigned int to = from + 4096;
    
    gtk_clist_clear(disasm);

    sprintf( addr, "%08X", from );
    gtk_entry_set_text( page_field, addr );

    if( !mem_has_page( from ) ) {
        arr[3] = "This page is currently unmapped";
        gtk_clist_append( disasm, arr );
        gtk_clist_set_foreground( disasm, 0, &clrError );
    } else {
        for( i=from; i<to; i+=2 ) {
            sh4_disasm_instruction( i, buf, sizeof(buf) );
            sprintf( addr, "%08X", i );
            op = mem_read_phys_word(i);
            sprintf( opcode, "%02X %02X", op&0xFF, op>>8 );
            posn = gtk_clist_append( disasm, arr );
            if( buf[0] == '?' )
                gtk_clist_set_foreground( disasm, posn, &clrWarn );
        }
        if( disasm_pc != -1 && disasm_pc >= from && disasm_pc < to )
            gtk_clist_set_foreground( disasm, (disasm_pc - from)>>1,
                                      &clrPC );
    }

    if( page != from ) { /* not a page boundary */
        gtk_clist_moveto( disasm, (page-from)>>1, 0, 0.5, 0.0 );
    }
    disasm_from = from;
    disasm_to = to;
}

void jump_to_disassembly( unsigned int addr, gboolean select )
{
    int row;
    
    if( addr < disasm_from || addr >= disasm_to )
        set_disassembly_region(addr);

    row = (addr-disasm_from)>>1;
    if(select) {
        gtk_clist_select_row( disasm, row, 0 );
    }
    if( gtk_clist_row_is_visible( disasm, row ) != GTK_VISIBILITY_FULL ){
        gtk_clist_moveto( disasm, row, 0, 0.5, 0.0 );
    }
}

void set_disassembly_pc( unsigned int pc, gboolean select )
{
    int row;
    
    jump_to_disassembly( pc, select );
    if( disasm_pc != -1 && disasm_pc >= disasm_from && disasm_pc < disasm_to )
        gtk_clist_set_foreground( disasm, (disasm_pc - disasm_from)>>1,
                                  &clrNormal );
    row = (pc - disasm_from)>>1;
    gtk_clist_set_foreground( disasm, row, &clrPC );
    disasm_pc = pc;
}


void emit( int level, int source, char *msg, ... )
{
    char buf[20], addr[10] = "", *p;
    char *arr[3] = {buf, addr};
    int posn;
    time_t tm = time(NULL);
    va_list ap;

    va_start(ap, msg);
    p = g_strdup_vprintf( msg, ap );
    strftime( buf, sizeof(buf), "%H:%M:%S", localtime(&tm) );
    if( source != -1 )
        sprintf( addr, "%08X", sh4r.pc );
    arr[2] = p;
    posn = gtk_clist_append(msgs, arr);
    free(p);
    va_end(ap);

    gtk_clist_set_foreground( msgs, posn, msg_colors[level] );
    gtk_clist_moveto( msgs, posn, 0, 1.0, 0.0 );

    /* emit _really_ slows down the emu, to the point where the gui can be
     * completely unresponsive if I don't include this:
     */
    while( gtk_events_pending() )
        gtk_main_iteration();
}
