#include <gnome.h>


void
on_new_file1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_open1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_save1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_save_as1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_exit1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_preferences1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_load_btn_clicked                    (GtkButton       *button,
                                        gpointer         user_data);

void
on_reset_btn_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_stop_btn_clicked                    (GtkButton       *button,
                                        gpointer         user_data);

void
on_step_btn_clicked                    (GtkButton       *button,
                                        gpointer         user_data);

void
on_run_btn_clicked                     (GtkButton       *button,
                                        gpointer         user_data);

void
on_runto_btn_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_break_btn_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_debug_win_delete_event              (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_disasm_list_select_row              (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_disasm_list_unselect_row            (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_mem_mapped_regs1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_mmu_reglist_select_row              (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_mmu_regclose_clicked                (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_mmr_win_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_mmr_close_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_page_field_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_mode_field_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_disasm_list_select_row              (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_disasm_list_unselect_row            (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_page_locked_btn_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

gboolean
on_page_field_key_press_event          (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

void
on_output_list_select_row              (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_jump_pc_btn_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_memory1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_memory_win_delete_event             (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
button_view_clicked                    (GtkButton       *button,
                                        gpointer         user_data);

void
on_button_add_watch_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
on_button_clear_all_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
on_button_close_clicked                (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_dump_win_delete_event               (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_view_memory_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
