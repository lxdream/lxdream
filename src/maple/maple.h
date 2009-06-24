/**
 * $Id$
 *
 * Maple bus definitions
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

#ifndef lxdream_maple_H
#define lxdream_maple_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "config.h"

#define MAPLE_PORTS 4
#define MAPLE_MAX_DEVICES 24 /* 1 Primary + 5 secondary per port */
#define MAPLE_USER_SLOTS 2   /* Number of slots to show in configuration */
    
/* Map devices to a single id from 0..MAPLE_MAX_DEVICES, to simplify
 * configuration. Port is 0..3 (A..D) representing the primary ports, and
 * slot is 0 for the primary, or 1..5 for for one the secondary slots.
 */
#define MAPLE_DEVID(port,slot) ( (port)|((slot)<<2) )
#define MAPLE_DEVID_PORT(id)   ((id)&0x03)
#define MAPLE_DEVID_SLOT(id)   ((id)>>2)
    
#define MAPLE_CMD_INFO        1  /* Request device information */
#define MAPLE_CMD_EXT_INFO    2  /* Request extended information */
#define MAPLE_CMD_RESET       3  /* Reset device */
#define MAPLE_CMD_SHUTDOWN    4  /* Shutdown device */
#define MAPLE_CMD_GET_COND    9  /* Get condition */
#define MAPLE_CMD_MEM_INFO    10 /* Get memory information */
#define MAPLE_CMD_READ_BLOCK  11 /* Block read */
#define MAPLE_CMD_WRITE_BLOCK 12 /* Block write */
#define MAPLE_CMD_SYNC_BLOCK  13 /* Block sync */
#define MAPLE_CMD_SET_COND    14 /* Set condition */
#define MAPLE_RESP_INFO       5  /* Device information response */
#define MAPLE_RESP_EXT_INFO   6  /* Extended device information response */
#define MAPLE_RESP_ACK        7  /* Acknowledge command */
#define MAPLE_RESP_DATA       8  /* Bytes read */
#define MAPLE_ERR_NO_RESPONSE -1 /* Device did not respond */
#define MAPLE_ERR_FUNC_UNSUP  -2 /* Function code unsupported */
#define MAPLE_ERR_CMD_UNKNOWN -3 /* Command code unknown */
#define MAPLE_ERR_RETRY       -4 /* Retry command */
#define MAPLE_ERR_FILE        -5 /* File error? */

#define MAPLE_FUNC_CONTROLLER 0x01000000
#define MAPLE_FUNC_MEMORY     0x02000000
#define MAPLE_FUNC_LCD        0x04000000
#define MAPLE_FUNC_CLOCK      0x08000000
#define MAPLE_FUNC_MICROPHONE 0x10000000
#define MAPLE_FUNC_AR_GUN     0x20000000
#define MAPLE_FUNC_KEYBOARD   0x40000000
#define MAPLE_FUNC_LIGHT_GUN  0x80000000
#define MAPLE_FUNC_PURU_PURU  0x00010000
#define MAPLE_FUNC_MOUSE      0x00020000

/* Internal flags, mainly for UI consumption */
#define MAPLE_GRAB_DONTCARE   0x00
#define MAPLE_GRAB_YES        0x01
#define MAPLE_GRAB_NO         0x02
#define MAPLE_GRAB_MASK       0x03
#define MAPLE_TYPE_PRIMARY    0x08  /* attaches directly to maple port */
#define MAPLE_SLOTS_MASK      0xF0  /* number of slots on device (primaries only) */
#define MAPLE_SLOTS_1         0x10
#define MAPLE_SLOTS_2         0x20
#define MAPLE_SLOTS(x)        ((((x)->flags)&MAPLE_SLOTS_MASK)>>4)
    
#define MAPLE_DEVICE_TAG 0x4D41504C
#define MAPLE_DEVICE(x) ((maple_device_t)x)

/* Some convenience methods for VMU handling */
#define MAPLE_IS_VMU(dev)  ((dev)->device_class == &vmu_class)
#define MAPLE_IS_VMU_CLASS(clz)  ((clz) == &vmu_class)
#define MAPLE_VMU_NAME(dev) (((dev)->get_config(dev))[0].value)
#define MAPLE_SET_VMU_NAME(dev,name) ((dev)->set_config_value((dev),0,(name)))
#define MAPLE_VMU_HAS_NAME(d1,name) (MAPLE_VMU_NAME(d1) == NULL ? name == NULL : \
    name != NULL && strcmp(MAPLE_VMU_NAME(d1),name) == 0)
#define MAPLE_IS_SAME_VMU(d1,d2) (MAPLE_VMU_NAME(d1) == NULL ? MAPLE_VMU_NAME(d2) == NULL : \
    MAPLE_VMU_NAME(d2) != NULL && strcmp(MAPLE_VMU_NAME(d1),MAPLE_VMU_NAME(d2)) == 0)

typedef const struct maple_device_class *maple_device_class_t;
typedef struct maple_device *maple_device_t;

struct maple_device_class {
    const char *name;
    int flags;
    maple_device_t (*new_device)();
};

/**
 * Table of functions to be implemented by any maple device.
 */
struct maple_device {
    uint32_t _tag;
    maple_device_class_t device_class;
    unsigned char ident[112];
    unsigned char version[80];
    lxdream_config_entry_t (*get_config)(struct maple_device *dev);
    void (*set_config_value)(struct maple_device *dev, unsigned int key, const gchar *value);
    void (*attach)(struct maple_device *dev);
    void (*detach)(struct maple_device *dev);
    void (*destroy)(struct maple_device *dev);
    struct maple_device * (*clone)(struct maple_device *dev);
    int (*reset)(struct maple_device *dev);
    int (*shutdown)(struct maple_device *dev);
    int (*get_condition)(struct maple_device *dev,
                         int function, unsigned char *outbuf, unsigned int *buflen);
    int (*set_condition)(struct maple_device *dev,
                         int function, unsigned char *inbuf, unsigned int buflen);
    int (*get_memory_info)(struct maple_device *dev, int function, unsigned int partition,
                           unsigned char *outbuf, unsigned int *buflen);
    int (*read_block)(struct maple_device *dev, int function, unsigned int partition,
            uint32_t block, unsigned int phase, unsigned char *outbuf, unsigned int *buflen);
    int (*write_block)(struct maple_device *dev, int function, unsigned int partition,
            uint32_t block, unsigned int phase, unsigned char *inbuf, unsigned int buflen);
    void (*start_gun)(struct maple_device *dev);
    void (*stop_gun)(struct maple_device *dev);
};

extern struct maple_device_class controller_class;
extern struct maple_device_class keyboard_class;
extern struct maple_device_class lightgun_class;
extern struct maple_device_class mouse_class;
extern struct maple_device_class vmu_class;

maple_device_t maple_new_device( const gchar *name );
maple_device_t maple_get_device( unsigned int port, unsigned int periph );
const struct maple_device_class *maple_get_device_class( const gchar *name );
const struct maple_device_class **maple_get_device_classes();
lxdream_config_entry_t maple_get_device_config( maple_device_t dev );
void maple_set_device_config_value( maple_device_t dev, unsigned int key, const gchar *value );

void maple_handle_buffer( uint32_t buffer );
void maple_attach_device( maple_device_t dev, unsigned int port, unsigned int periph );
void maple_detach_device( unsigned int port, unsigned int periph );
void maple_detach_all( );
void maple_reattach_all( );
gboolean maple_should_grab();

/**
 * Default destroy implementation that just frees the dev memory.
 */
void maple_default_destroy( maple_device_t dev );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_maple_H */
