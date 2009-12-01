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
#include "lxdream.h"
#include "lxpaths.h"
#include "gettext.h"
#include "mem.h"
#include "dreamcast.h"
#include "dream.h"
#include "display.h"
#include "gui.h"
#include "gdlist.h"
#include "syscall.h"
#include "loader.h"
#include "aica/audio.h"
#include "gdrom/gdrom.h"
#include "maple/maple.h"
#include "sh4/sh4.h"
#include "aica/armdasm.h"
#include "vmu/vmulist.h"
#include "serial.h"
#include "hotkeys.h"
#include "plugin.h"

char *option_list = "a:A:c:dfg:G:hHl:m:npt:T:uvV:x?";
struct option longopts[] = {
        { "aica", required_argument, NULL, 'a' },
        { "audio", required_argument, NULL, 'A' },
        { "config", required_argument, NULL, 'c' },
        { "debugger", no_argument, NULL, 'D' },
        { "fullscreen", no_argument, NULL, 'f' },
        { "gdb-sh4", required_argument, NULL, 'g' },  
        { "gdb-arm", required_argument, NULL, 'G' },  
        { "help", no_argument, NULL, 'h' },
        { "headless", no_argument, NULL, 'H' },
        { "log", required_argument, NULL,'l' }, 
        { "multiplier", required_argument, NULL, 'm' },
        { "run-time", required_argument, NULL, 't' },
        { "trace", required_argument, NULL, 'T' },
        { "unsafe", no_argument, NULL, 'u' },
        { "video", no_argument, NULL, 'V' },
        { "version", no_argument, NULL, 'v' }, 
        { NULL, 0, 0, 0 } };
char *aica_program = NULL;
char *display_driver_name = NULL;
char *audio_driver_name = NULL;
char *trace_regions = NULL;
char *sh4_gdb_port = NULL;
char *arm_gdb_port = NULL;
gboolean start_immediately = FALSE;
gboolean no_start = FALSE;
gboolean headless = FALSE;
gboolean use_xlat = TRUE;
gboolean show_debugger = FALSE;
gboolean show_fullscreen = FALSE;
extern uint32_t sh4_cpu_multiplier;

static void print_version()
{
    printf( "lxdream %s\n", lxdream_full_version );
}

static void print_usage()
{
    print_version();
    printf( "Usage: lxdream %s [options] [disc-file] [program-file]\n\n", lxdream_full_version );

    printf( "Options:\n" );
    printf( "   -a, --aica=PROGFILE    %s\n", _("Run the AICA SPU only, with the supplied program") );
    printf( "   -A, --audio=DRIVER     %s\n", _("Use the specified audio driver (? to list)") );
    printf( "   -c, --config=CONFFILE  %s\n", _("Load configuration from CONFFILE") );
    printf( "   -d, --debugger         %s\n", _("Start in debugger mode") );
    printf( "   -f, --fullscreen       %s\n", _("Start in fullscreen mode") );
    printf( "   -g, --gdb-sh4=PORT     %s\n", _("Start GDB remote server on PORT for SH4") );
    printf( "   -G, --gdb-arm=PORT     %s\n", _("Start GDB remote server on PORT for ARM") );
    printf( "   -h, --help             %s\n", _("Display this usage information") );
    printf( "   -H, --headless         %s\n", _("Run in headless (no video) mode") );
    printf( "   -l, --log=LEVEL        %s\n", _("Set the output log level") );
    printf( "   -m, --multiplier=SCALE %s\n", _("Set the SH4 multiplier (1.0 = fullspeed)") );
    printf( "   -n                     %s\n", _("Don't start running immediately") );
    printf( "   -p                     %s\n", _("Start running immediately on startup") );
    printf( "   -t, --run-time=SECONDS %s\n", _("Run for the specified number of seconds") );
    printf( "   -T, --trace=REGIONS    %s\n", _("Output trace information for the named regions") );
    printf( "   -u, --unsafe           %s\n", _("Allow unsafe dcload syscalls") );
    printf( "   -v, --version          %s\n", _("Print the lxdream version string") );
    printf( "   -V, --video=DRIVER     %s\n", _("Use the specified video driver (? to list)") );
    printf( "   -x                     %s\n", _("Disable the SH4 translator") );
}

static void bind_gettext_domain()
{
#ifdef ENABLE_NLS
    bindtextdomain( PACKAGE, get_locale_path() );
    textdomain(PACKAGE);
#endif
}

int main (int argc, char *argv[])
{
    int opt;
    double t;
    gboolean display_ok;
    uint32_t time_secs, time_nanos;

    install_crash_handler();
    bind_gettext_domain();
    display_ok = gui_parse_cmdline(&argc, &argv);

    while( (opt = getopt_long( argc, argv, option_list, longopts, NULL )) != -1 ) {
        switch( opt ) {
        case 'a': /* AICA only mode - argument is an AICA program */
            aica_program = optarg;
            break;
        case 'A': /* Audio driver */
            audio_driver_name = optarg;
            break;
        case 'c': /* Config file */
            lxdream_set_config_filename(optarg);
            break;
        case 'd': /* Launch w/ debugger */
            show_debugger = TRUE;
            break;
        case 'f':
            show_fullscreen = TRUE;
            break;
        case 'g':
            sh4_gdb_port = optarg;
            break;
        case 'G':
            arm_gdb_port = optarg;
            break;
        case 'h': /* help */
        case '?':
            print_usage();
            exit(0);
            break;
        case 'H': /* Headless - shorthand for -V null */
            display_driver_name = "null";
            break;
        case 'l': /* Log verbosity */
            if( !set_global_log_level(optarg) ) {
                ERROR( "Unrecognized log level '%s'", optarg );
            }
            break;
        case 'm': /* Set SH4 CPU clock multiplier (default 0.5) */
            t = strtod(optarg, NULL);
            sh4_cpu_multiplier = (int)(1000.0/t);
            break;
        case 'n': /* Don't start immediately */
            no_start = TRUE;
            start_immediately = FALSE;
            break;
        case 'p': /* Start immediately */
            start_immediately = TRUE;
            no_start = FALSE;
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
            set_global_log_level("trace");
            break;
        case 'u': /* Allow unsafe dcload syscalls */
            dcload_set_allow_unsafe(TRUE);
            break;
        case 'v': 
            print_version();
            exit(0);
            break;
        case 'V': /* Video driver */
            display_driver_name = optarg;
            break;
        case 'x': /* Disable translator */
            use_xlat = FALSE;
            break;
        }
    }

#ifdef ENABLE_SHARED
    plugin_init();
#endif

    lxdream_make_config_dir( );
    lxdream_load_config( );

    if( audio_driver_name != NULL && strcmp(audio_driver_name, "?") == 0 ) {
        print_version();
        print_audio_drivers(stdout);
        exit(0);
    }

    if( display_driver_name != NULL && strcmp(display_driver_name,"?") == 0 ) {
        print_version();
        print_display_drivers(stdout);
        exit(0);
    }

    gdrom_list_init();
    vmulist_init();

    if( aica_program == NULL ) {
        dreamcast_init();
    } else {
        dreamcast_configure_aica_only();
        mem_load_block( aica_program, 0x00800000, 2048*1024 );
    }
    mem_set_trace( trace_regions, TRUE );

    audio_init_driver( audio_driver_name );

    headless = display_driver_name != NULL && strcasecmp( display_driver_name, "null" ) == 0;
    if( headless ) {
        display_set_driver( &display_null_driver );
    } else {
        gui_init(show_debugger, show_fullscreen);

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
    
    hotkeys_init();
    serial_init();

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

    if( gdrom_get_current_disc() == NULL ) {
        gchar *disc_file = lxdream_get_global_config_path_value( CONFIG_GDROM );
        if( disc_file != NULL ) {
            gdrom_mount_image( disc_file );
            g_free(disc_file);
        }
    }

    sh4_translate_set_enabled( use_xlat );

    /* If requested, start the gdb server immediately before we go into the main
     * loop.
     */
    if( sh4_gdb_port != NULL ) {
        gdb_init_server( NULL, strtol(sh4_gdb_port,NULL,0), &sh4_cpu_desc, TRUE );
    }
    if( arm_gdb_port != NULL ) {
        gdb_init_server( NULL, strtol(arm_gdb_port,NULL,0), &arm_cpu_desc, TRUE );
    }
    
    if( headless ) {
        dreamcast_run();
    } else {
        gui_main_loop( start_immediately && dreamcast_can_run() );
    }
    dreamcast_shutdown();
    return 0;
}

