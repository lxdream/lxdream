/**
 * $Id$
 *
 * The PC side of the video support (responsible for actually displaying / 
 * rendering frames)
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

#include <gdk/gdkkeysyms.h>
#include <stdint.h>
#include <stdlib.h>
#include "lxdream.h"
#include "display.h"
#include "dckeysyms.h"
#include "drivers/video_gl.h"
#include "drivers/joy_linux.h"
#include "pvr2/pvr2.h"
#include "gtkui/gtkui.h"

#ifdef HAVE_GTK_X11

#include <gdk/gdkx.h>
#include "drivers/video_glx.h"

/************* X11-specificness **********/

guint gdk_keycode_to_modifier( GdkDisplay *display, guint keycode )
{
    int i;
    int result = 0;
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
    XModifierKeymap *keymap = XGetModifierMapping( xdisplay );
    for( i=0; i<8*keymap->max_keypermod; i++ ) {
        if( keymap->modifiermap[i] == keycode ) {
            result = 1 << (i/keymap->max_keypermod);
            break;
        }
    }
    XFreeModifiermap(keymap);
    return result;
}

#if !(GTK_CHECK_VERSION(2,8,0))
/* gdk_display_warp_pointer was added in GTK 2.8. If we're using an earlier
 * version, include the code here. (Can't just set the dependency on 2.8 as
 * it still hasn't been included in fink yet...) Original copyright statement
 * below.
 */

/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */
void gdk_display_warp_pointer (GdkDisplay *display,
                               GdkScreen  *screen,
                               gint        x,
                               gint        y)
{
    Display *xdisplay;
    Window dest;

    xdisplay = GDK_DISPLAY_XDISPLAY (display);
    dest = GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen));

    XWarpPointer (xdisplay, None, dest, 0, 0, 0, 0, x, y);  
}

#endif

#endif

#ifdef HAVE_GTK_OSX
#include "drivers/video_nsgl.h"

// Include this prototype as some systems don't have gdkquartz.h installed
NSView  *gdk_quartz_window_get_nsview( GdkWindow *window);

guint gdk_keycode_to_modifier( GdkDisplay *display, guint keycode )
{
    return 0;
}

#endif



GtkWidget *gtk_video_drawable = NULL;
int video_width = 640;
int video_height = 480;

gboolean video_gtk_init();
void video_gtk_shutdown();
void video_gtk_display_blank( uint32_t colour );
uint16_t video_gtk_resolve_keysym( const gchar *keysym );
uint16_t video_gtk_keycode_to_dckeysym(uint16_t keycode);

struct display_driver display_gtk_driver = { 
        "gtk",
        N_("GTK-based OpenGL driver"),
        video_gtk_init, 
        video_gtk_shutdown,
        video_gtk_resolve_keysym,
        video_gtk_keycode_to_dckeysym,
        NULL,
        NULL, NULL, NULL, NULL, NULL, 
        video_gtk_display_blank, NULL };

uint16_t video_gtk_resolve_keysym( const gchar *keysym )
{
    int val = gdk_keyval_from_name( keysym );
    if( val == GDK_VoidSymbol )
        return 0;
    return (uint16_t)val;
}

gboolean video_gtk_expose_callback(GtkWidget *widget, GdkEventExpose *event, gpointer data )
{
    pvr2_redraw_display();
    return TRUE;
}

gboolean video_gtk_resize_callback(GtkWidget *widget, GdkEventConfigure *event, gpointer data )
{
    video_width = event->width;
    video_height = event->height;
    pvr2_redraw_display();
    return TRUE;
}

uint16_t video_gtk_keycode_to_dckeysym(uint16_t keycode)
{
    if( keycode >= 'a' && keycode <= 'z' ) {
        return (keycode - 'a') + DCKB_a;
    } else if( keycode >= '1' && keycode <= '9' ) {
        return (keycode - '1') + DCKB_1;
    }
    switch(keycode) {
    case GDK_0:         return DCKB_0;
    case GDK_Return:    return DCKB_Return;
    case GDK_Escape:    return DCKB_Escape;
    case GDK_BackSpace: return DCKB_BackSpace;
    case GDK_Tab:       return DCKB_Tab;
    case GDK_space:     return DCKB_space;
    case GDK_minus:     return DCKB_minus;
    case GDK_equal:     return DCKB_equal;
    case GDK_bracketleft: return DCKB_bracketleft;
    case GDK_bracketright: return DCKB_bracketright;
    case GDK_semicolon: return DCKB_semicolon;
    case GDK_apostrophe:return DCKB_apostrophe;
    case GDK_grave : return DCKB_grave;
    case GDK_comma:     return DCKB_comma;
    case GDK_period:    return DCKB_period;
    case GDK_slash:     return DCKB_slash; 
    case GDK_Caps_Lock: return DCKB_Caps_Lock;
    case GDK_F1:        return DCKB_F1;
    case GDK_F2:        return DCKB_F2;
    case GDK_F3:        return DCKB_F3;
    case GDK_F4:        return DCKB_F4;
    case GDK_F5:        return DCKB_F5;
    case GDK_F6:        return DCKB_F6;
    case GDK_F7:        return DCKB_F7;
    case GDK_F8:        return DCKB_F8;
    case GDK_F9:        return DCKB_F9;
    case GDK_F10:       return DCKB_F10;
    case GDK_F11:       return DCKB_F11;
    case GDK_F12:       return DCKB_F12;
    case GDK_Scroll_Lock: return DCKB_Scroll_Lock;
    case GDK_Pause:     return DCKB_Pause;
    case GDK_Insert:    return DCKB_Insert;
    case GDK_Home:      return DCKB_Home;
    case GDK_Page_Up:   return DCKB_Page_Up;
    case GDK_Delete:    return DCKB_Delete;
    case GDK_End:       return DCKB_End;
    case GDK_Page_Down: return DCKB_Page_Down;
    case GDK_Right:     return DCKB_Right;
    case GDK_Left:      return DCKB_Left;
    case GDK_Down:      return DCKB_Down;
    case GDK_Up:        return DCKB_Up;
    case GDK_Num_Lock:  return DCKB_Num_Lock;
    case GDK_KP_Divide: return DCKB_KP_Divide;
    case GDK_KP_Multiply: return DCKB_KP_Multiply;
    case GDK_KP_Subtract: return DCKB_KP_Subtract;
    case GDK_KP_Add:    return DCKB_KP_Add;
    case GDK_KP_Enter:  return DCKB_KP_Enter;
    case GDK_KP_End:    return DCKB_KP_End;
    case GDK_KP_Down:   return DCKB_KP_Down;
    case GDK_KP_Page_Down: return DCKB_KP_Page_Down;
    case GDK_KP_Left:   return DCKB_KP_Left;
    case GDK_KP_Begin:  return DCKB_KP_Begin;
    case GDK_KP_Right:  return DCKB_KP_Right;
    case GDK_KP_Home:   return DCKB_KP_Home;
    case GDK_KP_Up:     return DCKB_KP_Up;
    case GDK_KP_Page_Up:return DCKB_KP_Page_Up;
    case GDK_KP_Insert: return DCKB_KP_Insert;
    case GDK_KP_Delete: return DCKB_KP_Delete;
    case GDK_backslash: return DCKB_backslash;
    case GDK_Control_L: return DCKB_Control_L;
    case GDK_Shift_L:   return DCKB_Shift_L;
    case GDK_Alt_L:     return DCKB_Alt_L;
    case GDK_Meta_L:    return DCKB_Meta_L;
    case GDK_Control_R: return DCKB_Control_R;
    case GDK_Shift_R:   return DCKB_Shift_R;
    case GDK_Alt_R:     return DCKB_Alt_R;
    case GDK_Meta_R:    return DCKB_Meta_R;
    }
    return DCKB_NONE;
}

GtkWidget *video_gtk_create_drawable()
{
    GtkWidget *drawable = gtk_drawing_area_new();
    GTK_WIDGET_SET_FLAGS(drawable, GTK_CAN_FOCUS|GTK_CAN_DEFAULT);

    g_signal_connect( drawable, "expose_event",
                      G_CALLBACK(video_gtk_expose_callback), NULL );
    g_signal_connect( drawable, "configure_event",
                      G_CALLBACK(video_gtk_resize_callback), NULL );

#ifdef HAVE_GLX
    Display *display = gdk_x11_display_get_xdisplay( gtk_widget_get_display(drawable));
    Screen *screen = gdk_x11_screen_get_xscreen( gtk_widget_get_screen(drawable));
    int screen_no = XScreenNumberOfScreen(screen);
    if( !video_glx_init(display, screen_no) ) {
        ERROR( "Unable to initialize GLX, aborting" );
        exit(3);
    }

    XVisualInfo *visual = video_glx_get_visual();
    if( visual != NULL ) {
        GdkVisual *gdkvis = gdk_x11_screen_lookup_visual( gtk_widget_get_screen(drawable), visual->visualid );
        GdkColormap *colormap = gdk_colormap_new( gdkvis, FALSE );
        gtk_widget_set_colormap( drawable, colormap );
    }
#endif
    gtk_video_drawable = drawable;
    return drawable;
}

gboolean video_gtk_init()
{

    if( gtk_video_drawable == NULL ) {
        return FALSE;
    }

    video_width = gtk_video_drawable->allocation.width;
    video_height = gtk_video_drawable->allocation.height;
#ifdef HAVE_OSMESA
    video_gdk_init_driver( &display_gtk_driver );
#else
#ifdef HAVE_GLX
    Display *display = gdk_x11_display_get_xdisplay( gtk_widget_get_display(GTK_WIDGET(gtk_video_drawable)));
    Window window = GDK_WINDOW_XWINDOW( GTK_WIDGET(gtk_video_drawable)->window );
    if( ! video_glx_init_context( display, window ) ||
            ! video_glx_init_driver( &display_gtk_driver ) ) {
        return FALSE;
    }
#else
#ifdef HAVE_NSGL
    NSView *view = gdk_quartz_window_get_nsview(gtk_video_drawable->window);
    if( ! video_nsgl_init_driver( view, &display_gtk_driver ) ) {
        return FALSE;
    }
#endif
#endif
#endif

    pvr2_setup_gl_context();

#ifdef HAVE_LINUX_JOYSTICK
    linux_joystick_init();
#endif
    return TRUE;
}

void video_gtk_display_blank( uint32_t colour )
{
    GdkGC *gc = gdk_gc_new(gtk_video_drawable->window);
    GdkColor color = {0, ((colour>>16)&0xFF)*257, ((colour>>8)&0xFF)*257, ((colour)&0xFF)*257 };
    GdkColormap *cmap = gdk_colormap_get_system();
    gdk_colormap_alloc_color( cmap, &color, TRUE, TRUE );
    gdk_gc_set_foreground( gc, &color );
    gdk_gc_set_background( gc, &color );
    gdk_draw_rectangle( gtk_video_drawable->window, gc, TRUE, 0, 0, video_width, video_height );
    gdk_gc_destroy(gc);
    gdk_colormap_free_colors( cmap, &color, 1 );
}

void video_gtk_shutdown()
{
    if( gtk_video_drawable != NULL ) {
#ifdef HAVE_OSMESA
        video_gdk_shutdown();
#else
#ifdef HAVE_GLX
        video_glx_shutdown();
#else
#ifdef HAVE_NSGL
        video_nsgl_shutdown();
#endif
#endif
#endif
    }
#ifdef HAVE_LINUX_JOYSTICK
    linux_joystick_shutdown();
#endif
}

