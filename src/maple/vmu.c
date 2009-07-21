/**
 * $Id$
 *
 * Implementation of the SEGA VMU device
 * Part No. HKT-7000
 * 
 * The standard VMU implements 3 functions - Clock, LCD, and memory card, 
 * in addition to having it's own little CPU, buttons, etc. The CPU isn't
 * implemented just yet.   
 *
 * Copyright (c) 2009 Nathan Keynes.
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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "display.h"
#include "maple/maple.h"
#include "vmu/vmuvol.h"
#include "vmu/vmulist.h"

#define VMU_LCD_SIZE (48*4)
#define VMU_BLOCK_COUNT 256
#define VMU_BLOCK_SIZE 512
#define VMU_PHASE_SIZE 128
#define VMU_CONFIG_ENTRIES 1

#define VMU_IDENT {          0x00, 0x00, 0x00, 0x0E,  0x7E, 0x7E, 0x3F, 0x40,  0x00, 0x05, 0x10, 0x00, \
	0x00, 0x0F, 0x41, 0x00,  0xFF, 0x00, 0x56, 0x69,  0x73, 0x75, 0x61, 0x6C,  0x20, 0x4D, 0x65, 0x6D, \
	0x6F, 0x72, 0x79, 0x20,  0x20, 0x20, 0x20, 0x20,  0x20, 0x20, 0x20, 0x20,  0x20, 0x20, 0x20, 0x20, \
	0x20, 0x20, 0x20, 0x20,  0x50, 0x72, 0x6F, 0x64,  0x75, 0x63, 0x65, 0x64,  0x20, 0x42, 0x79, 0x20, \
	0x6F, 0x72, 0x20, 0x55,  0x6E, 0x64, 0x65, 0x72,  0x20, 0x4C, 0x69, 0x63,  0x65, 0x6E, 0x73, 0x65, \
	0x20, 0x46, 0x72, 0x6F,  0x6D, 0x20, 0x53, 0x45,  0x47, 0x41, 0x20, 0x45,  0x4E, 0x54, 0x45, 0x52, \
	0x50, 0x52, 0x49, 0x53,  0x45, 0x53, 0x2C, 0x4C,  0x54, 0x44, 0x2E, 0x20,  0x20, 0x20, 0x20, 0x20, \
	0x7C, 0x00, 0x82, 0x00 }
#define VMU_VERSION {        0x56, 0x65, 0x72, 0x73,  0x69, 0x6F, 0x6E, 0x20,  0x31, 0x2E, 0x30, 0x30, \
	0x35, 0x2C, 0x31, 0x39,  0x39, 0x39, 0x2F, 0x30,  0x34, 0x2F, 0x31, 0x35,  0x2C, 0x33, 0x31, 0x35, \
	0x2D, 0x36, 0x32, 0x30,  0x38, 0x2D, 0x30, 0x33,  0x2C, 0x53, 0x45, 0x47,  0x41, 0x20, 0x56, 0x69, \
	0x73, 0x75, 0x61, 0x6C,  0x20, 0x4D, 0x65, 0x6D,  0x6F, 0x72, 0x79, 0x20,  0x53, 0x79, 0x73, 0x74, \
	0x65, 0x6D, 0x20, 0x42,  0x49, 0x4F, 0x53, 0x20,  0x50, 0x72, 0x6F, 0x64,  0x75, 0x63, 0x65, 0x64, \
	0x20, 0x62, 0x79, 0x20 }

static void vmu_destroy( maple_device_t dev );
static maple_device_t vmu_clone( maple_device_t dev );
static maple_device_t vmu_new();
static lxdream_config_group_t vmu_get_config( maple_device_t dev );
static gboolean vmu_set_config_value( void *data, lxdream_config_group_t group, unsigned int key,
                                      const gchar *oldvalue, const gchar *value );
static int vmu_get_condition( maple_device_t dev, int function, unsigned char *outbuf,
                              unsigned int *outlen );
static int vmu_get_meminfo( maple_device_t dev, int function, unsigned int pt, 
                            unsigned char *outbuf, unsigned int *outlen );
static void vmu_attach(struct maple_device *dev);
static void vmu_detach(struct maple_device *dev);
static int vmu_read_block(struct maple_device *dev, int function, unsigned int pt,
                   uint32_t block, unsigned int phase, 
                   unsigned char *outbuf, unsigned int *buflen);
static int vmu_write_block(struct maple_device *dev, int function, unsigned int pt,
                    uint32_t block, unsigned int phase, 
                    unsigned char *inbuf, unsigned int buflen);

typedef struct vmu_device {
    struct maple_device dev;
    vmu_volume_t vol;
    char lcd_bitmap[VMU_LCD_SIZE]; /* 48x32 bitmap */
    struct lxdream_config_group config;
} *vmu_device_t;

#define DEV_FROM_CONFIG_GROUP(grp)  ((vmu_device_t)(((char *)grp) - offsetof( struct vmu_device, config )))

struct maple_device_class vmu_class = { "Sega VMU", MAPLE_GRAB_DONTCARE, vmu_new };

static struct vmu_device base_vmu = {
        { MAPLE_DEVICE_TAG, &vmu_class,
          VMU_IDENT, VMU_VERSION, 
          vmu_get_config,
          vmu_attach, vmu_detach, vmu_destroy,
          vmu_clone, NULL, NULL, vmu_get_condition, NULL,
          vmu_get_meminfo, vmu_read_block, vmu_write_block, NULL, NULL },
          NULL, {0},
          {"Sega VMU", vmu_set_config_value, NULL, NULL,
          {{ "volume", N_("Volume"), CONFIG_TYPE_FILE },
           { NULL, CONFIG_TYPE_NONE }}} };



static maple_device_t vmu_new( )
{
    vmu_device_t dev = malloc( sizeof(struct vmu_device) );
    dev->config.data = dev;
    memcpy( dev, &base_vmu, sizeof(base_vmu) );
    return MAPLE_DEVICE(dev);
}

static maple_device_t vmu_clone( maple_device_t srcdevice )
{
    vmu_device_t src = (vmu_device_t)srcdevice;
    vmu_device_t dev = (vmu_device_t)vmu_new();
    lxdream_copy_config_group( &dev->config, &src->config );
    dev->config.data = dev;
    return MAPLE_DEVICE(dev);
}

static lxdream_config_group_t vmu_get_config( maple_device_t mdev )
{
    vmu_device_t dev = (vmu_device_t)mdev;
    return &dev->config;
}

static gboolean vmu_set_config_value( void *data, lxdream_config_group_t group, unsigned int key,
                                      const gchar *oldvalue, const gchar *value )
{
    vmu_device_t vmu = (vmu_device_t)data;
    assert( key < VMU_CONFIG_ENTRIES );
    
    if( vmu->vol != NULL ) {
        vmulist_detach_vmu(vmu->vol);
    }
    vmu->vol = vmulist_get_vmu_by_filename( value );
    if( vmu->vol != NULL ) {
        vmulist_attach_vmu(vmu->vol, "MAPLE");
    }
    return TRUE;
}

void vmu_attach(struct maple_device *dev)
{
    vmu_device_t vmu = (vmu_device_t)dev;
    if( vmu->config.params[0].value != NULL ) {
        vmu->vol = vmulist_get_vmu_by_filename(vmu->config.params[0].value);
        if( vmu->vol != NULL ) {
            vmulist_attach_vmu(vmu->vol, "MAPLE");
        }
    }
}

static void vmu_detach(struct maple_device *dev)
{
    vmu_device_t vmu = (vmu_device_t)dev;
    if( vmu->vol != NULL ) {
        vmulist_detach_vmu(vmu->vol);
        vmu->vol = NULL;
    }
}

static void vmu_destroy( maple_device_t dev )
{
    vmu_device_t vmu = (vmu_device_t)dev;
    free( dev );
}

static int vmu_get_condition(struct maple_device *dev, int function, 
                      unsigned char *outbuf, unsigned int *buflen)
{
    return 0;
}
static int vmu_set_condition(struct maple_device *dev, int function, 
                      unsigned char *inbuf, unsigned int buflen)
{
    return MAPLE_ERR_NO_RESPONSE; /* CHECKME */
}

static int vmu_get_meminfo(struct maple_device *dev, int function, unsigned int pt, 
                           unsigned char *outbuf, unsigned int *buflen)
{
    struct vmu_device *vmu = (struct vmu_device *)dev;
    switch(function) {
    case MAPLE_FUNC_MEMORY:
        if( vmu->vol != NULL ) {
            const struct vmu_volume_metadata *md = vmu_volume_get_metadata( vmu->vol, pt ); 
            memcpy( outbuf, md, sizeof(struct vmu_volume_metadata) );
            *buflen = sizeof(struct vmu_volume_metadata);
            return 0;
        } // Else fallthrough 
    case MAPLE_FUNC_LCD:
    case MAPLE_FUNC_CLOCK:
        return MAPLE_ERR_NO_RESPONSE;
    default:
        return MAPLE_ERR_FUNC_UNSUP;
    }

}
static int vmu_read_block(struct maple_device *dev, int function, unsigned int pt,
                   uint32_t block, unsigned int phase, 
                   unsigned char *outbuf, unsigned int *buflen)
{
    struct vmu_device *vmu = (struct vmu_device *)dev;
    switch( function ) {
    case MAPLE_FUNC_LCD:
        if( pt == 0 && block == 0 ) {
            *buflen = VMU_LCD_SIZE/4;
            memcpy( outbuf, vmu->lcd_bitmap, VMU_LCD_SIZE/4 );
        }
        return 0;
        break;
    case MAPLE_FUNC_MEMORY:
        if( vmu->vol != NULL ) {
            vmu_volume_read_block( vmu->vol, pt, block, outbuf );
            return 0;
        }
        // Else fallthrough for now
    case MAPLE_FUNC_CLOCK:
        return MAPLE_ERR_NO_RESPONSE; /* CHECKME */
    default:
        return MAPLE_ERR_FUNC_UNSUP;
    }
}

static int vmu_write_block(struct maple_device *dev, int function, unsigned int pt,
                    uint32_t block, unsigned int phase, 
                    unsigned char *inbuf, unsigned int buflen)
{
    struct vmu_device *vmu = (struct vmu_device *)dev;
    switch( function ) {
    case MAPLE_FUNC_LCD:
        if( pt == 0 && block == 0 && buflen == (VMU_LCD_SIZE/4) ) {
            memcpy( vmu->lcd_bitmap, inbuf, VMU_LCD_SIZE );
        }
        return 0;
        break;
    case MAPLE_FUNC_MEMORY:
        if( vmu->vol != NULL && buflen == (VMU_PHASE_SIZE/4) ) {
            vmu_volume_write_phase( vmu->vol, pt, block, phase, inbuf );
            return 0;
        } 
        // Else fallthrough for now
    case MAPLE_FUNC_CLOCK:
        return MAPLE_ERR_NO_RESPONSE; /* CHECKME */
    default:
        return MAPLE_ERR_FUNC_UNSUP;
    }
}
