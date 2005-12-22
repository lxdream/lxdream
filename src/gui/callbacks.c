#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "callbacks.h"
#include "interface.h"
#include "gui.h"
#include "sh4core.h"
#include "asic.h"
#include "dreamcast.h"

int selected_pc = -1;

void
on_new_file1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_open1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    open_file_dialog( "Open...", open_file, NULL, NULL );
}


void
on_save1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_save_as1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_exit1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    gtk_main_quit();
}


void
on_preferences1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkWidget *about = create_about_win();
    gtk_widget_show(about);
}


void
on_load_btn_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
    open_file_dialog( "Open...", load_bin_file, NULL, NULL );
}


void
on_reset_btn_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
    sh4_reset();
    mem_reset();
    update_gui();
}


void
on_stop_btn_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
    if( sh4_isrunning() ) {
        sh4_stop();
    }
}


void
on_step_btn_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
    sh4_execute_instruction();
    update_gui();
}


void run( debug_info_t data, uint32_t target ) {
    if( ! sh4_isrunning() ) {
        do {
            if( target == -1 )
                sh4_runfor(1000000);
            else
                sh4_runto(target, 1000000);
            update_icount(data);
            run_timers(1000000);
	    SCIF_clock_tick();
            while( gtk_events_pending() )
                gtk_main_iteration();
            pvr2_next_frame();
        } while( sh4_isrunning() );
        update_gui();
    }    
}
void
on_run_btn_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
    debug_info_t data = get_debug_info(GTK_WIDGET(button));
    run(data,-1);
}


void
on_runto_btn_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
    debug_info_t data = get_debug_info(GTK_WIDGET(button));
    if( selected_pc == -1 )
        WARN( "No address selected, so can't run to it", NULL );
    else {
        INFO( "Running until %08X...", selected_pc );
        run( data, selected_pc );
    }
}


void
on_break_btn_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{

}


gboolean
on_debug_win_delete_event              (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    gtk_main_quit();
  return FALSE;
}


void
on_disasm_list_select_row              (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    debug_info_t data = get_debug_info(GTK_WIDGET(clist));
    selected_pc = row_to_address(data, row);
}


void
on_disasm_list_unselect_row            (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    debug_info_t data = get_debug_info(GTK_WIDGET(clist));
    int pc = row_to_address(data,row);
    if( selected_pc == pc ) selected_pc = -1;
}


void
on_mem_mapped_regs1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    mmr_open_win();
}


gboolean
on_mmr_win_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    mmr_close_win();
    return TRUE;
}


void
on_mmr_close_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
    mmr_close_win();
}


void
on_mode_field_changed                  (GtkEditable     *editable,
                                        gpointer         user_data)
{
    const gchar *text = gtk_entry_get_text( GTK_ENTRY(editable) );
    debug_info_t data = get_debug_info( GTK_WIDGET(editable) );
    set_disassembly_cpu( data, text );
}


void
on_page_locked_btn_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{

}


gboolean
on_page_field_key_press_event          (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{
    if( event->keyval == GDK_Return || event->keyval == GDK_Linefeed ) {
	debug_info_t data = get_debug_info(widget);
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


void
on_output_list_select_row              (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    if( event->type == GDK_2BUTTON_PRESS && event->button.button == 1 ) {
        char *val;
        gtk_clist_get_text( clist, row, 1, &val );
        if( val[0] != '\0' ) {
            int addr = strtoul( val, NULL, 16 );
	    debug_info_t data = get_debug_info( GTK_WIDGET(clist) );
            jump_to_disassembly( data, addr, TRUE );
        }
    }
}


void
on_jump_pc_btn_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
    debug_info_t data = get_debug_info( GTK_WIDGET(button) );
    jump_to_pc( data, TRUE );
}


void
on_button_add_watch_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_button_clear_all_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_button_close_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_view_memory_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    dump_window_new();
}


void
on_loadstate_button_clicked            (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
    open_file_dialog( "Load state...", dreamcast_load_state, "*.dst", "DreamOn Save State (*.dst)" );
}


void
on_savestate_button_clicked            (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
    save_file_dialog( "Save state...", dreamcast_save_state, "*.dst", "DreamOn Save State (*.dst)" );
}

