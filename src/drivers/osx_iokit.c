/**
 * $Id$
 *
 * OSX support functions for handling the IOKit registry. 
 * Currently this manages access to CD/DVD drives + media, plus HID devices.
 * 
 * The HID part is much simpler...
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

#include <glib/gmem.h>
#include <glib/gstrfuncs.h>
#include <sys/param.h>
#include <paths.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <CoreFoundation/CFRunLoop.h>
#include <IOKit/IOMessage.h>
#include <IOKit/hid/IOHIDLib.h>
#include "osx_iokit.h"



static IONotificationPortRef notify_port = 0;
static io_iterator_t iokit_iterators[4] = {0,0,0,0};

struct osx_cdrom_drive {
    io_string_t ioservice_path;
    io_string_t vendor_name;
    io_string_t product_name;
    char media_path[MAXPATHLEN]; // BSD device path if media present, otherwise the empty string.
    io_iterator_t media_load_iterator;
    io_iterator_t media_unload_iterator;
    int media_fh; // BSD device handle if open, otherwise -1
    media_changed_callback_t media_changed;
    void *media_changed_user_data;
};

static gboolean get_bsdname_for_iomedia( io_object_t iomedia, char *buf, int buflen );

/***************** IOKit Callbacks ******************/

/**
 * Called from IOKit for any IOMessages on an IOMedia. Currently the only message
 * we're interested in is service termination.
 */
static void osx_cdrom_media_notify( void *ref, io_service_t service, uint32_t msgType,
                                    void *msgArgument )
{
    if( msgType == kIOMessageServiceIsTerminated ) {
        osx_cdrom_drive_t drive = (osx_cdrom_drive_t)ref;
        if( drive->media_changed != NULL ) {
            drive->media_changed( drive, FALSE, drive->media_changed_user_data );
        }
        if( drive->media_fh != -1 ) {
            close(drive->media_fh);
            drive->media_fh = -1;
        }
        drive->media_path[0] = '\0';
        IOObjectRelease( drive->media_unload_iterator );
    }
}

/**
 * Called from IOKit when an IOMedia is inserted that we have be interested in.
 * FIXME: Can the matcher be restricted to descendents of the drive node? currently
 * we watch for all IOMedia events and compare the device path to see if it's one we
 * care about.
 * FIXME: We assume for now that a drive has at most one piece of media at a time. 
 * If this isn't the case, the system may get a little confused.
 */
static void osx_cdrom_media_inserted( void *ref, io_iterator_t iterator )
{
    osx_cdrom_drive_t drive = (osx_cdrom_drive_t)ref;

    io_object_t object;
    while( (object = IOIteratorNext(iterator)) != 0 ) {
        io_string_t iopath = "";
        IORegistryEntryGetPath( object, kIOServicePlane, iopath );
        if( drive != NULL && g_str_has_prefix(iopath, drive->ioservice_path ) &&
                get_bsdname_for_iomedia(object, drive->media_path, sizeof(drive->media_path)) ) {
            // A disc was inserted within the drive of interest
            if( drive->media_fh != -1 ) {
                close(drive->media_fh);
                drive->media_fh = -1;
            }

            if( drive->media_changed != NULL ) {
                drive->media_changed(drive, TRUE, drive->media_changed_user_data);
            }
            // Add a notification listener to get removal events.
            IOServiceAddInterestNotification( notify_port, object, kIOGeneralInterest, 
                                              osx_cdrom_media_notify, drive, &drive->media_unload_iterator ); 

        }
        IOObjectRelease( object );
    }
}

static void osx_drives_changed( void *ref, io_iterator_t iterator )
{
    io_object_t object;
    while( (object = IOIteratorNext(iterator)) != 0 ) {
        IOObjectRelease(object);
    }

}

/******************** Support functions *********************/

/**
 * Determine the BSD device name (ie "/dev/rdisk1") for a given IO object.
 * @return TRUE if the device name was retrieved, FALSE if the request failed.
 */
static gboolean get_bsdname_for_iomedia( io_object_t iomedia, char *buf, int buflen )
{
    gboolean result = FALSE;
    CFTypeRef pathRef = IORegistryEntryCreateCFProperty(iomedia, CFSTR(kIOBSDNameKey),
            kCFAllocatorDefault, 0 );
    if( pathRef ) {
        char pathlen;
        strcpy( buf, _PATH_DEV "r" );
        pathlen = strlen(buf);
        if( CFStringGetCString( pathRef, buf + pathlen, buflen-pathlen,
                kCFStringEncodingASCII ) != noErr ) {
            result = TRUE;
        }
        CFRelease(pathRef);
    }
    return result;
}

static gboolean osx_cdrom_drive_get_name( io_object_t object, char *vendor, int vendor_len,
                                          char *product, int product_len )
{
    gboolean result = FALSE;
    CFMutableDictionaryRef props = 0;
    if( IORegistryEntryCreateCFProperties(object, &props, kCFAllocatorDefault, kNilOptions) == KERN_SUCCESS ) {
        CFDictionaryRef dict = 
            (CFDictionaryRef)CFDictionaryGetValue(props, CFSTR(kIOPropertyDeviceCharacteristicsKey));
        if( dict != NULL ) {
            CFTypeRef value = CFDictionaryGetValue(dict, CFSTR(kIOPropertyVendorNameKey));
            if( value && CFGetTypeID(value) == CFStringGetTypeID() ) {
                CFStringGetCString( (CFStringRef)value, vendor, vendor_len, kCFStringEncodingUTF8 );
            } else {
                vendor[0] = 0;
            }

            value = CFDictionaryGetValue(dict, CFSTR(kIOPropertyProductNameKey));
            if ( value && CFGetTypeID(value) == CFStringGetTypeID() ) {
                CFStringGetCString( (CFStringRef)value, product, product_len, kCFStringEncodingUTF8 );
            } else {
                product[0] = 0;
            }
            result = TRUE;
        }

        CFRelease(props);
    }
    return result;
}

/**
 * Construct and initialize a new osx_cdrom_drive object, including registering
 * it's media inserted notification.
 */
static osx_cdrom_drive_t osx_cdrom_drive_new( io_object_t device )  
{
    osx_cdrom_drive_t drive = g_malloc0(sizeof(struct osx_cdrom_drive));

    IORegistryEntryGetPath( device, kIOServicePlane, drive->ioservice_path );
    osx_cdrom_drive_get_name( device, drive->vendor_name, sizeof(drive->vendor_name),
                              drive->product_name, sizeof(drive->product_name) );
    drive->media_path[0] = '\0';
    drive->media_changed = NULL;
    drive->media_changed_user_data = NULL;
    drive->media_fh = -1;

    IOServiceAddMatchingNotification( notify_port, kIOFirstPublishNotification, 
                                      IOServiceMatching("IOMedia"),
                                      osx_cdrom_media_inserted, drive, 
                                      &drive->media_load_iterator );
    osx_cdrom_media_inserted( drive, drive->media_load_iterator );
    return drive;
}

/************************ Exported functions *************************/ 

osx_cdrom_drive_t osx_cdrom_open_drive( const char *devname )
{
    io_object_t object = IORegistryEntryFromPath( kIOMasterPortDefault, devname );
    if( object == MACH_PORT_NULL ) {
        return NULL;
    }

    osx_cdrom_drive_t drive = osx_cdrom_drive_new( object );
    IOObjectRelease( object );
    return drive;
}

void osx_cdrom_set_media_changed_callback( osx_cdrom_drive_t drive, 
                                           media_changed_callback_t callback,
                                           void *user_data )
{
    drive->media_changed = callback;
    drive->media_changed_user_data = user_data;
}

void osx_cdrom_close_drive( osx_cdrom_drive_t drive )
{
    IOObjectRelease( drive->media_load_iterator );
    IOObjectRelease( drive->media_unload_iterator );
    if( drive->media_fh != -1 ) {
        close(drive->media_fh);
        drive->media_fh = -1;
    }
    g_free( drive );
}

int osx_cdrom_get_media_handle( osx_cdrom_drive_t drive )
{
    if( drive->media_fh == -1 ) {
        if( drive->media_path[0] != '\0' ) {
            drive->media_fh = open( drive->media_path, O_RDONLY|O_NONBLOCK );
        }
    }
    return drive->media_fh;
}

void osx_cdrom_release_media_handle( osx_cdrom_drive_t drive )
{
    if( drive->media_fh != -1 ) {
        close( drive->media_fh );
        drive->media_fh = -1; 
    }
}

static io_object_t iterator_find_cdrom( io_object_t iterator, find_drive_callback_t callback, void *user_data )
{
    io_object_t object;
    while( (object = IOIteratorNext(iterator)) != 0 ) {
        io_string_t iopath = "";
        char product[256], vendor[256];
        IORegistryEntryGetPath( object, kIOServicePlane, iopath );
        osx_cdrom_drive_get_name( object, vendor, sizeof(vendor), product, sizeof(product) );
        if( callback( object, vendor, product, iopath, user_data ) ) {
            IOObjectRelease(iterator);
            return object;
        }
        IOObjectRelease(object);
    }
    IOObjectRelease(iterator);
    return 0;
}


/**
 * Search for a CD or DVD drive (instance of IODVDServices or IOCompactDiscServices).
 * The callback will be called repeatedly until either it returns TRUE, or all drives
 * have been iterated over.
 * 
 * @return an IO registry entry for the matched drive, or 0 if no drives matched.
 * 
 * Note: Use of IOCompactDiscServices is somewhat tentative since I don't have a Mac
 * with a CD-Rom drive.
 */ 
io_object_t find_cdrom_drive( find_drive_callback_t callback, void *user_data )
{
    mach_port_t master_port;
    CFMutableDictionaryRef match;
    io_iterator_t services;
    io_object_t result;

    if( IOMasterPort( MACH_PORT_NULL, &master_port ) != KERN_SUCCESS ) {
        return 0; // Failed to get the master port?
    }

    match = IOServiceMatching("IODVDServices");
    if( IOServiceGetMatchingServices(master_port, match, &services) != kIOReturnSuccess ) {
        return 0;
    }

    result = iterator_find_cdrom( services, callback, user_data );
    if( result != 0 ) {
        return result;
    }

    match = IOServiceMatching("IOCompactDiscServices");
    if( IOServiceGetMatchingServices(master_port, match, &services) != kIOReturnSuccess ) {
        return 0;
    }
    return iterator_find_cdrom( services, callback, user_data );
}


// *********************** Notification management ************************/

static void osx_hid_inserted( void *ref, io_iterator_t iterator )
{
    io_object_t object;
    while( (object = IOIteratorNext(iterator)) != 0 ) {
        io_string_t iopath = "";
        IORegistryEntryGetPath( object, kIOServicePlane, iopath );
        IOObjectRelease( object );
    }
}

gboolean osx_register_iokit_notifications()
{
    notify_port = IONotificationPortCreate( kIOMasterPortDefault );
    CFRunLoopSourceRef runloop_source = IONotificationPortGetRunLoopSource( notify_port );
    CFRunLoopAddSource( CFRunLoopGetCurrent(), runloop_source, kCFRunLoopCommonModes );

    // Drive notifications
    if( IOServiceAddMatchingNotification( notify_port, kIOFirstPublishNotification,
            IOServiceMatching("IOCompactDiscServies"),
            osx_drives_changed, NULL, &iokit_iterators[0] ) != kIOReturnSuccess ) {
        ERROR( "IOServiceAddMatchingNotification failed" );
    }
    osx_drives_changed(NULL, iokit_iterators[0]);
    if( IOServiceAddMatchingNotification( notify_port, kIOFirstPublishNotification,
            IOServiceMatching("IODVDServies"),
            osx_drives_changed, NULL, &iokit_iterators[1] ) != kIOReturnSuccess ) {
        ERROR( "IOServiceAddMatchingNotification failed" );
    }
    osx_drives_changed(NULL, iokit_iterators[1]);

    if( IOServiceAddMatchingNotification( notify_port, kIOFirstPublishNotification, 
            IOServiceMatching(kIOHIDDeviceKey),
            osx_hid_inserted, NULL, &iokit_iterators[2] ) != kIOReturnSuccess ) {
        ERROR( "IOServiceAddMatchingNotification failed" );
    }
    osx_hid_inserted(NULL, iokit_iterators[2]);
    return TRUE;
}

void osx_unregister_iokit_notifications()
{
    CFRunLoopSourceRef runloop_source = IONotificationPortGetRunLoopSource( notify_port );
    CFRunLoopRemoveSource( CFRunLoopGetCurrent(), runloop_source, kCFRunLoopCommonModes );
    IONotificationPortDestroy( notify_port );
    notify_port = 0;
}
