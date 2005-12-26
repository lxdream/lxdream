/**
 * $Id: main.c,v 1.8 2005-12-26 03:11:14 nkeynes Exp $
 *
 * Main program, initializes dreamcast and gui, then passes control off to
 * the gtk main loop (currently). 
 *
 * FIXME: Remove explicit GTK/Gnome references from this file
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "gui/gui.h"
#include "dream.h"
#include "dreamcast.h"

int main (int argc, char *argv[])
{
#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif
  dreamcast_init();

  gnome_init ("dreamon", VERSION, argc, argv);
  video_open();
  dreamcast_register_module( &gtk_gui_module );
  
  emit( main_debug, EMIT_INFO, -1, "DreamOn! ready..." );

  gtk_main ();
  return 0;
}

