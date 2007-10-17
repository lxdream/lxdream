/**
 * $Id: controller.c,v 1.7 2007-10-17 11:26:45 nkeynes Exp $
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
#include "dream.h"
#include "dreamcast.h"
#include "display.h"
#include "maple.h"
#include "maple/controller.h"

#define CONTROLLER_CONFIG_ENTRIES 16

void controller_attach( maple_device_t dev );
void controller_detach( maple_device_t dev );
void controller_destroy( maple_device_t dev );
maple_device_t controller_new();
lxdream_config_entry_t controller_get_config( maple_device_t dev );
int controller_get_cond( maple_device_t dev, int function, unsigned char *outbuf,
                         int *outlen );

typedef struct controller_device {
    struct maple_device dev;
    uint32_t condition[2];
    struct lxdream_config_entry config[CONTROLLER_CONFIG_ENTRIES];
} *controller_device_t;

struct maple_device_class controller_class = { "Sega Controller", controller_new };

static struct controller_device base_controller = {
    { MAPLE_DEVICE_TAG, &controller_class, CONTROLLER_IDENT, CONTROLLER_VERSION, 
      controller_get_config, controller_attach, controller_detach, controller_destroy,
      NULL, NULL, controller_get_cond, NULL, NULL, NULL },
    {0x0000FFFF, 0x80808080}, 
    {{ "dpad left", CONFIG_TYPE_KEY },
     { "dpad right", CONFIG_TYPE_KEY },
     { "dpad up", CONFIG_TYPE_KEY },
     { "dpad down", CONFIG_TYPE_KEY },
     { "analog left", CONFIG_TYPE_KEY },
     { "analog right", CONFIG_TYPE_KEY },
     { "analog up", CONFIG_TYPE_KEY },
     { "analog down", CONFIG_TYPE_KEY },
     { "button X", CONFIG_TYPE_KEY },
     { "button Y", CONFIG_TYPE_KEY },
     { "button A", CONFIG_TYPE_KEY },
     { "button B", CONFIG_TYPE_KEY },
     { "trigger left", CONFIG_TYPE_KEY },
     { "trigger right", CONFIG_TYPE_KEY },
     { "start", CONFIG_TYPE_KEY },
     { NULL, CONFIG_TYPE_NONE }} };

#define CONTROLLER(x) ((controller_device_t)(x))

maple_device_t controller_new( )
{
    controller_device_t dev = malloc( sizeof(struct controller_device) );
    memcpy( dev, &base_controller, sizeof(base_controller) );
    return MAPLE_DEVICE(dev);
}

/**
 * Input callback 
 */
void controller_key_callback( void *mdev, uint32_t value, gboolean isKeyDown )
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

lxdream_config_entry_t controller_get_config( maple_device_t mdev )
{
    controller_device_t dev = (controller_device_t)mdev;
    return dev->config;
}

void controller_destroy( maple_device_t mdev )
{
    free( mdev );
}

/**
 * Device is being attached to the bus. Go through the config and reserve the
 * keys we need.
 */
void controller_attach( maple_device_t mdev )
{
    controller_device_t dev = (controller_device_t)mdev;
    input_register_key( dev->config[0].value, controller_key_callback, dev, BUTTON_DPAD_LEFT );
    input_register_key( dev->config[1].value, controller_key_callback, dev, BUTTON_DPAD_RIGHT );
    input_register_key( dev->config[2].value, controller_key_callback, dev, BUTTON_DPAD_UP );
    input_register_key( dev->config[3].value, controller_key_callback, dev, BUTTON_DPAD_DOWN );
    input_register_key( dev->config[4].value, controller_key_callback, dev, JOY_LEFT );
    input_register_key( dev->config[5].value, controller_key_callback, dev, JOY_RIGHT );
    input_register_key( dev->config[6].value, controller_key_callback, dev, JOY_UP );
    input_register_key( dev->config[7].value, controller_key_callback, dev, JOY_DOWN );
    input_register_key( dev->config[8].value, controller_key_callback, dev, BUTTON_X );
    input_register_key( dev->config[9].value, controller_key_callback, dev, BUTTON_Y );
    input_register_key( dev->config[10].value, controller_key_callback, dev, BUTTON_A );
    input_register_key( dev->config[11].value, controller_key_callback, dev, BUTTON_B );
    input_register_key( dev->config[12].value, controller_key_callback, dev, BUTTON_LEFT_TRIGGER );
    input_register_key( dev->config[13].value, controller_key_callback, dev, BUTTON_RIGHT_TRIGGER );
    input_register_key( dev->config[14].value, controller_key_callback, dev, BUTTON_START );
}

void controller_detach( maple_device_t dev )
{

}


int controller_get_cond( maple_device_t mdev, int function, unsigned char *outbuf,
                         int *outlen )
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

