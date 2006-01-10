/**
 * $Id: main.c,v 1.10 2006-01-10 13:57:54 nkeynes Exp $
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

#include <unistd.h>
#include <gnome.h>
#include "gui/gui.h"
#include "dream.h"
#include "dreamcast.h"

char *option_list = "a:A:V:p";
char *aica_program = NULL;
gboolean start_immediately = FALSE;

int main (int argc, char *argv[])
{
    int opt;
#ifdef ENABLE_NLS
    bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
    textdomain (PACKAGE);
#endif
  
    while( (opt = getopt( argc, argv, option_list )) != -1 ) {
	switch( opt ) {
	case 'a': /* AICA only mode - argument is an AICA program */
	    aica_program = optarg;
	    break;
	case 'A': /* Audio driver */
	    break;
	case 'V': /* Video driver */
	    break;
	case 'p': /* Start immediately */
	    start_immediately = TRUE;
	}
    }

    if( aica_program == NULL ) {
	dreamcast_init();
	gnome_init ("dreamon", VERSION, argc, argv);
	video_open();
	dreamcast_register_module( &gtk_gui_module );
    } else {
	dreamcast_configure_aica_only();
	mem_load_block( aica_program, 0x00800000, 2048*1024 );
	gnome_init ("dreamon", VERSION, argc, argv);
	dreamcast_register_module( &gtk_gui_module );
	set_disassembly_cpu( main_debug, "ARM7" );
    }

    INFO( "DreamOn! ready..." );
    if( start_immediately )
	dreamcast_run();
    gtk_main ();
    return 0;
}

