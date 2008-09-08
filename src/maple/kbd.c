/**
 * $Id$
 *
 * Implements the standard dreamcast keyboard
 * Part No. HKT-7620
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
#include <X11/keysym.h>
#include "dream.h"
#include "dreamcast.h"
#include "display.h"
#include "maple.h"

#define KEYBOARD_IDENT {  0x00, 0x00, 0x00, 0x40,  0x02, 0x05, 0x00, 0x80,  0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00,  0x01, 0x00, 0x4b, 0x65,  0x79, 0x62, 0x6f, 0x61,  0x72, 0x64, 0x20, 0x20, \
    0x20, 0x20, 0x20, 0x20,  0x20, 0x20, 0x20, 0x20,  0x20, 0x20, 0x20, 0x20,  0x20, 0x20, 0x20, 0x20, \
    0x20, 0x20, 0x20, 0x20,  0x50, 0x72, 0x6f, 0x64,  0x75, 0x63, 0x65, 0x64,  0x20, 0x42, 0x79, 0x20, \
    0x6f, 0x72, 0x20, 0x55,  0x6e, 0x64, 0x65, 0x72,  0x20, 0x4c, 0x69, 0x63,  0x65, 0x6e, 0x73, 0x65, \
    0x20, 0x46, 0x72, 0x6f,  0x6d, 0x20, 0x53, 0x45,  0x47, 0x41, 0x20, 0x45,  0x4e, 0x54, 0x45, 0x52, \
    0x50, 0x52, 0x49, 0x53,  0x45, 0x53, 0x2c, 0x4c,  0x54, 0x44, 0x2e, 0x20,  0x20, 0x20, 0x20, 0x20, \
    0x2c, 0x01, 0x90, 0x01 }

#define KEYBOARD_VERSION {0x56, 0x65, 0x72, 0x73,  0x69, 0x6f, 0x6e, 0x20,  0x31, 0x2e, 0x30, 0x31, \
    0x30, 0x2c, 0x31, 0x39,  0x39, 0x39, 0x2f, 0x30,  0x34, 0x2f, 0x32, 0x37,  0x2c, 0x33, 0x31, 0x35, \
    0x2d, 0x36, 0x32, 0x31,  0x31, 0x2d, 0x41, 0x4d,  0x20, 0x20, 0x20, 0x2c,  0x4b, 0x65, 0x79, 0x20, \
    0x53, 0x63, 0x61, 0x6e,  0x20, 0x4d, 0x6f, 0x64,  0x75, 0x6c, 0x65, 0x20,  0x3a, 0x20, 0x54, 0x68, \
    0x65, 0x20, 0x32, 0x6e,  0x64, 0x20, 0x45, 0x64,  0x69, 0x74, 0x69, 0x6f,  0x6e, 0x2e, 0x20, 0x30, \
    0x34, 0x2f, 0x32, 0x35 }

void keyboard_attach( maple_device_t dev );
void keyboard_detach( maple_device_t dev );
maple_device_t keyboard_clone( maple_device_t dev );
maple_device_t keyboard_new();
int keyboard_get_cond( maple_device_t dev, int function, unsigned char *outbuf,
                       unsigned int *outlen );

typedef struct keyboard_device {
    struct maple_device dev;
    uint8_t condition[8];
} *keyboard_device_t; 

struct maple_device_class keyboard_class = { "Sega Keyboard", keyboard_new };

static struct keyboard_device base_keyboard = {
        { MAPLE_DEVICE_TAG, &keyboard_class, MAPLE_GRAB_DONTCARE,
                KEYBOARD_IDENT, KEYBOARD_VERSION, 
                NULL, NULL, keyboard_attach, keyboard_detach, maple_default_destroy,
                keyboard_clone, NULL, NULL, keyboard_get_cond, NULL, NULL, NULL },
                {0,0,0,0,0,0,0,0}, 
};

#define KEYBOARD(x) ((keyboard_device_t)(x))

maple_device_t keyboard_new( )
{
    keyboard_device_t dev = malloc( sizeof(struct keyboard_device) );
    memcpy( dev, &base_keyboard, sizeof(base_keyboard) );
    return MAPLE_DEVICE(dev);
}

maple_device_t keyboard_clone( maple_device_t srcdevice )
{
    keyboard_device_t src = (keyboard_device_t)srcdevice;
    keyboard_device_t dev = (keyboard_device_t)keyboard_new();
    memcpy( dev->condition, src->condition, sizeof(src->condition) );
    return MAPLE_DEVICE(dev);
}

void keyboard_key_down( keyboard_device_t dev, uint8_t key )
{
    int i;
    for( i=2; i<8; i++ ) {
        if( dev->condition[i] == key ) {
            return; // key already down, missed event or repeat
        } else if( dev->condition[i] == 0 ) {
            dev->condition[i] = key;
            return;
        }
    }
    /* Key array is full - skip for the moment */
}

void keyboard_key_up( keyboard_device_t dev, uint8_t key )
{
    int i;
    for( i=2; i<8; i++ ) {
        if( dev->condition[i] == key ) {
            for( ; i<7; i++ ) {
                dev->condition[i] = dev->condition[i+1];
            }
            dev->condition[7] = 0;
            break;
        }
    }
}

void keyboard_input_hook( void *mdev, uint32_t keycode, uint32_t pressure, gboolean isKeyDown )
{
    keyboard_device_t dev = (keyboard_device_t)mdev;
    uint16_t key = input_keycode_to_dckeysym( keycode );
    if( key != 0 ) {
        if( key >> 8 == 0xFF ) { // shift 
            if( isKeyDown ) {
                dev->condition[0] |= (key&0xFF);
            } else {
                dev->condition[0] &= ~(key&0xFF);
            }
        } else {
            if( isKeyDown ) {
                keyboard_key_down( dev, (uint8_t)key );
            } else {
                keyboard_key_up( dev, (uint8_t)key );
            }
        }
    }
    /*
    fprintf( stderr, "Key cond: %02X %02X  %02X %02X %02X %02X %02X %02X\n",
	     dev->condition[0], dev->condition[1], dev->condition[2],
	     dev->condition[3], dev->condition[4], dev->condition[5],
	     dev->condition[6], dev->condition[7] );
     */     
}

/**
 * Device is being attached to the bus. Go through the config and reserve the
 * keys we need.
 */
void keyboard_attach( maple_device_t mdev )
{
    input_register_keyboard_hook( keyboard_input_hook, mdev );
}

void keyboard_detach( maple_device_t mdev )
{
    input_unregister_keyboard_hook( keyboard_input_hook, mdev );
}


int keyboard_get_cond( maple_device_t mdev, int function, unsigned char *outbuf,
                       unsigned int *outlen )
{
    keyboard_device_t dev = (keyboard_device_t)mdev;
    if( function == MAPLE_FUNC_KEYBOARD ) {
        *outlen = 2;
        memcpy( outbuf, dev->condition, 8 );
        return 0;
    } else {
        return MAPLE_ERR_FUNC_UNSUP;
    }
}

