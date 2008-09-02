/**
 * $Id$
 *
 * Define the main (emu) GTK window, along with its menubars,
 * toolbars, etc.
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

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <X11/Xutil.h>

#include "lxdream.h"
#include "dreamcast.h"
#include "display.h"
#include "gdrom/gdrom.h"
#include "gtkui/gtkui.h"


struct main_window_info {
    GtkWidget *window;
    GtkWidget *video;
    GtkWidget *menubar;
    GtkWidget *toolbar;
    GtkWidget *statusbar;
    GtkActionGroup *actions;
    gboolean use_grab;
    gboolean is_grabbed;
    int32_t mouse_x, mouse_y;
};


/******************** Video window **************************/

#if !(GTK_CHECK_VERSION(2,8,0))
void gdk_display_warp_pointer (GdkDisplay *display,
                               GdkScreen  *screen,
                               gint        x,
                               gint        y);
#endif

/**
 * Adjust the mouse pointer so that it appears in the center of the video
 * window. Mainly used for when we have the mouse grab
 */
void video_window_center_pointer( main_window_t win )
{
    GdkDisplay *display = gtk_widget_get_display(win->video);
    GdkScreen *screen = gtk_widget_get_screen(win->video);
    int x,y;
    int width, height;

    gdk_window_get_origin(win->video->window, &x, &y);
    gdk_drawable_get_size(GDK_DRAWABLE(win->video->window), &width, &height);
    x += width / 2;
    y += height / 2;

    gdk_display_warp_pointer( display, screen, x, y );
    win->mouse_x = width/2;
    win->mouse_y = height/2;
}

/**
 * Grab the keyboard and mouse for the display. The mouse cursor is hidden and
 * moved to the centre of the window.
 *
 * @param win The window receiving the grab
 * @return TRUE if the grab was successful, FALSE on failure.
 */
gboolean video_window_grab_display( main_window_t win )
{
    GdkWindow *gdkwin = win->video->window;
    GdkColor color = { 0,0,0,0 };
    char bytes[32]; /* 16 * 16 / 8 */
    memset(bytes, 0, 32);
    GdkPixmap *pixmap = gdk_bitmap_create_from_data(NULL, bytes, 16, 16);
    GdkCursor *cursor = gdk_cursor_new_from_pixmap(pixmap, pixmap, &color, &color, 16, 16);
    gdk_pixmap_unref(pixmap);

    gboolean success =
        gdk_pointer_grab( gdkwin, FALSE, 
                GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK, 
                gdkwin, cursor, GDK_CURRENT_TIME ) == GDK_GRAB_SUCCESS;
    gdk_cursor_unref(cursor);
    if( success ) {
        success = gdk_keyboard_grab( gdkwin, FALSE, GDK_CURRENT_TIME ) == GDK_GRAB_SUCCESS;
        if( !success ) {
            gdk_pointer_ungrab(GDK_CURRENT_TIME);
        }
    }
    win->is_grabbed = success;
    main_window_set_running(win, dreamcast_is_running());
    return success;
}

/**
 * Release the display grab.
 */
void video_window_ungrab_display( main_window_t win )
{
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    gdk_keyboard_ungrab(GDK_CURRENT_TIME);
    win->is_grabbed = FALSE;
    main_window_set_running(win, dreamcast_is_running());
}

static gboolean on_video_window_mouse_motion( GtkWidget *widget, GdkEventMotion *event,
                                              gpointer user_data )
{
    main_window_t win = (main_window_t)user_data;
    int32_t x = (int32_t)event->x;
    int32_t y = (int32_t)event->y;
    if( win->is_grabbed && 
            (x != win->mouse_x || y != win->mouse_y) ) {
        uint32_t buttons = (event->state >> 8)&0x1F;
        input_event_mouse( buttons, x - win->mouse_x, y - win->mouse_y, FALSE );
        video_window_center_pointer(win);
    }
    return TRUE;
}

static gboolean on_video_window_mouse_pressed( GtkWidget *widget, GdkEventButton *event,
                                               gpointer user_data )
{
    main_window_t win = (main_window_t)user_data;
    // Get the buttons from the event state, and add the pressed button
    uint32_t buttons = ((event->state >> 8) & 0x1F) | (1<<(event->button-1));
    if( win->is_grabbed ) {
        input_event_mouse( buttons, 0, 0, FALSE );
    } else {
        input_event_mouse( buttons, (int)event->x, (int)event->y, TRUE );
    }
    return TRUE;
}

static gboolean on_video_window_mouse_released( GtkWidget *widget, GdkEventButton *event,
                                                gpointer user_data )
{
    main_window_t win = (main_window_t)user_data;
    // Get the buttons from the event state, and remove the released button
    uint32_t buttons = ((event->state >> 8) & 0x1F) & (~(1<<(event->button-1)));
    if( win->is_grabbed ) {
        input_event_mouse( buttons, 0, 0, FALSE );
    } else if( win->use_grab) {
        video_window_grab_display(win);
    } else {
        input_event_mouse( buttons, (int)event->x, (int)event->y, TRUE );
    }        
    return TRUE;
}

static gboolean on_video_window_key_pressed( GtkWidget *widget, GdkEventKey *event,
                                             gpointer user_data )
{
    main_window_t win = (main_window_t)user_data;
    if( win->is_grabbed ) {
#ifdef HAVE_GTK_OSX
        /* On OSX, use the command key rather than ctrl-alt. Mainly because GTK/OSX 
         * doesn't seem to be able to get ctrl-alt reliably 
         **/
        if( event->keyval == GDK_Meta_L || event->keyval == GDK_Meta_R ) {
            video_window_ungrab_display(win);
            return TRUE;
        }
#else    	
        /* Check for ungrab key combo (ctrl-alt). Unfortunately GDK sends it as
         * a singly-modified keypress rather than a double-modified 'null' press, 
         * so we have to do a little more work.
         * Only check Ctrl/Shift/Alt for state - don't want to check numlock/capslock/
         * mouse buttons/etc
         */
        int mod = gdk_keycode_to_modifier(gtk_widget_get_display(widget), event->hardware_keycode);
        int state = event->state & gtk_accelerator_get_default_mod_mask();
        if( (state == GDK_CONTROL_MASK && mod == GDK_MOD1_MASK) ||
                (state == GDK_MOD1_MASK && mod == GDK_CONTROL_MASK) ) {
            video_window_ungrab_display(win);
            // Consume the keypress, DC doesn't get it.
            return TRUE;
        }
#endif
    }
    input_event_keydown( NULL, gtk_get_unmodified_keyval(event), 1 );
    return TRUE;
}

static gboolean on_video_window_key_released( GtkWidget *widget, GdkEventKey *event,
                                              gpointer user_data )
{
    input_event_keyup( NULL, gtk_get_unmodified_keyval(event), 0 );
    return TRUE;
}

static gboolean on_video_window_focus_changed( GtkWidget *widget, GdkEventFocus *event,
                                               gpointer user_data )
{
    display_set_focused(event->in);
    return TRUE;
}

/*************************** Main window (frame) ******************************/

static gboolean on_main_window_deleted( GtkWidget *widget, GdkEvent event, gpointer user_data )
{
    dreamcast_shutdown();
    exit(0);
}

static void on_main_window_state_changed( GtkWidget *widget, GdkEventWindowState *state, 
                                          gpointer userdata )
{
    main_window_t win = (main_window_t)userdata;
    if( state->changed_mask & GDK_WINDOW_STATE_FULLSCREEN ) {
        gboolean fs = (state->new_window_state & GDK_WINDOW_STATE_FULLSCREEN);
        GtkWidget *frame = gtk_widget_get_parent(win->video);
        if( frame->style == NULL ) {
            gtk_widget_set_style( frame, gtk_style_new() );
        }
        if( fs ) {
            gtk_widget_hide( win->menubar );
            gtk_widget_hide( win->toolbar );
            gtk_widget_hide( win->statusbar );

            frame->style->xthickness = 0;
            frame->style->ythickness = 0;
        } else {
            frame->style->xthickness = 2;
            frame->style->ythickness = 2;
            gtk_widget_show( win->menubar );
            gtk_widget_show( win->toolbar );
            gtk_widget_show( win->statusbar );
        }
        gtk_widget_queue_draw( win->window );
    }
}

main_window_t main_window_new( const gchar *title, GtkWidget *menubar, GtkWidget *toolbar,
                               GtkAccelGroup *accel_group )
{
    GtkWidget *vbox;
    GtkWidget *frame;
    main_window_t win = g_malloc0( sizeof(struct main_window_info) );

    win->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    win->menubar = menubar;
    win->toolbar = toolbar;
    win->use_grab = FALSE;
    win->is_grabbed = FALSE;
    gtk_window_set_title( GTK_WINDOW(win->window), title );
    gtk_window_add_accel_group (GTK_WINDOW (win->window), accel_group);

    gtk_toolbar_set_style( GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS );

    win->video = video_gtk_create_drawable();
    gtk_widget_set_size_request( win->video, 640, 480 ); 
    gtk_widget_set_double_buffered( win->video, FALSE );
    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type( GTK_FRAME(frame), GTK_SHADOW_IN );
    gtk_container_add( GTK_CONTAINER(frame), win->video );

    win->statusbar = gtk_statusbar_new();

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add( GTK_CONTAINER(win->window), vbox );
    gtk_box_pack_start( GTK_BOX(vbox), menubar, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), toolbar, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), frame, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), win->statusbar, FALSE, FALSE, 0 );
    gtk_widget_show_all( win->window );
    gtk_widget_grab_focus( win->video );

    gtk_statusbar_push( GTK_STATUSBAR(win->statusbar), 1, "Stopped" );

    g_signal_connect( win->window, "delete_event", 
                      G_CALLBACK(on_main_window_deleted), win );
    g_signal_connect( win->window, "window-state-event",
                      G_CALLBACK(on_main_window_state_changed), win );

    g_signal_connect( win->video, "key-press-event",
                      G_CALLBACK(on_video_window_key_pressed), win );
    g_signal_connect( win->video, "key-release-event",
                      G_CALLBACK(on_video_window_key_released), win );
    g_signal_connect( win->video, "motion-notify-event",
                      G_CALLBACK(on_video_window_mouse_motion), win );
    g_signal_connect( win->video, "button-press-event",
                      G_CALLBACK(on_video_window_mouse_pressed), win );
    g_signal_connect( win->video, "button-release-event", 
                      G_CALLBACK(on_video_window_mouse_released), win );
    g_signal_connect( win->video, "focus-in-event",
                      G_CALLBACK(on_video_window_focus_changed), win);
    g_signal_connect( win->video, "focus-out-event",
                      G_CALLBACK(on_video_window_focus_changed), win);

    gtk_widget_add_events( win->video, 
                           GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
                           GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                           GDK_POINTER_MOTION_MASK | GDK_FOCUS_CHANGE_MASK );

    return win;
}

void main_window_set_status_text( main_window_t win, char *text )
{
    gtk_statusbar_pop( GTK_STATUSBAR(win->statusbar), 1 );
    if( win->is_grabbed ) {
        char buf[128];
#ifdef HAVE_GTK_OSX
        snprintf( buf, sizeof(buf), "%s %s", text, _("(Press <command> to release grab)") );
#else	
        snprintf( buf, sizeof(buf), "%s %s", text, _("(Press <ctrl><alt> to release grab)") );
#endif
        gtk_statusbar_push( GTK_STATUSBAR(win->statusbar), 1, buf );
    } else {
        gtk_statusbar_push( GTK_STATUSBAR(win->statusbar), 1, text );
    }
}

void main_window_set_running( main_window_t win, gboolean running )
{
    char *text = running ? _("Running") : _("Stopped");
    gtk_gui_enable_action( "Pause", running );
    gtk_gui_enable_action( "Run", !running && dreamcast_can_run() );
    main_window_set_status_text( win, text );
}

void main_window_set_framerate( main_window_t win, float rate )
{


}

void main_window_set_speed( main_window_t win, double speed )
{
    char buf[32];

    snprintf( buf, 32, "Running (%2.4f%%)", speed );
    main_window_set_status_text( win, buf );
}

GtkWidget *main_window_get_renderarea( main_window_t win )
{
    return win->video;
}

GtkWindow *main_window_get_frame( main_window_t win )
{
    return GTK_WINDOW(win->window);
}

void main_window_set_fullscreen( main_window_t win, gboolean fullscreen )
{
    if( fullscreen ) {
        gtk_window_fullscreen( GTK_WINDOW(win->window) );
    } else {
        gtk_window_unfullscreen( GTK_WINDOW(win->window) );
    }
}

void main_window_set_use_grab( main_window_t win, gboolean use_grab )
{
    if( use_grab != win->use_grab ) {
        if( use_grab ) {
            GdkCursor *cursor = gdk_cursor_new( GDK_HAND2 );
            gdk_window_set_cursor( win->video->window, cursor );
            gdk_cursor_unref( cursor );
        } else {
            gdk_window_set_cursor( win->video->window, NULL );
            if( gdk_pointer_is_grabbed() ) {
                video_window_ungrab_display(win);
            }
        }
        win->use_grab = use_grab;
    }
}

void main_window_update_title( main_window_t win )
{
    const char *disc = gdrom_get_current_disc_title();
    
    if( disc == NULL ) {
        gtk_window_set_title( GTK_WINDOW(win->window), lxdream_package_name );
    } else {
        char buf[256];
        snprintf( buf, sizeof(buf), "%s - %s", lxdream_package_name, disc );
        gtk_window_set_title( GTK_WINDOW(win->window), buf );
    }
}
