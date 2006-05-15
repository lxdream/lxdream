/**
 * $Id: display.c,v 1.1 2006-05-15 08:28:48 nkeynes Exp $
 *
 * Generic support for keyboard and other input sources. The active display
 * driver is expected to deliver events here, where they're translated and
 * passed to the appropriate dreamcast controllers (if any).
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

#include <stdint.h>
#include <assert.h>
#include "dream.h"
#include "display.h"

typedef struct keymap_entry {
    uint16_t keycode;
    input_key_callback_t callback;
    void *data;
    uint32_t value;
} *keymap_entry_t;

/**
 * FIXME: make this more memory efficient
 */
struct keymap_entry *keymap[65536];


static struct keymap_entry *input_create_key( uint16_t keycode )
{
    struct keymap_entry *key = keymap[ keycode ];
    if( key == NULL ) {
	key = malloc( sizeof( struct keymap_entry ) );
	assert( key != NULL );
	keymap[ keycode ] = key;
	key->keycode = keycode;
    }
    return key;
}

static void input_delete_key( uint16_t keycode )
{
    struct keymap_entry *key = keymap[keycode];
    if( key != NULL ) {
	free( key );
	keymap[keycode] = NULL;
    }
}

static struct keymap_entry *input_get_key( uint16_t keycode )
{
    return keymap[ keycode ];
}

gboolean input_register_key( const gchar *keysym, input_key_callback_t callback,
			     void *data, uint32_t value )
{
    if( display_driver == NULL || keysym == NULL )
	return FALSE; /* No display driver */
    uint16_t keycode = display_driver->resolve_keysym(keysym);
    if( keycode == 0 )
	return FALSE; /* Invalid keysym */

    struct keymap_entry *key = input_create_key( keycode );
    key->callback = callback;
    key->data = data;
    key->value = value;
    INFO( "Registered key '%s' (%4x)", keysym, keycode );
    return TRUE;
}

void input_unregister_key( const gchar *keysym )
{
    if( display_driver == NULL || keysym == NULL )
	return;
    uint16_t keycode = display_driver->resolve_keysym(keysym);
    if( keycode == 0 )
	return;
    input_delete_key( keycode );
}
    

gboolean input_is_key_valid( const gchar *keysym )
{
    if( display_driver == NULL )
	return FALSE; /* No display driver */
    return display_driver->resolve_keysym(keysym) != 0;
}

gboolean input_is_key_registered( const gchar *keysym )
{
    if( display_driver == NULL )
	return FALSE;
    uint16_t keycode = display_driver->resolve_keysym(keysym);
    if( keycode == 0 )
	return FALSE;
    return input_get_key( keycode ) != NULL;
}

void input_event_keydown( uint16_t keycode )
{
    struct keymap_entry *key = input_get_key(keycode);
    if( key != NULL ) {
	key->callback( key->data, key->value, TRUE );
    }	
}

void input_event_keyup( uint16_t keycode )
{
    struct keymap_entry *key = input_get_key(keycode);
    if( key != NULL ) {
	key->callback( key->data, key->value, FALSE );
    }
}



void display_set_driver( display_driver_t driver )
{
    if( display_driver != NULL && display_driver->shutdown_driver != NULL )
	display_driver->shutdown_driver();

    display_driver = driver;
    if( driver->init_driver != NULL )
	driver->init_driver();
    driver->set_display_format( 640, 480, COLFMT_ARGB8888 );
    driver->set_render_format( 640, 480, COLFMT_ARGB8888, FALSE );
    texcache_gl_init();
}
