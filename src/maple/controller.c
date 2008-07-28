/**
 * $Id$
 *
 * Implements the standard dreamcast controller
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
#include "maple/controller.h"

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
        { MAPLE_DEVICE_TAG, &controller_class, CONTROLLER_IDENT, CONTROLLER_VERSION, 
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

