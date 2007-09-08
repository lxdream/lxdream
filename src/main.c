/**
 * $Id: main.c,v 1.22 2007-09-08 04:38:38 nkeynes Exp $
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
#include "syscall.h"
#include "dreamcast.h"
#include "aica/audio.h"
#include "display.h"
#include "maple/maple.h"

#define S3M_PLAYER "s3mplay.bin"

char *option_list = "a:s:A:V:puhbd:c:t:";
struct option longopts[1] = { { NULL, 0, 0, 0 } };
char *aica_program = NULL;
char *s3m_file = NULL;
char *disc_file = NULL;
char *display_driver_name = "gtk";
char *audio_driver_name = "esd";
char *config_file = DEFAULT_CONFIG_FILENAME;
gboolean start_immediately = FALSE;
gboolean headless = FALSE;
gboolean without_bios = FALSE;
uint32_t time_secs = 0;
uint32_t time_nanos = 0;

audio_driver_t audio_driver_list[] = { &audio_null_driver,
				       &audio_esd_driver,
				       NULL };

display_driver_t display_driver_list[] = { &display_null_driver,
					   &display_gtk_driver,
					   NULL };

int main (int argc, char *argv[])
{
    int opt, i;
    double t;
#ifdef ENABLE_NLS
    bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
    textdomain (PACKAGE);
#endif
  
    while( (opt = getopt_long( argc, argv, option_list, longopts, NULL )) != -1 ) {
	switch( opt ) {
	case 'a': /* AICA only mode - argument is an AICA program */
	    aica_program = optarg;
	    break;
	case 'c': /* Config file */
	    config_file = optarg;
	    break;
	case 'd': /* Mount disc */
	    disc_file = optarg;
	    break;
	case 's': /* AICA-only w/ S3M player */
	    aica_program = S3M_PLAYER;
	    s3m_file = optarg;
	    break;
	case 'A': /* Audio driver */
	    audio_driver_name = optarg;
	    break;
	case 'V': /* Video driver */
	    display_driver_name = optarg;
	    break;
	case 'p': /* Start immediately */
	    start_immediately = TRUE;
    	    break;
	case 'u': /* Allow unsafe dcload syscalls */
	    dcload_set_allow_unsafe(TRUE);
	    break;
    	case 'b': /* No BIOS */
    	    without_bios = TRUE;
    	    break;
        case 'h': /* Headless */
            headless = TRUE;
            break;
	case 't': /* Time limit */
	    t = strtod(optarg, NULL);
	    time_secs = (uint32_t)t;
	    time_nanos = (int)((t - time_secs) * 1000000000);
	}
    }

    dreamcast_load_config( config_file );

    if( aica_program == NULL ) {
	if( !headless ) {
	    gnome_init ("lxdream", VERSION, argc, argv);
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
	    gnome_init ("lxdream", VERSION, argc, argv);
	    dreamcast_register_module( &gtk_gui_module );
	    set_disassembly_cpu( main_debug, "ARM7" );
	}
    }

    if( without_bios ) {
    	bios_install();
	dcload_install();
    }

    for( i=0; audio_driver_list[i] != NULL; i++ ) {
	if( strcasecmp( audio_driver_list[i]->name, audio_driver_name ) == 0 ) {
	    if( audio_set_driver( audio_driver_list[i], 44100, AUDIO_FMT_16ST ) == FALSE ) {
		audio_set_driver( &audio_null_driver, 44100, AUDIO_FMT_16ST );
	    }
	    break;
	}

    }
    if( audio_driver_list[i] == NULL ) {
	ERROR( "Audio driver '%s' not found, using null driver", audio_driver_name );
	audio_set_driver( &audio_null_driver, 44100, AUDIO_FMT_16ST );
    }

    if( headless ) {
	display_set_driver( &display_null_driver );
    } else {
	gboolean initialized = FALSE;
	for( i=0; display_driver_list[i] != NULL; i++ ) {
	    if( strcasecmp( display_driver_list[i]->name, display_driver_name ) == 0 ) {
		initialized = display_set_driver( display_driver_list[i] );
		break;
	    }
	}
	if( !initialized ) {
	    if( display_driver_list[i] == NULL ) {
		ERROR( "Video driver '%s' not found, using null driver", display_driver_name );
	    } else {
		ERROR( "Video driver '%s' failed to initialize, falling back to null driver", display_driver_name );
	    }
	    display_set_driver( &display_null_driver );
	}
    }

    maple_reattach_all();
    INFO( "%s! ready...", APP_NAME );
    if( optind < argc ) {
	file_load_magic( argv[optind] );
    }

    if( disc_file != NULL ) {
	gdrom_mount_image( disc_file );
    }

    if( start_immediately ) {
	if( time_nanos != 0 || time_secs != 0 ) {
	    dreamcast_run_for(time_secs, time_nanos);
	    return 0;
	} else {
	    dreamcast_run();
	}
    }
    if( !headless ) {
        gtk_main ();
    }
    return 0;
}

