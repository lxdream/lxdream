/**
 * $Id: controller.c,v 1.2 2005-12-25 08:24:11 nkeynes Exp $
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
#include "maple.h"
#include "maple/controller.h"

void controller_attach( maple_device_t dev );
void controller_detach( maple_device_t dev );
int controller_get_cond( maple_device_t dev, int function, char *outbuf,
                         int *outlen );

static struct maple_device base_controller = {
    MAPLE_DEVICE_TAG, CONTROLLER_IDENT, CONTROLLER_VERSION, NULL, NULL,
    controller_get_cond, NULL, NULL, NULL,
    controller_attach, controller_detach };

typedef struct controller_device {
    struct maple_device dev;
    uint32_t condition[2];
} *controller_device_t;



maple_device_t controller_new( )
{
    controller_device_t dev = malloc( sizeof(struct controller_device) );
    memcpy( dev, &base_controller, sizeof(base_controller) );
    memset( dev->condition, 0, 8 );
    dev->condition[0] = 0x0000FFFF;
    return MAPLE_DEVICE(dev);
}


void controller_attach( maple_device_t dev )
{

}

void controller_detach( maple_device_t dev )
{

}


int controller_get_cond( maple_device_t mdev, int function, char *outbuf,
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

