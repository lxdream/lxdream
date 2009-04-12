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
#else
#ifdef HAVE_COCOA
        &display_osx_driver,
#endif
#endif					   
        &display_null_driver,
        NULL };

/* Some explanation:
 *   The system has at least one "root" device representing the main display 
 * (which may be the null display). This device is part of the display_driver
 * and generates events with no input_driver. The root device has no id
 * as such (for the purposes of event names)
 *
 *   The system may also have one or more auxilliary devices which each have
 * an input_driver and an id (eg "JS0"). So the keysym "Return" is (de)coded by
 * the root device, and the keysym "JS0: Button0" is (de)coded by the JS0 input
 * device as "Button0".
 *
 *   For the moment, mice are handled specially, as they behave a little
 * differently from other devices (although this will probably change in the 
 * future.
 */


typedef struct keymap_entry {
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

typedef struct input_driver_entry {
    input_driver_t driver;
    uint16_t entry_count;
    struct keymap_entry *keymap[0];
} *input_driver_entry_t;

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
static struct keymap_entry *root_keymap[65535];
static struct keymap_entry *keyhooks = NULL;
static gboolean display_focused = TRUE;
static GList *input_drivers= NULL;
static display_keysym_callback_t display_keysym_hook = NULL;
void *display_keysym_hook_data;

gboolean input_register_device( input_driver_t driver, uint16_t max_keycode )
{
    GList *ptr;
    for( ptr = input_drivers; ptr != NULL; ptr = g_list_next(ptr) ) {
        input_driver_entry_t entry = (input_driver_entry_t)ptr->data;
        if( strcasecmp( entry->driver->id, driver->id ) == 0 ) {
            return FALSE;
        }
    }

    input_driver_entry_t entry = g_malloc0( sizeof(struct input_driver_entry) + (sizeof(keymap_entry_t) * max_keycode) );
    entry->driver = driver;
    entry->entry_count = max_keycode;
    input_drivers = g_list_append( input_drivers, entry );
    return TRUE;
}

gboolean input_has_device( const gchar *id )
{
    GList *ptr;
    for( ptr = input_drivers; ptr != NULL; ptr = g_list_next(ptr) ) {
        input_driver_entry_t entry = (input_driver_entry_t)ptr->data;
        if( strcasecmp(entry->driver->id, id) == 0 ) {
            return TRUE;
        }
    }
    return FALSE;
}


void input_unregister_device( input_driver_t driver )
{
    GList *ptr;
    for( ptr = input_drivers; ptr != NULL; ptr = g_list_next(ptr) ) {
        input_driver_entry_t entry = (input_driver_entry_t)ptr->data;
        if( entry->driver == driver ) {
            if( driver->destroy != NULL ) {
                driver->destroy(driver);
            }
            input_drivers = g_list_remove(input_drivers, (gpointer)entry);
            g_free( entry );
            return;
        }
    }
}

/**
 * Resolve the keysym and return a pointer to the keymap entry pointer
 * @return keymap pointer or NULL if the key was unresolved
 */
static struct keymap_entry **input_entry_from_keysym( const gchar *keysym )
{
    if( keysym == NULL || keysym[0] == 0 ) {
        return NULL;
    }
    char **strv = g_strsplit(keysym,":",2);
    if( strv[1] == NULL ) {
        /* root device */
        if( display_driver == NULL || display_driver->resolve_keysym == NULL) {
            // Root device has no input handling
            g_strfreev(strv);
            return NULL;
        }
        uint16_t keycode = display_driver->resolve_keysym(g_strstrip(strv[0]));
        g_strfreev(strv);
        if( keycode == 0 ) {
            return NULL;
        }
        return &root_keymap[keycode-1];

    } else {
        char *id = g_strstrip(strv[0]);
        GList *ptr;
        for( ptr = input_drivers; ptr != NULL; ptr = g_list_next(ptr) ) {
            input_driver_entry_t entry = (input_driver_entry_t)ptr->data;
            if( strcasecmp( entry->driver->id, id ) == 0 ) {
                /* we have ze device */
                if( entry->driver->resolve_keysym == NULL ) {
                    g_strfreev(strv);
                    return NULL;
                } 
                uint16_t keycode = entry->driver->resolve_keysym(entry->driver, g_strstrip(strv[1]));
                g_strfreev(strv);
                if( keycode == 0 || keycode > entry->entry_count ) {
                    return NULL;
                }
                return &entry->keymap[keycode-1];
            }
        }
        g_strfreev(strv);
        return NULL; // device not found
    }
}

static struct keymap_entry **input_entry_from_keycode( input_driver_t driver, uint16_t keycode )
{
    GList *ptr;

    if( keycode == 0 ) {
        return NULL;
    }

    if( driver == NULL ) {
        return &root_keymap[keycode-1];
    }

    for( ptr = input_drivers; ptr != NULL; ptr = g_list_next(ptr) ) {
        input_driver_entry_t entry = (input_driver_entry_t)ptr->data;
        if( entry->driver == driver ) {
            if( keycode > entry->entry_count ) {
                return NULL;
            } else {
                return &entry->keymap[keycode-1];
            }
        }
    }
    return NULL;
}

gchar *input_keycode_to_keysym( input_driver_t driver, uint16_t keycode )
{
    if( keycode == 0 ) {
        return NULL;
    }
    if( driver == NULL ) {
        if( display_driver != NULL && display_driver->get_keysym_for_keycode != NULL ) {
            return display_driver->get_keysym_for_keycode(keycode);
        }
    } else if( driver->get_keysym_for_keycode ) {
        gchar *sym = driver->get_keysym_for_keycode(driver,keycode);
        if( sym != NULL ) {
            gchar *result = g_strdup_printf( "%s: %s", driver->id, sym );
            g_free(sym);
            return result;
        }
    }
    return NULL;
}


gboolean input_register_key( const gchar *keysym, input_key_callback_t callback,
                             void *data, uint32_t value )
{
    if( keysym == NULL ) {
        return FALSE;
    }
    int keys = 0;
    gchar **strv = g_strsplit(keysym, ",", 16);
    gchar **s = strv;
    while( *s != NULL ) {
        keymap_entry_t *entryp = input_entry_from_keysym(*s);
        if( entryp != NULL ) {
            *entryp = g_malloc0(sizeof(struct keymap_entry));
            (*entryp)->callback = callback;
            (*entryp)->data = data;
            (*entryp)->value = value;
            keys++;
        }
        s++;
    }
    g_strfreev(strv);
    return keys != 0;
}

void input_unregister_key( const gchar *keysym, input_key_callback_t callback,
                           void *data, uint32_t value )
{
    if( keysym == NULL ) {
        return;
    }

    gchar **strv = g_strsplit(keysym, ",", 16);
    gchar **s = strv;
    while( *s != NULL ) {
        keymap_entry_t *entryp = input_entry_from_keysym(*s);
        if( entryp != NULL && *entryp != NULL && (*entryp)->callback == callback &&
                (*entryp)->data == data && (*entryp)->value == value ) {
            g_free( *entryp );
            *entryp = NULL;
        }
        s++;
    }
    g_strfreev(strv);
}

gboolean input_register_keyboard_hook( input_key_callback_t callback,
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

void input_unregister_keyboard_hook( input_key_callback_t callback,
                            void *data )
{
    keymap_entry_t key = keyhooks;
    if( key != NULL ) {
        if( key->callback == callback && key->data == data ) {
            keyhooks = keyhooks->next;
            free(key);
            return;
        }
        while( key->next != NULL ) {
            if( key->next->callback == callback && key->next->data == data ) {
                keymap_entry_t next = key->next;
                key->next = next->next;
                free(next);
                return;
            }
            key = key->next;
        }
    }
}

gboolean input_is_key_valid( const gchar *keysym )
{
    keymap_entry_t *ptr = input_entry_from_keysym(keysym);
    return ptr != NULL;
}

gboolean input_is_key_registered( const gchar *keysym )
{
    keymap_entry_t *ptr = input_entry_from_keysym(keysym);
    return ptr != NULL && *ptr != NULL;
}

void input_event_keydown( input_driver_t driver, uint16_t keycode, uint32_t pressure )
{
    if( display_focused ) {
        keymap_entry_t *entryp = input_entry_from_keycode(driver,keycode);
        if( entryp != NULL && *entryp != NULL ) {
            (*entryp)->callback( (*entryp)->data, (*entryp)->value, pressure, TRUE );
        }
        if( driver == NULL ) {
            keymap_entry_t key = keyhooks;
            while( key != NULL ) {
                key->callback( key->data, keycode, pressure, TRUE );
                key = key->next;
            }
        }
    }
    if( display_keysym_hook != NULL ) {
        gchar *sym = input_keycode_to_keysym( driver, keycode );
        if( sym != NULL ) {
            display_keysym_hook(display_keysym_hook_data, sym);
            g_free(sym);
        }
    }
}

void input_event_keyup( input_driver_t driver, uint16_t keycode )
{
    if( display_focused ) {
        keymap_entry_t *entryp = input_entry_from_keycode(driver,keycode);
        if( entryp != NULL && *entryp != NULL ) {
            (*entryp)->callback( (*entryp)->data, (*entryp)->value, 0, FALSE );
        }

        if( driver == NULL ) {
            keymap_entry_t key = keyhooks;
            while( key != NULL ) {
                key->callback( key->data, keycode, 0, FALSE );
                key = key->next;
            }
        }
    }
}

uint16_t input_keycode_to_dckeysym( uint16_t keycode )
{
    return display_driver->convert_to_dckeysym(keycode);
}

void input_set_keysym_hook( display_keysym_callback_t hook, void *data )
{
    display_keysym_hook = hook;
    display_keysym_hook_data = data;
}

/***************** System mouse driver ****************/

static struct keymap_entry *mouse_keymap[MAX_MOUSE_BUTTONS];
static struct mouse_entry *mousehooks = NULL;
static uint32_t mouse_x = -1, mouse_y = -1, mouse_buttons = 0;

uint16_t mouse_resolve_keysym( struct input_driver *driver, const gchar *keysym )
{
    if( strncasecmp( keysym, "Button", 6 ) == 0 ){
        unsigned long button = strtoul( keysym+6, NULL, 10 );
        if( button > MAX_MOUSE_BUTTONS ) {
            return 0;
        }
        return (uint16_t)button;
    }
    return 0;
}

gchar *mouse_get_keysym( struct input_driver *driver, uint16_t keycode )
{
    return g_strdup_printf( "Button%d", (keycode) );
}

struct input_driver system_mouse_driver = { "Mouse", mouse_resolve_keysym, NULL, mouse_get_keysym, NULL };


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
        if( ent->callback == callback && ent->data == data ) {
            mousehooks = mousehooks->next;
            free(ent);
            return;
        }
        while( ent->next != NULL ) {
            if( ent->next->callback == callback && ent->next->data == data ) {
                mouse_entry_t next = ent->next;
                ent->next = next->next;
                free(next);
                return;
            }
            ent = ent->next;
        }
    }
}

void input_event_run_mouse_hooks( uint32_t buttons, int32_t x, int32_t y, gboolean absolute )
{
    mouse_entry_t ent = mousehooks;
    while( ent != NULL ) {
        ent->callback(ent->data, buttons, x, y, absolute);
        ent = ent->next;
    }
}

void input_event_mousedown( uint16_t button, int32_t x, int32_t y, gboolean absolute )
{
    if( absolute ) {
        mouse_x = x;
        mouse_y = y;
    }
    mouse_buttons |= (1<<button);
    input_event_keydown( &system_mouse_driver, button+1, MAX_PRESSURE );
    input_event_run_mouse_hooks( mouse_buttons, x, y, absolute );
}    

void input_event_mouseup( uint16_t button, int32_t x, int32_t y, gboolean absolute )
{
    if( absolute ) {
        mouse_x = x;
        mouse_y = y;
    }
    mouse_buttons &= ~(1<<button);
    input_event_keyup( &system_mouse_driver, button+1 );
    input_event_run_mouse_hooks( mouse_buttons, x, y, absolute );
}

void input_event_mousemove( int32_t x, int32_t y, gboolean absolute )
{
    if( absolute ) {
        mouse_x = x;
        mouse_y = y;
    }
    input_event_run_mouse_hooks( mouse_buttons, x, y, absolute );
}

/************************ Main display driver *************************/

void print_display_drivers( FILE *out )
{
    int i;
    fprintf( out, "Available video drivers:\n" );
    for( i=0; display_driver_list[i] != NULL; i++ ) {
        fprintf( out, "  %-8s %s\n", display_driver_list[i]->name,
                gettext(display_driver_list[i]->description) );
    }
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
    if( rv ) {
        input_register_device(&system_mouse_driver, MAX_MOUSE_BUTTONS);
    } else {
        display_driver = NULL;
    }
    return rv;
}

void display_set_focused( gboolean has_focus )
{
    display_focused = has_focus;
}
