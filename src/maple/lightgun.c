/**
 * $Id$
 *
 * Implementation of the SEGA lightgun device
 * Part No. HKT-7800
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "display.h"
#include "eventq.h"
#include "pvr2/pvr2.h"
#include "maple/maple.h"

#define BUTTON_B            0x00000002
#define BUTTON_A            0x00000004
#define BUTTON_START        0x00000008
#define BUTTON_DPAD_UP      0x00000010
#define BUTTON_DPAD_DOWN    0x00000020
#define BUTTON_DPAD_LEFT    0x00000040
#define BUTTON_DPAD_RIGHT   0x00000080

#define LIGHTGUN_IDENT {     0x00, 0x00, 0x00, 0x81,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0xFE, \
	0x00, 0x00, 0x00, 0x00,  0xFF, 0x01, 0x44, 0x72,  0x65, 0x61, 0x6D, 0x63,  0x61, 0x73, 0x74, 0x20, \
	0x47, 0x75, 0x6E, 0x20,  0x20, 0x20, 0x20, 0x20,  0x20, 0x20, 0x20, 0x20,  0x20, 0x20, 0x20, 0x20, \
	0x20, 0x20, 0x20, 0x20,  0x50, 0x72, 0x6F, 0x64,  0x75, 0x63, 0x65, 0x64,  0x20, 0x42, 0x79, 0x20, \
	0x6F, 0x72, 0x20, 0x55,  0x6E, 0x64, 0x65, 0x72,  0x20, 0x4C, 0x69, 0x63,  0x65, 0x6E, 0x73, 0x65, \
	0x20, 0x46, 0x72, 0x6F,  0x6D, 0x20, 0x53, 0x45,  0x47, 0x41, 0x20, 0x45,  0x4E, 0x54, 0x45, 0x52, \
	0x50, 0x52, 0x49, 0x53,  0x45, 0x53, 0x2C, 0x4C,  0x54, 0x44, 0x2E, 0x20,  0x20, 0x20, 0x20, 0x20, \
	0xDC, 0x00, 0x2C, 0x01 }
#define LIGHTGUN_VERSION {   0x56, 0x65, 0x72, 0x73,  0x69, 0x6F, 0x6E, 0x20,  0x31, 0x2E, 0x30, 0x30, \
	0x30, 0x2C, 0x31, 0x39,  0x39, 0x38, 0x2F, 0x30,  0x39, 0x2F, 0x31, 0x36,  0x2C, 0x33, 0x31, 0x35, \
	0x2D, 0x36, 0x31, 0x32,  0x35, 0x2D, 0x41, 0x47,  0x20, 0x20, 0x20, 0x2C,  0x55, 0x2C, 0x44, 0x2C, \
	0x4C, 0x2C, 0x52, 0x2C,  0x53, 0x2C, 0x41, 0x2C,  0x42, 0x20, 0x4B, 0x65,  0x79, 0x20, 0x26, 0x20, \
	0x53, 0x63, 0x61, 0x6E,  0x6E, 0x69, 0x6E, 0x67,  0x20, 0x4C, 0x69, 0x6E,  0x65, 0x20, 0x41, 0x6D, \
	0x70, 0x2E, 0x20, 0x20 }                                       


#define LIGHTGUN_CONFIG_ENTRIES 7

static void lightgun_attach( maple_device_t dev );
static void lightgun_detach( maple_device_t dev );
static void lightgun_destroy( maple_device_t dev );
static maple_device_t lightgun_clone( maple_device_t dev );
static maple_device_t lightgun_new();
static lxdream_config_entry_t lightgun_get_config( maple_device_t dev );
static void lightgun_set_config_value( maple_device_t dev, unsigned int key, const gchar *value );
static int lightgun_get_cond( maple_device_t dev, int function, unsigned char *outbuf,
                         unsigned int *outlen );
static void lightgun_start_gun( maple_device_t dev );
static void lightgun_stop_gun( maple_device_t dev );

typedef struct lightgun_device {
    struct maple_device dev;
    uint32_t condition[2];
    int gun_active;
    int mouse_x, mouse_y;
    struct lxdream_config_entry config[LIGHTGUN_CONFIG_ENTRIES+1];
} *lightgun_device_t;

struct maple_device_class lightgun_class = { "Sega Lightgun", 
        MAPLE_TYPE_PRIMARY|MAPLE_GRAB_NO|MAPLE_SLOTS_2, lightgun_new };

static struct lightgun_device base_lightgun = {
        { MAPLE_DEVICE_TAG, &lightgun_class,
          LIGHTGUN_IDENT, LIGHTGUN_VERSION, 
          lightgun_get_config, lightgun_set_config_value, 
          lightgun_attach, lightgun_detach, lightgun_destroy,
          lightgun_clone, NULL, NULL, lightgun_get_cond, NULL, NULL, NULL, NULL,
          lightgun_start_gun, lightgun_stop_gun},
          {0x0000FFFF, 0x80808080}, 0, -1, -1, 
          {{ "dpad left", N_("Dpad left"), CONFIG_TYPE_KEY },
           { "dpad right", N_("Dpad right"), CONFIG_TYPE_KEY },
           { "dpad up", N_("Dpad up"), CONFIG_TYPE_KEY },
           { "dpad down", N_("Dpad down"), CONFIG_TYPE_KEY },
           { "button A", N_("Button A"), CONFIG_TYPE_KEY },
           { "button B", N_("Button B"), CONFIG_TYPE_KEY },
           { "start", N_("Start button"), CONFIG_TYPE_KEY },
           { NULL, CONFIG_TYPE_NONE }} };

static int config_button_map[] = { 
        BUTTON_DPAD_LEFT, BUTTON_DPAD_RIGHT, BUTTON_DPAD_UP, BUTTON_DPAD_DOWN,
        BUTTON_A, BUTTON_B, BUTTON_START };
        
#define lightgun(x) ((lightgun_device_t)(x))

static maple_device_t lightgun_new( )
{
    lightgun_device_t dev = malloc( sizeof(struct lightgun_device) );
    memcpy( dev, &base_lightgun, sizeof(base_lightgun) );
    return MAPLE_DEVICE(dev);
}

static maple_device_t lightgun_clone( maple_device_t srcdevice )
{
    lightgun_device_t src = (lightgun_device_t)srcdevice;
    lightgun_device_t dev = (lightgun_device_t)lightgun_new();
    lxdream_copy_config_list( dev->config, src->config );
    memcpy( dev->condition, src->condition, sizeof(src->condition) );
    return MAPLE_DEVICE(dev);
}

/**
 * Input callback 
 */
static void lightgun_key_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown )
{
    lightgun_device_t dev = (lightgun_device_t)mdev;
    if( isKeyDown ) {
        dev->condition[0] &= ~value;
    } else {
        dev->condition[0] |= value;
    }
}

static lxdream_config_entry_t lightgun_get_config( maple_device_t mdev )
{
    lightgun_device_t dev = (lightgun_device_t)mdev;
    return dev->config;
}

static void lightgun_set_config_value( maple_device_t mdev, unsigned int key, const gchar *value )
{
    lightgun_device_t dev = (lightgun_device_t)mdev;
    assert( key < LIGHTGUN_CONFIG_ENTRIES );
    
    input_unregister_key( dev->config[key].value, lightgun_key_callback, dev, config_button_map[key] );
    lxdream_set_config_value( &dev->config[key], value );
    input_register_key( dev->config[key].value, lightgun_key_callback, dev, config_button_map[key] );
}

static void lightgun_destroy( maple_device_t mdev )
{
    free( mdev );
}


static void lightgun_mouse_callback( void *mdev, uint32_t buttons, int32_t x, int32_t y, gboolean absolute )
{
    lightgun_device_t dev = (lightgun_device_t)mdev;
    if( absolute ) {
        dev->mouse_x = x;
        dev->mouse_y = y;
        if( dev->gun_active ) {
            pvr2_queue_gun_event( x, y );
            dev->gun_active = FALSE;
        }
    }
}

/**
 * Device is being attached to the bus. Go through the config and reserve the
 * keys we need.
 */
static void lightgun_attach( maple_device_t mdev )
{
    lightgun_device_t dev = (lightgun_device_t)mdev;
    int i;
    for( i=0; i<LIGHTGUN_CONFIG_ENTRIES; i++ ) {
        input_register_key( dev->config[i].value, lightgun_key_callback, dev, config_button_map[i] );
    }
    input_register_mouse_hook( TRUE, lightgun_mouse_callback, dev );
    
}

static void lightgun_detach( maple_device_t mdev )
{
    lightgun_device_t dev = (lightgun_device_t)mdev;
    int i;
    for( i=0; i<LIGHTGUN_CONFIG_ENTRIES; i++ ) {
        input_unregister_key( dev->config[i].value, lightgun_key_callback, dev, config_button_map[i] );
    }
    input_unregister_mouse_hook( lightgun_mouse_callback, dev );

}


static int lightgun_get_cond( maple_device_t mdev, int function, unsigned char *outbuf,
                         unsigned int *outlen )
{
    lightgun_device_t dev = (lightgun_device_t)mdev;
    if( function == MAPLE_FUNC_CONTROLLER ) {
        *outlen = 2;
        memcpy( outbuf, dev->condition, 8 );
        return 0;
    } else {
        return MAPLE_ERR_FUNC_UNSUP;
    }
}

static void lightgun_start_gun( maple_device_t mdev )
{
    lightgun_device_t dev = (lightgun_device_t)mdev;
    if( dev->mouse_x != -1 && dev->mouse_y != -1 ) {
        pvr2_queue_gun_event( dev->mouse_x, dev->mouse_y );
    } else {
        // Wait for a mouse event
        dev->gun_active = 1;
    }
}

static void lightgun_stop_gun( maple_device_t mdev )
{
    lightgun_device_t dev = (lightgun_device_t)mdev;
    dev->gun_active = 0;
    event_cancel( EVENT_GUNPOS );
}
