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
#include <libisofs.h>
#include "lxdream.h"
#include "lxpaths.h"
#include "gettext.h"
#include "dream.h"
#include "dreamcast.h"
#include "display.h"
#include "gui.h"
#include "gdlist.h"
#include "hotkeys.h"
#include "loader.h"
#include "mem.h"
#include "plugin.h"
#include "serial.h"
#include "syscall.h"
#include "aica/audio.h"
#include "aica/armdasm.h"
#include "gdrom/gdrom.h"
#include "maple/maple.h"
#include "pvr2/glutil.h"
#include "sh4/sh4.h"
#include "vmu/vmulist.h"

#define GL_INFO_OPT 1

char *option_list = "a:A:bc:e:dfg:G:hHl:m:npt:T:uvV:xX?";
struct option longopts[] = {
        { "aica", required_argument, NULL, 'a' },
        { "audio", required_argument, NULL, 'A' },
        { "biosless", no_argument, NULL, 'b' },
        { "config", required_argument, NULL, 'c' },
        { "debugger", no_argument, NULL, 'd' },
        { "execute", required_argument, NULL, 'e' },
        { "fullscreen", no_argument, NULL, 'f' },
        { "gdb-sh4", required_argument, NULL, 'g' },  
        { "gdb-arm", required_argument, NULL, 'G' },
        { "gl-info", no_argument, NULL, GL_INFO_OPT },
        { "help", no_argument, NULL, 'h' },
        { "headless", no_argument, NULL, 'H' },
        { "log", required_argument, NULL,'l' }, 
        { "multiplier", required_argument, NULL, 'm' },
        { "run-time", required_argument, NULL, 't' },
        { "shadow", no_argument, NULL, 'X' },
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
sh4core_t sh4_core = SH4_TRANSLATE;
gboolean show_debugger = FALSE;
gboolean show_fullscreen = FALSE;
gboolean use_bootrom = TRUE;
extern uint32_t sh4_cpu_multiplier;

static void print_version()
{
    printf( "lxdream %s\n", lxdream_full_version );
}

static void print_usage()
{
    print_version();
    printf( "Usage: lxdream %s [options] [disc-file] [save-state]\n\n", lxdream_full_version );

    printf( "Options:\n" );
    printf( "   -a, --aica=PROGFILE    %s\n", _("Run the AICA SPU only, with the supplied program") );
    printf( "   -A, --audio=DRIVER     %s\n", _("Use the specified audio driver (? to list)") );
    printf( "   -b, --biosless         %s\n", _("Run without the BIOS boot rom even if available") );
    printf( "   -c, --config=CONFFILE  %s\n", _("Load configuration from CONFFILE") );
    printf( "   -e, --execute=PROGRAM  %s\n", _("Load and execute the given SH4 program") );
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
    printf( "   -X                     %s\n", _("Run both SH4 interpreter and translator") );
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
    gboolean display_ok, have_disc = FALSE, have_save = FALSE, have_exec = FALSE;
    gboolean print_glinfo = FALSE;
    uint32_t time_secs, time_nanos;
    const char *exec_name = NULL;

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
        case 'b': /* No Boot rom */
            use_bootrom = FALSE;
        case 'c': /* Config file */
            lxdream_set_config_filename(optarg);
            break;
        case 'd': /* Launch w/ debugger */
            show_debugger = TRUE;
            break;
        case 'e':
            exec_name = optarg;
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
            sh4_core = SH4_INTERPRET;
            break;
        case 'X': /* Shadow translator */
            sh4_core = SH4_SHADOW;
            break;
        case GL_INFO_OPT:
            print_glinfo = TRUE;
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

    if( print_glinfo ) {
        gui_init(FALSE, FALSE);
        display_driver_t display_driver = get_display_driver_by_name(display_driver_name);
        if( display_driver == NULL ) {
            ERROR( "Video driver '%s' not found, aborting.", display_driver_name );
            exit(2);
        } else if( display_set_driver( display_driver ) == FALSE ) {
            ERROR( "Video driver '%s' failed to initialize (could not connect to display?)",
                    display_driver->name );
            exit(2);
        } else if( display_driver->capabilities.has_gl == FALSE ) {
            ERROR( "Video driver '%s' has no GL capabilities.", display_driver_name );
            exit(2);
        }
        glPrintInfo(stdout);
        exit(0);

    }


    iso_init();
    gdrom_list_init();
    vmulist_init();

    if( aica_program == NULL ) {
        dreamcast_init(use_bootrom);
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
        ERROR err;
        lxdream_file_type_t type = file_identify(argv[optind], -1, &err);
        if( type == FILE_SAVE_STATE ) {
            if( have_save ) {
                ERROR( "Multiple save states given on command-line, ignoring %s", argv[optind] );
            } else {
                have_save = dreamcast_load_state(argv[optind]);
                if( !have_save )
                    no_start = TRUE;
            }
        } else {
            if( have_disc ) {
                ERROR( "Multiple GD-ROM discs given on command-line, ignoring %s", argv[optind] );
            } else {
                have_disc = gdrom_mount_image(argv[optind], &err);
                if( !have_disc ) {
                    ERROR( err.msg );
                    no_start = TRUE;
                }
            }
        }
    }

    if( exec_name != NULL ) {
        ERROR err;
        if( have_save ) {
            ERROR( "Both a save state and an executable were specified, ignoring %s", exec_name );
        } else {
            have_exec = file_load_exec( exec_name, &err );
            if( !have_exec ) {
                ERROR( err.msg );
                no_start = TRUE;
            }
        }
    }

    if( !no_start && (have_exec || have_disc || have_save) ) {
        start_immediately = TRUE;
    }

    if( gdrom_get_current_disc() == NULL ) {
        ERROR err;
        gchar *disc_file = lxdream_get_global_config_path_value( CONFIG_GDROM );
        if( disc_file != NULL ) {
            gboolean ok = gdrom_mount_image( disc_file, &err );
            g_free(disc_file);
            if( !ok ) {
                WARN( err.msg );
            }
        }
    }

    sh4_set_core( sh4_core );

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

