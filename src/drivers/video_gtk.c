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
#include "lxdream.h"
#include "display.h"
#include "dckeysyms.h"
#include "drivers/video_gl.h"
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

GtkWidget *gtk_video_drawable = NULL;
int video_width = 640;
int video_height = 480;

gboolean video_gtk_init();
void video_gtk_shutdown();
gboolean video_gtk_display_blank( uint32_t colour );
uint16_t video_gtk_resolve_keysym( const gchar *keysym );
uint16_t video_gtk_keycode_to_dckeysym(uint16_t keycode);

struct display_driver display_gtk_driver = { "gtk", video_gtk_init, video_gtk_shutdown,
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
    render_buffer_t buffer = pvr2_get_front_buffer();
    if( buffer == NULL ) {
	display_gtk_driver.display_blank(pvr2_get_border_colour());
    } else {
	display_gtk_driver.display_render_buffer(buffer);
    }
    return TRUE;
}

gboolean video_gtk_resize_callback(GtkWidget *widget, GdkEventConfigure *event, gpointer data )
{
    video_width = event->width;
    video_height = event->height;
    video_gtk_expose_callback(widget, NULL, data);
    return TRUE;
}

uint16_t video_gtk_keycode_to_dckeysym(uint16_t keycode)
{
    if( keycode >= 'a' && keycode <= 'z' ) {
	return (keycode - 'a') + DCKB_A;
    } else if( keycode >= '1' && keycode <= '9' ) {
	return (keycode - '1') + DCKB_1;
    }
    switch(keycode) {
    case GDK_0:         return DCKB_0;
    case GDK_Return:    return DCKB_ENTER;
    case GDK_Escape:    return DCKB_ESCAPE;
    case GDK_BackSpace: return DCKB_BACKSPACE;
    case GDK_Tab:       return DCKB_TAB;
    case GDK_space:     return DCKB_SPACE;
    case GDK_minus:     return DCKB_MINUS;
    case GDK_equal:     return DCKB_EQUAL;
    case GDK_bracketleft: return DCKB_LBRACKET;
    case GDK_bracketright: return DCKB_RBRACKET;
    case GDK_semicolon: return DCKB_SEMICOLON;
    case GDK_apostrophe:return DCKB_QUOTE;
    case GDK_grave : return DCKB_BACKQUOTE;
    case GDK_comma:     return DCKB_COMMA;
    case GDK_period:    return DCKB_PERIOD;
    case GDK_slash:     return DCKB_SLASH; 
    case GDK_Caps_Lock: return DCKB_CAPSLOCK;
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
    case GDK_Scroll_Lock: return DCKB_SCROLLLOCK;
    case GDK_Pause:     return DCKB_PAUSE;
    case GDK_Insert:    return DCKB_INSERT;
    case GDK_Home:      return DCKB_HOME;
    case GDK_Page_Up:   return DCKB_PAGEUP;
    case GDK_Delete:    return DCKB_DELETE;
    case GDK_End:       return DCKB_END;
    case GDK_Page_Down: return DCKB_PAGEDOWN;
    case GDK_Right:     return DCKB_RIGHT;
    case GDK_Left:      return DCKB_LEFT;
    case GDK_Down:      return DCKB_DOWN;
    case GDK_Up:        return DCKB_UP;
    case GDK_Num_Lock:  return DCKB_NUMLOCK;
    case GDK_KP_Divide: return DCKB_KP_SLASH;
    case GDK_KP_Multiply: return DCKB_KP_STAR;
    case GDK_KP_Subtract: return DCKB_KP_MINUS;
    case GDK_KP_Add:    return DCKB_KP_PLUS;
    case GDK_KP_Enter:  return DCKB_KP_ENTER;
    case GDK_KP_End:    return DCKB_KP_1;
    case GDK_KP_Down:   return DCKB_KP_2;
    case GDK_KP_Page_Down: return DCKB_KP_3;
    case GDK_KP_Left:   return DCKB_KP_4;
    case GDK_KP_Begin:  return DCKB_KP_5;
    case GDK_KP_Right:  return DCKB_KP_6;
    case GDK_KP_Home:   return DCKB_KP_7;
    case GDK_KP_Up:     return DCKB_KP_8;
    case GDK_KP_Page_Up:return DCKB_KP_9;
    case GDK_KP_Insert: return DCKB_KP_0;
    case GDK_KP_Delete: return DCKB_KP_PERIOD;
    case GDK_backslash: return DCKB_BACKSLASH;
    case GDK_Control_L: return DCKB_CONTROL_L;
    case GDK_Shift_L:   return DCKB_SHIFT_L;
    case GDK_Alt_L:     return DCKB_ALT_L;
    case GDK_Meta_L:    return DCKB_S1;
    case GDK_Control_R: return DCKB_CONTROL_R;
    case GDK_Shift_R:   return DCKB_SHIFT_R;
    case GDK_Alt_R:     return DCKB_ALT_R;
    case GDK_Meta_R:    return DCKB_S2;
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
#endif
#endif

#ifdef HAVE_LINUX_JOYSTICK
    linux_joystick_init();
#endif
    return TRUE;
}

gboolean video_gtk_display_blank( uint32_t colour )
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
#endif
#endif
    }
#ifdef HAVE_LINUX_JOYSTICK
    linux_joystick_shutdown();
#endif
}

