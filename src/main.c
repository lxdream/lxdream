/**
 * $Id: main.c,v 1.13 2006-02-05 04:05:27 nkeynes Exp $
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
#include <getopt.h>
#include <gnome.h>
#include "gui/gui.h"
#include "dream.h"
#include "bios.h"
#include "dreamcast.h"

#define S3M_PLAYER "s3mplay.bin"

char *option_list = "a:s:A:V:phb";
struct option longopts[1] = { { NULL, 0, 0, 0 } };
char *aica_program = NULL;
char *s3m_file = NULL;
gboolean start_immediately = FALSE;
gboolean headless = FALSE;
gboolean without_bios = FALSE;

int main (int argc, char *argv[])
{
    int opt;
#ifdef ENABLE_NLS
    bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
    textdomain (PACKAGE);
#endif
  
    while( (opt = getopt_long( argc, argv, option_list, longopts, NULL )) != -1 ) {
	switch( opt ) {
	case 'a': /* AICA only mode - argument is an AICA program */
	    aica_program = optarg;
	    break;
	case 's': /* AICA-only w/ S3M player */
	    aica_program = S3M_PLAYER;
	    s3m_file = optarg;
	    break;
	case 'A': /* Audio driver */
	    break;
	case 'V': /* Video driver */
	    break;
	case 'p': /* Start immediately */
	    start_immediately = TRUE;
    	    break;
    	case 'b': /* No BIOS */
    	    without_bios = TRUE;
    	    break;
        case 'h': /* Headless */
            headless = TRUE;
            break;
	}
    }

    if( aica_program == NULL ) {
	if( !headless ) {
	    gnome_init ("dreamon", VERSION, argc, argv);
	    dreamcast_init();
	    dreamcast_register_module( &gtk_gui_module );
	} else {
	    dreamcast_init();
	}
    } else {
	dreamcast_configure_aica_only();
	mem_load_block( aica_program, 0x00800000, 2048*1024 );
	if( s3m_file != NULL ) {
	    mem_load_block( s3m_file, 0x00810000, 2048*1024 - 0x10000 );
	}
	if( !headless ) {
	    gnome_init ("dreamon", VERSION, argc, argv);
	    dreamcast_register_module( &gtk_gui_module );
	    set_disassembly_cpu( main_debug, "ARM7" );
	}
    }

    if( without_bios ) {
    	bios_install();
    }
    INFO( "DreamOn! ready..." );
    if( optind < argc ) {
	file_load_magic( argv[optind] );
    }

    if( start_immediately )
	dreamcast_run();
    if( !headless ) {
        gtk_main ();
    }
    return 0;
}

