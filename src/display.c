/**
 * $Id$
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
#include "pvr2/pvr2.h"

display_driver_t display_driver_list[] = { 
#ifdef HAVE_GTK
					   &display_gtk_driver,
#endif
					   &display_null_driver,
					   NULL };

typedef struct keymap_entry {
    uint16_t keycode;
    input_key_callback_t callback;
    void *data;
    uint32_t value;
    struct keymap_entry *next; // allow chaining
} *keymap_entry_t;

typedef struct mouse_entry {
    gboolean relative;
    input_mouse_callback_t callback;
    void *data;
    struct mouse_entry *next;
} *mouse_entry_t;

/**
 * Colour format information
 */
struct colour_format colour_formats[] = {
    { GL_UNSIGNED_SHORT_1_5_5_5_REV, GL_BGRA, GL_RGB5_A1, 2 },
    { GL_UNSIGNED_SHORT_5_6_5, GL_RGB, GL_RGB5, 2 },
    { GL_UNSIGNED_SHORT_4_4_4_4_REV, GL_BGRA, GL_RGBA4, 2 },
    { GL_UNSIGNED_BYTE, GL_BGRA, GL_RGBA8, 4 }, /* YUV decoded to ARGB8888 */
    { GL_UNSIGNED_BYTE, GL_BGR, GL_RGB, 3 },
    { GL_UNSIGNED_BYTE, GL_BGRA, GL_RGBA8, 4 },
    { GL_UNSIGNED_BYTE, GL_BGRA, GL_RGBA8, 4 }, /* Index4 decoded */
    { GL_UNSIGNED_BYTE, GL_BGRA, GL_RGBA8, 4 }, /* Index8 decoded */
    { GL_UNSIGNED_BYTE, GL_BGRA, GL_RGBA8, 4 },
    { GL_UNSIGNED_BYTE, GL_RGB, GL_RGB, 3 },
    
};

/**
 * FIXME: make this more memory efficient
 */
struct keymap_entry *keymap[65536];
struct keymap_entry *keyhooks = NULL;
struct mouse_entry *mousehooks = NULL;

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

static void input_delete_key( uint16_t keycode, input_key_callback_t callback, void *data,
			      uint32_t value )
{
    struct keymap_entry *key = keymap[keycode];
    if( key != NULL && key->callback == callback && key->data == data && key->value == value ) {
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
    if( display_driver == NULL || keysym == NULL || display_driver->resolve_keysym == NULL )
	return FALSE; /* No display driver */
    gchar **strv = g_strsplit(keysym, ",", 16);
    gchar **s = strv;
    while( *s != NULL ) {
	uint16_t keycode = display_driver->resolve_keysym(g_strstrip(*s));
	if( keycode == 0 )
	    return FALSE; /* Invalid keysym */
	
	struct keymap_entry *key = input_create_key( keycode );
	key->callback = callback;
	key->data = data;
	key->value = value;
	s++;
    }
    g_strfreev(strv);
    return TRUE;
}

void input_unregister_key( const gchar *keysym, input_key_callback_t callback,
			   void *data, uint32_t value )
{
    if( display_driver == NULL || keysym == NULL || display_driver->resolve_keysym == NULL )
	return;
    uint16_t keycode = display_driver->resolve_keysym(keysym);
    if( keycode == 0 )
	return;
    input_delete_key( keycode, callback, data, value );
}

gboolean input_register_hook( input_key_callback_t callback,
			      void *data )
{
    keymap_entry_t key = malloc( sizeof( struct keymap_entry ) );
    assert( key != NULL );
    key->callback = callback;
    key->data = data;
    key->next = keyhooks;
    keyhooks = key;
    return TRUE;
}

void input_unregister_hook( input_key_callback_t callback,
			    void *data )
{
    keymap_entry_t key = keyhooks;
    if( key != NULL ) {
	keymap_entry_t next = key->next;
	if( key->callback == callback && key->data == data ) {
	    free(key);
	    keyhooks = next;
	    return;
	}
	while( next != NULL ) {
	    if( next->callback == callback && next->data == data ) {
		key->next = next->next;
		free(next);
	    }
	}
    }
}

gboolean input_register_mouse_hook( gboolean relative, input_mouse_callback_t callback,
				void *data )
{
    mouse_entry_t ent = malloc( sizeof( struct mouse_entry ) );
    assert( ent != NULL );
    ent->callback = callback;
    ent->data = data;
    ent->next = mousehooks;
    mousehooks = ent;
    return TRUE;
}    

void input_unregister_mouse_hook( input_mouse_callback_t callback, void *data )
{
    mouse_entry_t ent = mousehooks;
    if( ent != NULL ) {
	mouse_entry_t next = ent->next;
	if( ent->callback == callback && ent->data == data ) {
	    free(ent);
	    mousehooks = next;
	    return;
	}
	while( next != NULL ) {
	    if( next->callback == callback && next->data == data ) {
		ent->next = next->next;
		free(next);
	    }
	}
    }
}

void input_event_mouse( uint32_t buttons, int32_t x, int32_t y )
{
    mouse_entry_t ent = mousehooks;
    while( ent != NULL ) {
	ent->callback(ent->data, buttons, x, y);
	ent = ent->next;
    }
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
    key = keyhooks;
    while( key != NULL ) {
	key->callback( key->data, keycode, TRUE );
	key = key->next;
    }
}

void input_event_keyup( uint16_t keycode )
{
    struct keymap_entry *key = input_get_key(keycode);
    if( key != NULL ) {
	key->callback( key->data, key->value, FALSE );
    }
    key = keyhooks;
    while( key != NULL ) {
	key->callback( key->data, keycode, FALSE );
	key = key->next;
    }
}

uint16_t input_keycode_to_dckeysym( uint16_t keycode )
{
    return display_driver->convert_to_dckeysym(keycode);
}

display_driver_t get_display_driver_by_name( const char *name )
{
    int i;
    if( name == NULL ) {
	return display_driver_list[0];
    }
    for( i=0; display_driver_list[i] != NULL; i++ ) {
	if( strcasecmp( display_driver_list[i]->name, name ) == 0 ) {
	    return display_driver_list[i];
	}
    }

    return NULL;
}


gboolean display_set_driver( display_driver_t driver )
{
    gboolean rv = TRUE;
    if( display_driver != NULL && display_driver->shutdown_driver != NULL ) 
	display_driver->shutdown_driver();

    display_driver = driver;
    if( driver->init_driver != NULL )
	rv = driver->init_driver();
    if( !rv ) {
	display_driver = NULL;
    }
    return rv;
}
