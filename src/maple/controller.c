/**
 * $Id$
 *
 * Implements the standard dreamcast controller
 * Part No. HKT-7700
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
#include <assert.h>
#include "dream.h"
#include "dreamcast.h"
#include "display.h"
#include "maple.h"

/* First word of controller condition */
#define BUTTON_C            0x00000001 /* not on standard controller */
#define BUTTON_B            0x00000002
#define BUTTON_A            0x00000004
#define BUTTON_START        0x00000008
#define BUTTON_DPAD_UP      0x00000010
#define BUTTON_DPAD_DOWN    0x00000020
#define BUTTON_DPAD_LEFT    0x00000040
#define BUTTON_DPAD_RIGHT   0x00000080
#define BUTTON_Z            0x00000100 /* not on standard controller */
#define BUTTON_Y            0x00000200
#define BUTTON_X            0x00000400
#define BUTTON_D            0x00000800 /* not on standard controller */
#define BUTTON_LEFT_TRIGGER 0xFF000000 /* Bitmask */
#define BUTTON_RIGHT_TRIGGER 0x00FF0000 /* Bitmask */

/* Second word of controller condition (bitmasks) */
#define JOY_X_AXIS          0x000000FF
#define JOY_Y_AXIS          0x0000FF00
#define JOY_X_AXIS_CENTER   0x00000080
#define JOY_Y_AXIS_CENTER   0x00008000
#define JOY2_X_AXIS         0x00FF0000 /* not on standard controller */
#define JOY2_Y_AXIS         0xFF000000 /* not on standard controller */

/* The following bits are used by the emulator for flags but don't actually
 * appear in the hardware
 */
#define JOY_LEFT            0x80000001
#define JOY_RIGHT           0x80000002
#define JOY_UP              0x80000004
#define JOY_DOWN            0x80000008

/* Standard controller ID */
#define CONTROLLER_IDENT {0x00, 0x00, 0x00, 0x01,  0x00, 0x0f, 0x06, 0xfe,  0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00,  0xff, 0x00, 0x44, 0x72,  0x65, 0x61, 0x6d, 0x63,  0x61, 0x73, 0x74, 0x20,  \
    0x43, 0x6f, 0x6e, 0x74,  0x72, 0x6f, 0x6c, 0x6c,  0x65, 0x72, 0x20, 0x20,  0x20, 0x20, 0x20, 0x20, \
    0x20, 0x20, 0x20, 0x20,  0x50, 0x72, 0x6f, 0x64,  0x75, 0x63, 0x65, 0x64,  0x20, 0x42, 0x79, 0x20, \
    0x6f, 0x72, 0x20, 0x55,  0x6e, 0x64, 0x65, 0x72,  0x20, 0x4c, 0x69, 0x63,  0x65, 0x6e, 0x73, 0x65, \
    0x20, 0x46, 0x72, 0x6f,  0x6d, 0x20, 0x53, 0x45,  0x47, 0x41, 0x20, 0x45,  0x4e, 0x54, 0x45, 0x52, \
    0x50, 0x52, 0x49, 0x53,  0x45, 0x53, 0x2c, 0x4c,  0x54, 0x44, 0x2e, 0x20,  0x20, 0x20, 0x20, 0x20, \
    0xae, 0x01, 0xf4, 0x01}
#define CONTROLLER_VERSION {0x56, 0x65, 0x72, 0x73,  0x69, 0x6f, 0x6e, 0x20,  0x31, 0x2e, 0x30, 0x31, \
    0x30, 0x2c, 0x31, 0x39,  0x39, 0x38, 0x2f, 0x30,  0x39, 0x2f, 0x32, 0x38,  0x2c, 0x33, 0x31, 0x35, \
    0x2d, 0x36, 0x32, 0x31,  0x31, 0x2d, 0x41, 0x42,  0x20, 0x20, 0x20, 0x2c,  0x41, 0x6e, 0x61, 0x6c, \
    0x6f, 0x67, 0x20, 0x4d,  0x6f, 0x64, 0x75, 0x6c,  0x65, 0x20, 0x3a, 0x20,  0x54, 0x68, 0x65, 0x20, \
    0x34, 0x74, 0x68, 0x20,  0x45, 0x64, 0x69, 0x74,  0x69, 0x6f, 0x6e, 0x2e,  0x35, 0x2f, 0x38, 0x20, \
    0x20, 0x2b, 0x44, 0x46 }

#define CONTROLLER_CONFIG_ENTRIES 15

static void controller_attach( maple_device_t dev );
static void controller_detach( maple_device_t dev );
static void controller_destroy( maple_device_t dev );
static maple_device_t controller_clone( maple_device_t dev );
static maple_device_t controller_new();
static lxdream_config_entry_t controller_get_config( maple_device_t dev );
static void controller_set_config_value( maple_device_t dev, unsigned int key, const gchar *value );
static int controller_get_cond( maple_device_t dev, int function, unsigned char *outbuf,
                         unsigned int *outlen );

typedef struct controller_device {
    struct maple_device dev;
    uint32_t condition[2];
    struct lxdream_config_entry config[CONTROLLER_CONFIG_ENTRIES+1];
} *controller_device_t;

struct maple_device_class controller_class = { "Sega Controller", controller_new };

static struct controller_device base_controller = {
        { MAPLE_DEVICE_TAG, &controller_class, MAPLE_GRAB_DONTCARE,
          CONTROLLER_IDENT, CONTROLLER_VERSION, 
          controller_get_config, controller_set_config_value, 
          controller_attach, controller_detach, controller_destroy,
          controller_clone, NULL, NULL, controller_get_cond, NULL, NULL, NULL },
          {0x0000FFFF, 0x80808080}, 
          {{ "dpad left", N_("Dpad left"), CONFIG_TYPE_KEY },
           { "dpad right", N_("Dpad right"), CONFIG_TYPE_KEY },
           { "dpad up", N_("Dpad up"), CONFIG_TYPE_KEY },
           { "dpad down", N_("Dpad down"), CONFIG_TYPE_KEY },
           { "analog left", N_("Analog left"), CONFIG_TYPE_KEY },
           { "analog right", N_("Analog right"), CONFIG_TYPE_KEY },
           { "analog up", N_("Analog up"), CONFIG_TYPE_KEY },
           { "analog down", N_("Analog down"), CONFIG_TYPE_KEY },
           { "button X", N_("Button X"), CONFIG_TYPE_KEY },
           { "button Y", N_("Button Y"), CONFIG_TYPE_KEY },
           { "button A", N_("Button A"), CONFIG_TYPE_KEY },
           { "button B", N_("Button B"), CONFIG_TYPE_KEY },
           { "trigger left", N_("Trigger left"), CONFIG_TYPE_KEY },
           { "trigger right", N_("Trigger right"), CONFIG_TYPE_KEY },
           { "start", N_("Start button"), CONFIG_TYPE_KEY },
           { NULL, CONFIG_TYPE_NONE }} };

static int config_button_map[] = { 
        BUTTON_DPAD_LEFT, BUTTON_DPAD_RIGHT, BUTTON_DPAD_UP, BUTTON_DPAD_DOWN,
        JOY_LEFT, JOY_RIGHT, JOY_UP, JOY_DOWN, BUTTON_X, BUTTON_Y, BUTTON_A, 
        BUTTON_B, BUTTON_LEFT_TRIGGER, BUTTON_RIGHT_TRIGGER, BUTTON_START };
        
#define CONTROLLER(x) ((controller_device_t)(x))

static maple_device_t controller_new( )
{
    controller_device_t dev = malloc( sizeof(struct controller_device) );
    memcpy( dev, &base_controller, sizeof(base_controller) );
    return MAPLE_DEVICE(dev);
}

static maple_device_t controller_clone( maple_device_t srcdevice )
{
    controller_device_t src = (controller_device_t)srcdevice;
    controller_device_t dev = (controller_device_t)controller_new();
    lxdream_copy_config_list( dev->config, src->config );
    memcpy( dev->condition, src->condition, sizeof(src->condition) );
    return MAPLE_DEVICE(dev);
}

/**
 * Input callback 
 */
static void controller_key_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown )
{
    controller_device_t dev = (controller_device_t)mdev;
    if( isKeyDown ) {
        switch( value ) {
        case JOY_LEFT:
            dev->condition[1] &= ~JOY_X_AXIS;
            break;
        case JOY_RIGHT:
            dev->condition[1] |= JOY_X_AXIS;
            break;
        case JOY_UP:
            dev->condition[1] &= ~JOY_Y_AXIS;
            break;
        case JOY_DOWN:
            dev->condition[1] |= JOY_Y_AXIS;
            break;
        case BUTTON_LEFT_TRIGGER:
        case BUTTON_RIGHT_TRIGGER:
            dev->condition[0] |= value;
            break;
        default:
            dev->condition[0] &= ~value;
        }
    } else {
        switch(value ) {
        case JOY_LEFT:
        case JOY_RIGHT:
            dev->condition[1] = (dev->condition[1] & ~JOY_X_AXIS)| JOY_X_AXIS_CENTER;
            break;
        case JOY_UP:
        case JOY_DOWN:
            dev->condition[1] = (dev->condition[1] & ~JOY_Y_AXIS)| JOY_Y_AXIS_CENTER;
            break;
        case BUTTON_LEFT_TRIGGER:
        case BUTTON_RIGHT_TRIGGER:
            dev->condition[0] &= ~value;
            break;
        default:
            dev->condition[0] |= value;
        }
    }
}

static lxdream_config_entry_t controller_get_config( maple_device_t mdev )
{
    controller_device_t dev = (controller_device_t)mdev;
    return dev->config;
}

static void controller_set_config_value( maple_device_t mdev, unsigned int key, const gchar *value )
{
    controller_device_t dev = (controller_device_t)mdev;
    assert( key < CONTROLLER_CONFIG_ENTRIES );
    
    input_unregister_key( dev->config[key].value, controller_key_callback, dev, config_button_map[key] );
    lxdream_set_config_value( &dev->config[key], value );
    input_register_key( dev->config[key].value, controller_key_callback, dev, config_button_map[key] );
}

static void controller_destroy( maple_device_t mdev )
{
    free( mdev );
}

/**
 * Device is being attached to the bus. Go through the config and reserve the
 * keys we need.
 */
static void controller_attach( maple_device_t mdev )
{
    controller_device_t dev = (controller_device_t)mdev;
    int i;
    for( i=0; i<CONTROLLER_CONFIG_ENTRIES; i++ ) {
        input_register_key( dev->config[i].value, controller_key_callback, dev, config_button_map[i] );
    }
}

static void controller_detach( maple_device_t mdev )
{
    controller_device_t dev = (controller_device_t)mdev;
    int i;
    for( i=0; i<CONTROLLER_CONFIG_ENTRIES; i++ ) {
        input_unregister_key( dev->config[i].value, controller_key_callback, dev, config_button_map[i] );
    }

}


static int controller_get_cond( maple_device_t mdev, int function, unsigned char *outbuf,
                         unsigned int *outlen )
{
    controller_device_t dev = (controller_device_t)mdev;
    if( function == MAPLE_FUNC_CONTROLLER ) {
        *outlen = 2;
        memcpy( outbuf, dev->condition, 8 );
        return 0;
    } else {
        return MAPLE_ERR_FUNC_UNSUP;
    }
}

