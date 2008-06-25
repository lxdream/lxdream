/**
 * $Id$
 *
 * OSX native cd-rom driver.
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

#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/scsi-commands/SCSITaskLib.h>
#include <IOKit/scsi-commands/SCSICommandOperationCodes.h>
#include <stdio.h>
#include "gdrom/gddriver.h"

static gboolean osx_image_is_valid( FILE *f );
static gdrom_disc_t osx_open_device( const gchar *filename, FILE *f );

struct gdrom_image_class cdrom_device_class = { "osx", NULL,
                        osx_image_is_valid, osx_open_device };

typedef struct osx_cdrom_disc {
    struct gdrom_disc disc;
    
    io_object_t entry;
    MMCDeviceInterface **mmc;
    SCSITaskDeviceInterface **scsi;
} *osx_cdrom_disc_t;

/**
 * CD-ROM drive visitor. Returns FALSE to continue iterating, TRUE if the desired CD-ROM
 * has been found. In the latter case, the io_object is returned from find_cdrom_device
 * (and not freed)
 */ 
typedef gboolean (*find_cdrom_callback_t)( io_object_t object, char *vendor, char *product,
                                      void *user_data );

io_object_t find_cdrom_device( find_cdrom_callback_t callback, void *user_data )
{
    mach_port_t master_port;
    CFMutableDictionaryRef match;
    io_iterator_t services;
    io_object_t object;
    
    if( IOMasterPort( MACH_PORT_NULL, &master_port ) != KERN_SUCCESS ) {
        return; // Failed to get the master port?
    }
    
    match = IOServiceMatching("IODVDServices");
    if( IOServiceGetMatchingServices(master_port, match, &services) != kIOReturnSuccess ) {
        return;
    }
    
    while( (object = IOIteratorNext(services)) != 0 ) {
        CFMutableDictionaryRef props = 0;
        if( IORegistryEntryCreateCFProperties(object, &props, kCFAllocatorDefault, kNilOptions) == KERN_SUCCESS ) {
            CFDictionaryRef dict = 
                (CFDictionaryRef)CFDictionaryGetValue(props, CFSTR(kIOPropertyDeviceCharacteristicsKey));
            if( dict ) {
                /* The vendor name. */
                char vendor[256] = "", product[256] = "";
                CFTypeRef value = CFDictionaryGetValue(dict, CFSTR(kIOPropertyVendorNameKey));
                if( value && CFGetTypeID(value) == CFStringGetTypeID() ) {
                    CFStringGetCString( (CFStringRef)value, vendor, sizeof(vendor), 
                                        kCFStringEncodingUTF8 );
                }
                value = CFDictionaryGetValue(dict, CFSTR(kIOPropertyProductNameKey));
                if ( value && CFGetTypeID(value) == CFStringGetTypeID() ) {
                    CFStringGetCString( (CFStringRef)value, product, sizeof(product), 
                                        kCFStringEncodingUTF8 );
                }
                if( callback(object, vendor, product, user_data) ) {
                    CFRelease(props);
                    IOObjectRelease(services);
                    return object;
                }
            }
            CFRelease(props);
        }
        
        IOObjectRelease(object);
    }
    IOObjectRelease(services);
}

io_object_t get_cdrom_by_service_path( const gchar *path )
{
    mach_port_t master_port;
    io_object_t result;

    if( IOMasterPort( MACH_PORT_NULL, &master_port ) != KERN_SUCCESS ) {
        return MACH_PORT_NULL; // Failed to get the master port?
    }
    
    return IORegistryEntryFromPath( master_port, path );
}

void cdrom_osx_destroy( gdrom_disc_t disc )
{
    osx_cdrom_disc_t cdrom = (osx_cdrom_disc_t)disc;
    
    if( cdrom->scsi != NULL ) {
        (*cdrom->scsi)->Release(cdrom->scsi);
        cdrom->scsi = NULL;
    }
    if( cdrom->mmc != NULL ) {
        (*cdrom->mmc)->Release(cdrom->mmc);
        cdrom->mmc = NULL;
    }
    if( cdrom->entry != MACH_PORT_NULL ) {
        IOObjectRelease( cdrom->entry );
        cdrom->entry = MACH_PORT_NULL;
    }
    
    if( disc->name != NULL ) {
        g_free( (gpointer)disc->name );
        disc->name = NULL;
    }
    g_free(disc);
}

gdrom_disc_t cdrom_osx_new( io_object_t obj )
{
    osx_cdrom_disc_t cdrom = g_malloc0( sizeof(struct osx_cdrom_disc) );
    gdrom_disc_t disc = &cdrom->disc;
    IOCFPlugInInterface **pluginInterface = NULL;
    HRESULT herr;
    SInt32 score = 0;
    
    cdrom->entry = obj;
    
    if( IOCreatePlugInInterfaceForService( obj, kIOMMCDeviceUserClientTypeID,
            kIOCFPlugInInterfaceID, &pluginInterface, &score ) != KERN_SUCCESS ) {
        ERROR( "Failed to create plugin interface" );
        cdrom_osx_destroy(disc);
        return NULL;
    }
    
    herr = (*pluginInterface)->QueryInterface( pluginInterface,
            CFUUIDGetUUIDBytes( kIOMMCDeviceInterfaceID ), (LPVOID *)&cdrom->mmc );
    (*pluginInterface)->Release(pluginInterface);
    pluginInterface = NULL;
    if( herr != S_OK ) {
        ERROR( "Failed to create MMC interface" );
        cdrom_osx_destroy(disc);
        return NULL;
    }
    
    cdrom->scsi = (*cdrom->mmc)->GetSCSITaskDeviceInterface( cdrom->mmc );
    if( cdrom->scsi == NULL ) {
        ERROR( "Failed to create SCSI interface" );
        cdrom_osx_destroy(disc);
        return NULL;
    }
    
    char name[sizeof(io_string_t) + 6] = "dvd://";
    IORegistryEntryGetPath( cdrom->entry, kIOServicePlane, name+6 );
    disc->name = g_strdup(name);
    
    disc->close = cdrom_osx_destroy;
    return disc;
}

gboolean cdrom_enum_callback( io_object_t object, char *vendor, char *product, void *ptr )
{
    GList **list = (GList **)ptr;
    char tmp1[sizeof(io_string_t) + 6] = "dvd://";
    char tmp2[512];
    IORegistryEntryGetPath( object, kIOServicePlane, tmp1+6 );
    snprintf( tmp2, sizeof(tmp2), "%s %s", vendor, product );
    *list = g_list_append( *list, gdrom_device_new( tmp1, tmp2 ) );
    return FALSE;
}

GList *cdrom_get_native_devices(void)
{
    GList *list = NULL;
    find_cdrom_device(cdrom_enum_callback, &list);
    return list;
}

gdrom_disc_t cdrom_open_device( const gchar *method, const gchar *path )
{
    io_object_t obj;
    gdrom_disc_t result = NULL;
    
    if( strncasecmp( path, "IOService:", 10 ) == 0 ) {
        obj = get_cdrom_by_service_path( path );
    }
    
    if( obj == MACH_PORT_NULL ) {
        return NULL;
    } else {
        return cdrom_osx_new( obj );
    }
}



static gboolean osx_image_is_valid( FILE *f )
{
    return FALSE;
}

static gdrom_disc_t osx_open_device( const gchar *filename, FILE *f )
{
    return NULL;
}
