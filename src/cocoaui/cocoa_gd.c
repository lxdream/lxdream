/**
 * $Id$
 *
 * Management of the GDRom menu under cocoa
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


#include <AppKit/AppKit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "lxdream.h"
#include "dreamcast.h"
#include "dream.h"
#include "gdlist.h"
#include "cocoaui/cocoaui.h"

void cocoa_gdrom_menu_build( NSMenu *menu )
{
    int i,len = gdrom_list_size();
    for( i=0; i<len; i++ ) {
        const gchar *entry = gdrom_list_get_display_name(i);
        if( entry[0] == '\0' ) {
            [menu addItem: [NSMenuItem separatorItem]];
        } else {
            [[menu addItemWithTitle: [NSString stringWithCString: entry] 
                  action: @selector(gdrom_list_action:) keyEquivalent: @""]
                  setTag: i];
        }
    }
    [menu addItem: [NSMenuItem separatorItem]];
    [menu addItemWithTitle: NS_("Open image file...") action: @selector(mount_action:)
       keyEquivalent: @"i"];
}
    
void cocoa_gdrom_menu_rebuild( NSMenu *menu )
{
    while( [menu numberOfItems] > 0  ) {
        [ menu removeItemAtIndex: 0 ];
    }
    
    cocoa_gdrom_menu_build( menu );
}

void cocoa_gdrom_menu_update( gboolean list_changed, int selection, void *user_data )
{
    // Create an auto-release pool - we may be called outside of the GUI main loop
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSMenu *menu = (NSMenu *)user_data;
    int i;
    
    if( list_changed ) {
        cocoa_gdrom_menu_rebuild(menu);
    }
    
    for( i=0; i< [menu numberOfItems]; i++ ) {
        if( i == selection ) {
            [[menu itemAtIndex: i] setState: NSOnState];
        } else {
            [[menu itemAtIndex: i] setState: NSOffState];
        }
    }
    [pool release];
}

NSMenu *cocoa_gdrom_menu_new()
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle: @"GD-Rom Settings"];
    cocoa_gdrom_menu_build(menu);
    
    register_gdrom_list_change_hook(cocoa_gdrom_menu_update, menu);
    cocoa_gdrom_menu_update( FALSE, gdrom_list_get_selection(), menu );    
    return menu;
}
