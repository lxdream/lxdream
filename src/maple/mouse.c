/**
 * $Id$
 *
 * Implementation of the standard mouse device
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
#include <stdio.h>
#include <string.h>
#include "maple/maple.h"

#define MOUSE_IDENT     { 0x00, 0x00, 0x02, 0x00,  0x02, 0x00, 0x07, 0x00,  0x00, 0x00, 0x00, 0x00,\
 0x00, 0x00, 0x00, 0x00,  0xff, 0x00, 0x44, 0x72,  0x65, 0x61, 0x6d, 0x63,  0x61, 0x73, 0x74, 0x20,\
 0x4d, 0x6f, 0x75, 0x73,  0x65, 0x20, 0x20, 0x20,  0x20, 0x20, 0x20, 0x20,  0x20, 0x20, 0x20, 0x20,\
 0x20, 0x20, 0x20, 0x20,  0x50, 0x72, 0x6f, 0x64,  0x75, 0x63, 0x65, 0x64,  0x20, 0x42, 0x79, 0x20,\
 0x6f, 0x72, 0x20, 0x55,  0x6e, 0x64, 0x65, 0x72,  0x20, 0x4c, 0x69, 0x63,  0x65, 0x6e, 0x73, 0x65,\
 0x20, 0x46, 0x72, 0x6f,  0x6d, 0x20, 0x53, 0x45,  0x47, 0x41, 0x20, 0x45,  0x4e, 0x54, 0x45, 0x52,\
 0x50, 0x52, 0x49, 0x53,  0x45, 0x53, 0x2c, 0x4c,  0x54, 0x44, 0x2e, 0x20,  0x20, 0x20, 0x20, 0x20,\
 0x90, 0x01, 0xf4, 0x01 }
#define MOUSE_VERSION   { 0x56, 0x65, 0x72, 0x73,  0x69, 0x6f, 0x6e, 0x20,  0x31, 0x2e, 0x30, 0x30,\
 0x30, 0x2c, 0x32, 0x30,  0x30, 0x30, 0x2f, 0x30,  0x32, 0x2f, 0x32, 0x35,  0x2c, 0x33, 0x31, 0x35,\
 0x2d, 0x36, 0x32, 0x31,  0x31, 0x2d, 0x41, 0x54,  0x20, 0x20, 0x20, 0x2c,  0x33, 0x20, 0x42, 0x75,\
 0x74, 0x74, 0x6f, 0x6e,  0x20, 0x26, 0x20, 0x58,  0x2d, 0x59, 0x20, 0x42,  0x61, 0x6c, 0x6c, 0x20,\
 0x26, 0x20, 0x5a, 0x20,  0x57, 0x68, 0x65, 0x65,  0x6c, 0x20, 0x2c, 0x34,  0x30, 0x30, 0x64, 0x70,\
 0x69, 0x20, 0x20, 0x20 }

#define BUTTON_MIDDLE 0x01
#define BUTTON_RIGHT  0x02
#define BUTTON_LEFT   0x04
#define BUTTON_THUMB  0x08

void mouse_attach( maple_device_t dev );
void mouse_detach( maple_device_t dev );
maple_device_t mouse_clone( maple_device_t dev );
maple_device_t mouse_new();
int mouse_get_cond( maple_device_t dev, int function, unsigned char *outbuf,
                         unsigned int *outlen );

typedef struct mouse_device {
    struct maple_device dev;
    uint32_t buttons;
    int32_t axis[8];
} *mouse_device_t; 

struct maple_device_class mouse_class = { "Sega Mouse", mouse_new };

static struct mouse_device base_mouse = {
    { MAPLE_DEVICE_TAG, &mouse_class, MOUSE_IDENT, MOUSE_VERSION, 
      NULL, mouse_attach, mouse_detach, maple_default_destroy,
      mouse_clone, NULL, NULL, mouse_get_cond, NULL, NULL, NULL },
    0, {0,0,0,0,0,0,0,0}, 
};

static int32_t mouse_axis_scale_factors[8] = { 10, 10, 1, 1, 1, 1, 1, 1 };

#define MOUSE(x) ((mouse_device_t)(x))

maple_device_t mouse_new( )
{
    mouse_device_t dev = malloc( sizeof(struct mouse_device) );
    memcpy( dev, &base_mouse, sizeof(base_mouse) );
    return MAPLE_DEVICE(dev);
}

maple_device_t mouse_clone( maple_device_t srcdevice )
{
    mouse_device_t src = (mouse_device_t)srcdevice;
    mouse_device_t dev = (mouse_device_t)mouse_new();
    dev->buttons = src->buttons;
    memcpy( dev->axis, src->axis, sizeof(src->axis) );
    return MAPLE_DEVICE(dev);
}

void mouse_input_callback( void *mdev, uint32_t buttons, int32_t x, int32_t y )
{
    mouse_device_t dev = (mouse_device_t)mdev;
    dev->buttons = 0xFF;
    if( buttons & 0x01 ) {
	dev->buttons &= ~BUTTON_LEFT;
    }
    if( buttons & 0x02 ) {
	dev->buttons &= ~BUTTON_MIDDLE;
    } 
    if( buttons & 0x04 ) {
	dev->buttons &= ~BUTTON_RIGHT;
    }
    if( buttons & 0x08 ) {
	dev->buttons &= ~BUTTON_THUMB;
    }
    dev->axis[0] += x;
    dev->axis[1] += y;
}

void mouse_attach( maple_device_t dev )
{
    input_register_mouse_hook( TRUE, mouse_input_callback, dev );
}

void mouse_detach( maple_device_t dev )
{
    input_unregister_mouse_hook( mouse_input_callback, dev );
}

int mouse_get_cond( maple_device_t mdev, int function, unsigned char *outbuf,
		       unsigned int *outlen )
{
    mouse_device_t dev = (mouse_device_t)mdev;
    if( function == MAPLE_FUNC_MOUSE ) {
        *outlen = 5;
	*(uint32_t *)outbuf = dev->buttons;
	uint16_t *p = (uint16_t *)(outbuf+4);
	int i;
	// Axis values are in the range 0..0x3FF, where 0x200 is zero movement
	for( i=0; i<8; i++ ) {
	    int32_t value = dev->axis[i] / mouse_axis_scale_factors[i];
	    if( value < -0x200 ) {
		p[i] = 0;
	    } else if( value > 0x1FF ) {
		p[i] = 0x3FF;
	    } else {
		p[i] = 0x200 + value;
	    }
	    dev->axis[i] = 0; // clear after returning.
	}
        return 0;
    } else {
        return MAPLE_ERR_FUNC_UNSUP;
    }
}

