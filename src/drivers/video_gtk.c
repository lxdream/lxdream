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

#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <stdint.h>
#include "dream.h"
#include "display.h"
#include "dckeysyms.h"
#include "drivers/video_glx.h"
#include "drivers/gl_common.h"
#include "pvr2/pvr2.h"
#include "gtkui/gtkui.h"

static GtkWidget *video_win = NULL;
int video_width = 640;
int video_height = 480;

gboolean video_gtk_init();
void video_gtk_shutdown();
gboolean video_gtk_display_blank( uint32_t colour );
uint16_t video_gtk_resolve_keysym( const gchar *keysym );
uint16_t video_gtk_keycode_to_dckeysym(uint32_t keycode);

struct display_driver display_gtk_driver = { "gtk", video_gtk_init, video_gtk_shutdown,
					     video_gtk_resolve_keysym,
					     video_gtk_keycode_to_dckeysym,
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

uint16_t video_gtk_keycode_to_dckeysym(uint32_t keycode)
{
    if( keycode >= 'a' && keycode <= 'z' ) {
	return (keycode - 'a') + DCKB_A;
    } else if( keycode >= '1' && keycode <= '9' ) {
	return (keycode - '1') + DCKB_1;
    }
    switch(keycode) {
    case XK_0:         return DCKB_0;
    case XK_Return:    return DCKB_ENTER;
    case XK_Escape:    return DCKB_ESCAPE;
    case XK_BackSpace: return DCKB_BACKSPACE;
    case XK_Tab:       return DCKB_TAB;
    case XK_space:     return DCKB_SPACE;
    case XK_minus:     return DCKB_MINUS;
    case XK_equal:     return DCKB_EQUAL;
    case XK_bracketleft: return DCKB_LBRACKET;
    case XK_bracketright: return DCKB_RBRACKET;
    case XK_semicolon: return DCKB_SEMICOLON;
    case XK_apostrophe:return DCKB_QUOTE;
    case XK_grave : return DCKB_BACKQUOTE;
    case XK_comma:     return DCKB_COMMA;
    case XK_period:    return DCKB_PERIOD;
    case XK_slash:     return DCKB_SLASH; 
    case XK_Caps_Lock: return DCKB_CAPSLOCK;
    case XK_F1:        return DCKB_F1;
    case XK_F2:        return DCKB_F2;
    case XK_F3:        return DCKB_F3;
    case XK_F4:        return DCKB_F4;
    case XK_F5:        return DCKB_F5;
    case XK_F6:        return DCKB_F6;
    case XK_F7:        return DCKB_F7;
    case XK_F8:        return DCKB_F8;
    case XK_F9:        return DCKB_F9;
    case XK_F10:       return DCKB_F10;
    case XK_F11:       return DCKB_F11;
    case XK_F12:       return DCKB_F12;
    case XK_Scroll_Lock: return DCKB_SCROLLLOCK;
    case XK_Pause:     return DCKB_PAUSE;
    case XK_Insert:    return DCKB_INSERT;
    case XK_Home:      return DCKB_HOME;
    case XK_Page_Up:   return DCKB_PAGEUP;
    case XK_Delete:    return DCKB_DELETE;
    case XK_End:       return DCKB_END;
    case XK_Page_Down: return DCKB_PAGEDOWN;
    case XK_Right:     return DCKB_RIGHT;
    case XK_Left:      return DCKB_LEFT;
    case XK_Down:      return DCKB_DOWN;
    case XK_Up:        return DCKB_UP;
    case XK_Num_Lock:  return DCKB_NUMLOCK;
    case XK_KP_Divide: return DCKB_KP_SLASH;
    case XK_KP_Multiply: return DCKB_KP_STAR;
    case XK_KP_Subtract: return DCKB_KP_MINUS;
    case XK_KP_Add:    return DCKB_KP_PLUS;
    case XK_KP_Enter:  return DCKB_KP_ENTER;
    case XK_KP_1:      return DCKB_KP_1;
    case XK_KP_2:      return DCKB_KP_2;
    case XK_KP_3:      return DCKB_KP_3;
    case XK_KP_4:      return DCKB_KP_4;
    case XK_KP_5:      return DCKB_KP_5;
    case XK_KP_6:      return DCKB_KP_6;
    case XK_KP_7:      return DCKB_KP_7;
    case XK_KP_8:      return DCKB_KP_8;
    case XK_KP_9:      return DCKB_KP_9;
    case XK_KP_0:      return DCKB_KP_0;
    case XK_KP_Decimal:return DCKB_KP_PERIOD;
    case XK_backslash: return DCKB_BACKSLASH;
    case XK_Control_L: return DCKB_CONTROL_L;
    case XK_Shift_L:   return DCKB_SHIFT_L;
    case XK_Alt_L:     return DCKB_ALT_L;
    case XK_Meta_L:    return DCKB_S1;
    case XK_Control_R: return DCKB_CONTROL_R;
    case XK_Shift_R:   return DCKB_SHIFT_R;
    case XK_Alt_R:     return DCKB_ALT_R;
    case XK_Meta_R:    return DCKB_S2;
    }
    return DCKB_NONE;
}

gboolean video_gtk_init()
{
  
    video_win = gtk_gui_get_renderarea();
    if( video_win == NULL ) {
	return FALSE;
    }

    g_signal_connect( video_win, "expose_event",
		      G_CALLBACK(video_gtk_expose_callback), NULL );
    g_signal_connect( video_win, "configure_event",
		      G_CALLBACK(video_gtk_resize_callback), NULL );
    video_width = video_win->allocation.width;
    video_height = video_win->allocation.height;
    Display *display = gdk_x11_display_get_xdisplay( gtk_widget_get_display(GTK_WIDGET(video_win)));
    Window window = GDK_WINDOW_XWINDOW( GTK_WIDGET(video_win)->window );
    if( ! video_glx_init_context( display, window ) ||
        ! video_glx_init_driver( &display_gtk_driver ) ) {
        return FALSE;
    }
    return TRUE;
}

gboolean video_gtk_display_blank( uint32_t colour )
{
    GdkGC *gc = gdk_gc_new(video_win->window);
    GdkColor color = {0, ((colour>>16)&0xFF)*257, ((colour>>8)&0xFF)*257, ((colour)&0xFF)*257 };
    GdkColormap *cmap = gdk_colormap_get_system();
    gdk_colormap_alloc_color( cmap, &color, TRUE, TRUE );
    gdk_gc_set_foreground( gc, &color );
    gdk_gc_set_background( gc, &color );
    gdk_draw_rectangle( video_win->window, gc, TRUE, 0, 0, video_width, video_height );
    gdk_gc_destroy(gc);
    gdk_colormap_free_colors( cmap, &color, 1 );
}

void video_gtk_shutdown()
{
    if( video_win != NULL ) {
	video_glx_shutdown();
    }

}

