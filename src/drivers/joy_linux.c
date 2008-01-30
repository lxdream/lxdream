/**
 * $Id: joy_linux.c,v 1.12 2007-11-08 11:54:16 nkeynes Exp $
 *
 * Linux joystick input device support
 *
 * Copyright (c) 2008 Nathan Keynes.
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

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

#include <linux/joystick.h>
#include <glib/giochannel.h>
#include <glib.h>

#include "lxdream.h"
#include "display.h"

#define INPUT_PATH "/dev/input"

typedef struct linux_joystick {
    struct input_driver driver;
    const gchar *filename;
    char name[128];
    int fd;
    int button_count, axis_count;
    GIOChannel *channel;

} *linux_joystick_t;

static gboolean linux_joystick_callback( GIOChannel *source, GIOCondition condition, 
					 gpointer data );
static linux_joystick_t linux_joystick_add( const gchar *filename, int fd );
static uint16_t linux_joystick_resolve_keysym( input_driver_t dev, const gchar *str );
static gchar *linux_joystick_keysym_for_keycode( input_driver_t dev, uint16_t keycode );
static void linux_joystick_destroy( input_driver_t joy );
static gboolean linux_joystick_install_watch( const gchar *dir );
static void linux_joystick_uninstall_watch( void );

/**
 * Convert keysym to keycode. Keysyms are either Button%d or Axis%d[+-], with buttons
 * numbered 1 .. button_count, then axes from button_count+1 .. button_count + axis_count*2.
 * The first button is Button1. (no Button0)
 * The first axis is Axis1+, then Axis1-, Axis2+ and so forth.
 */
static uint16_t linux_joystick_resolve_keysym( input_driver_t dev, const gchar *str )
{
    linux_joystick_t joy = (linux_joystick_t)dev;
    if( strncasecmp( str, "Button", 6 ) == 0 ){
	unsigned long button = strtoul( str+6, NULL, 10 );
	if( button > joy->button_count ) {
	    return 0;
	}
	return (uint16_t)button;
    } else if( strncasecmp( str, "Axis", 4 ) == 0 ) {
	char *endptr;
	unsigned long axis = strtoul( str+4, &endptr, 10 );
	if( axis > joy->axis_count || axis == 0 ) {
	    return 0;
	}
	int keycode = ((axis - 1) << 1) + joy->button_count + 1;
	if( *endptr == '-' ) {
	    return keycode + 1;
	} else {
	    return keycode;
	}
    } else {
	return 0;
    }
}

static gchar *linux_joystick_keysym_for_keycode( input_driver_t dev, uint16_t keycode )
{
    linux_joystick_t joy = (linux_joystick_t)dev;
    if( keycode == 0 ) {
	return NULL;
    }
    if( keycode <= joy->button_count ) {
	return g_strdup_printf( "Button%d", keycode );
    }
    if( keycode <= joy->button_count + joy->axis_count*2 ) {
	int axis = keycode - joy->button_count - 1;
	if( (axis & 1) == 0 ) {
	    return g_strdup_printf( "Axis%d+", (axis >> 1)+1 );
	} else {
	    return g_strdup_printf( "Axis%d-", (axis >> 1)+1 );
	}
    }
    return NULL;
}

static void linux_joystick_destroy( input_driver_t dev )
{
    linux_joystick_t joy = (linux_joystick_t)dev;
    g_free( (gchar *)joy->filename );
    g_io_channel_shutdown(joy->channel, FALSE, NULL );
    g_io_channel_unref(joy->channel);
    g_free( joy );
}

/**
 * Callback from the GIOChannel whenever data is available, or an error/disconnect
 * occurs.
 *
 * On data, process all pending events and direct them to the input system.
 * On error, close the channel and delete the device.
 */
static gboolean linux_joystick_callback( GIOChannel *source, GIOCondition condition, 
					 gpointer data )
{
    linux_joystick_t joy = (linux_joystick_t)data;

    if( condition & G_IO_HUP ) {
	INFO( "Joystick '%s' disconnected\n", joy->name );
	input_unregister_device((input_driver_t)joy);
	return FALSE;
    }
    if( condition & G_IO_IN ) {
	struct js_event event;
	while( read( joy->fd, &event, sizeof(event) ) == sizeof(event) ) {
	    if( event.type == JS_EVENT_BUTTON ) {
		int keycode = event.number+1;
		if( event.value == 0 ) {
		    input_event_keyup( (input_driver_t)joy, keycode, 0 );
		} else {
		    input_event_keydown( (input_driver_t)joy, keycode, event.value );
		}
	    } else if( event.type == JS_EVENT_AXIS ) {
		int keycode = (event.number*2) + joy->button_count + 1;
		if( event.value == 0 ) {
		    input_event_keyup( (input_driver_t)joy, keycode, 0 );
		    input_event_keyup( (input_driver_t)joy, keycode+1, 0 );
		} else if( event.value < 0 ) {
		    input_event_keydown( (input_driver_t)joy, keycode+1, -event.value );
		    input_event_keyup( (input_driver_t)joy, keycode, 0 );
		} else {
		    input_event_keydown( (input_driver_t)joy, keycode, event.value );
		    input_event_keyup( (input_driver_t)joy, keycode+1, 0 );
		}
	    }
	}
	return TRUE;
    }
}

/**
 * Create a new joystick device structure given filename and (open) file
 * descriptor. The joystick is automatically added to the watch list.
 * @return The new joystick, or NULL if an error occurred.
 */
linux_joystick_t linux_joystick_new( const gchar *filename, int fd )
{
    linux_joystick_t joy = g_malloc0(sizeof(struct linux_joystick));
    joy->filename = filename;
    joy->fd = fd;
    joy->name[0] = '\0';
    joy->driver.resolve_keysym = linux_joystick_resolve_keysym;
    joy->driver.get_keysym_for_keycode = linux_joystick_keysym_for_keycode;
    joy->driver.destroy = linux_joystick_destroy;

    char *p = strrchr(filename, '/');
    if( p == NULL ) {
	joy->driver.id = filename;
    } else {
	joy->driver.id = p+1;
    }

    if( ioctl( fd, JSIOCGNAME(128), joy->name ) == -1 ||
	ioctl( fd, JSIOCGAXES, &joy->axis_count ) == -1 || 
	ioctl( fd, JSIOCGBUTTONS, &joy->button_count ) == -1 ) {
	ERROR( "Error reading joystick data from %s (%s)\n", filename, strerror(errno) );
	g_free(joy);
	return NULL;
    }

    joy->channel = g_io_channel_unix_new(fd);
    g_io_add_watch( joy->channel, G_IO_IN|G_IO_ERR|G_IO_HUP, linux_joystick_callback, joy );
    return joy;
}

int linux_joystick_init()
{
    linux_joystick_install_watch(INPUT_PATH);
    linux_joystick_scan();
}

int linux_joystick_scan()
{
    int joysticks = 0;
    struct dirent *ent;
    DIR *dir = opendir(INPUT_PATH);

    if( dir == NULL ) {
	return 0;
    }
    
    while( (ent = readdir(dir)) != NULL ) {
	if( ent->d_name[0] == 'j' && ent->d_name[1] == 's' &&
	    isdigit(ent->d_name[2]) && !input_has_device(ent->d_name) ) {
	    gchar *name = g_strdup_printf( "%s/%s", INPUT_PATH, ent->d_name );
	    int fd = open(name, O_RDONLY|O_NONBLOCK);
	    if( fd == -1 ) {
		g_free( name );
	    } else {
		linux_joystick_t joy = linux_joystick_new( name, fd );
		input_register_device( (input_driver_t)joy, (joy->axis_count*2) + joy->button_count );
		INFO( "Attached joystick %s named '%s', (%d buttons, %d axes)", 
		      joy->driver.id, joy->name, joy->button_count, joy->axis_count );
		joysticks++;
	    }
	}
    }

    closedir(dir);
    return joysticks;
}

void linux_joystick_shutdown(void)
{
    linux_joystick_uninstall_watch();
}

/*************************** dnotify support **********************/

static volatile gboolean need_input_rescan = FALSE;
static int watch_dir_fd;

static gboolean gtk_loop_check_input(gpointer data)
{
    if( need_input_rescan ) {
	int js = linux_joystick_scan();
	if( js > 0 ) {
	    maple_reattach_all();
	}
    }
    return TRUE;
}

static void dnotify_handler(int sig )
{
    need_input_rescan = TRUE;
}

static gboolean linux_joystick_install_watch( const gchar *dir )
{
    int fd = open( dir, O_RDONLY|O_NONBLOCK );
    if( fd == -1 ) {
	return FALSE;
    }
    
    signal( SIGRTMIN+1, dnotify_handler );
    fcntl(fd, F_SETSIG, SIGRTMIN + 1);
    if( fcntl(fd, F_NOTIFY, DN_CREATE|DN_MULTISHOT) == -1 ) {
	close(fd);
	return FALSE;
    }
    watch_dir_fd = fd;
    g_timeout_add( 500, gtk_loop_check_input, NULL );
}

static void linux_joystick_uninstall_watch(void)
{
    signal( SIGRTMIN+1, SIG_IGN );
    close( watch_dir_fd );
}
