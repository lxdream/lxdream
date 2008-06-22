/**
 * $Id$
 *
 * Main program, initializes dreamcast and gui, then passes control off to
 * the main loop. 
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

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <glib/gi18n.h>
#include "lxdream.h"
#include "syscall.h"
#include "mem.h"
#include "dreamcast.h"
#include "display.h"
#include "loader.h"
#include "gui.h"
#include "aica/audio.h"
#include "gdrom/gdrom.h"
#include "maple/maple.h"
#include "sh4/sh4.h"

#define S3M_PLAYER "s3mplay.bin"

char *option_list = "a:m:s:A:V:v:puhbd:c:t:T:xDn";
struct option longopts[1] = { { NULL, 0, 0, 0 } };
char *aica_program = NULL;
char *s3m_file = NULL;
const char *disc_file = NULL;
char *display_driver_name = NULL;
char *audio_driver_name = NULL;
char *trace_regions = NULL;
gboolean start_immediately = FALSE;
gboolean no_start = FALSE;
gboolean headless = FALSE;
gboolean without_bios = FALSE;
gboolean use_xlat = TRUE;
gboolean show_debugger = FALSE;
extern uint32_t sh4_cpu_multiplier;

int main (int argc, char *argv[])
{
    int opt;
    double t;
    gboolean display_ok;
    uint32_t time_secs, time_nanos;

    install_crash_handler();
    gdrom_get_native_devices();
#ifdef ENABLE_NLS
    bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
    textdomain (PACKAGE);
#endif
    display_ok = gui_parse_cmdline(&argc, &argv);

    while( (opt = getopt_long( argc, argv, option_list, longopts, NULL )) != -1 ) {
	switch( opt ) {
	case 'a': /* AICA only mode - argument is an AICA program */
	    aica_program = optarg;
	    break;
	case 'c': /* Config file */
	    lxdream_set_config_filename(optarg);
	    break;
	case 'd': /* Mount disc */
	    disc_file = optarg;
	    break;
	case 'D': /* Launch w/ debugger */
	    show_debugger = TRUE;
	    break;
	case 'm': /* Set SH4 CPU clock multiplier (default 0.5) */
	    t = strtod(optarg, NULL);
	    sh4_cpu_multiplier = (int)(1000.0/t);
	    break;
	case 'n': /* Don't start immediately */
	    no_start = TRUE;
	    start_immediately = FALSE;
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
	    no_start = FALSE;
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
	case 't': /* Time limit + auto quit */
	    t = strtod(optarg, NULL);
	    time_secs = (uint32_t)t;
	    time_nanos = (int)((t - time_secs) * 1000000000);
	    dreamcast_set_run_time( time_secs, time_nanos );
	    dreamcast_set_exit_on_stop( TRUE );
	    break;
	case 'T': /* trace regions */
	    trace_regions = optarg;
	    break;
	case 'v': /* Log verbosity */
	    if( !set_global_log_level(optarg) ) {
		ERROR( "Unrecognized log level '%s'", optarg );
	    }
	    break;
	case 'x': /* Disable translator */
	    use_xlat = FALSE;
	    break;
	}
    }

    lxdream_load_config( );
    gdrom_list_init();
    
    if( aica_program == NULL ) {
	dreamcast_init();
    } else {
	dreamcast_configure_aica_only();
	mem_load_block( aica_program, 0x00800000, 2048*1024 );
	if( s3m_file != NULL ) {
	    mem_load_block( s3m_file, 0x00810000, 2048*1024 - 0x10000 );
	}
    }
    mem_set_trace( trace_regions, TRUE );

    if( without_bios ) {
    	bios_install();
    	dcload_install();
    }

    audio_init_driver( audio_driver_name, 44100, AUDIO_FMT_16ST );
    
    if( headless ) {
	display_set_driver( &display_null_driver );
    } else {
	gui_init(show_debugger);

	display_driver_t display_driver = get_display_driver_by_name(display_driver_name);
	if( display_driver == NULL ) {
	    ERROR( "Video driver '%s' not found, aborting.", display_driver_name );
	    exit(2);
	} else if( display_set_driver( display_driver ) == FALSE ) {
	    ERROR( "Video driver '%s' failed to initialize (could not connect to display?)", 
		   display_driver->name );
	    exit(2);
	}
    }

    maple_reattach_all();
    INFO( "%s! ready...", APP_NAME );

    for( ; optind < argc; optind++ ) {
	gboolean ok = gdrom_mount_image(argv[optind]);
	if( !ok ) {
	    ok = file_load_magic( argv[optind] );
	}
	if( !ok ) {
	    ERROR( "Unrecognized file '%s'", argv[optind] );
	}
	if( !no_start ) {
	    start_immediately = ok;
	}
    }

    if( disc_file != NULL ) {
        gdrom_mount_image( disc_file );
    }

    if( gdrom_get_current_disc() == NULL ) {
        disc_file = lxdream_get_config_value( CONFIG_GDROM );
        if( disc_file != NULL ) {
            gdrom_mount_image( disc_file );
        }
    }

    sh4_set_use_xlat( use_xlat );

    if( headless ) {
        dreamcast_run();
    } else {
        gui_main_loop( start_immediately && dreamcast_can_run() );
    }
    dreamcast_shutdown();
    return 0;
}

