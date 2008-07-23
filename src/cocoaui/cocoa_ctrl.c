/**
 * $Id$
 *
 * Construct and manage the controller configuration pane
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

#include "cocoaui.h"
#include "config.h"
#include "maple/maple.h"

#define MAX_DEVICES 4

@implementation LxdreamPrefsControllerPane
+ (LxdreamPrefsControllerPane *)new
{
    return [[LxdreamPrefsControllerPane alloc] initWithFrame: NSMakeRect(0,0,600,400)];
}
- (id)initWithFrame: (NSRect)frameRect
{
    if( [super initWithFrame: frameRect title: NS_("Controllers")] == nil ) {
        return nil;
    } else {
        const struct maple_device_class **devices = maple_get_device_classes();
        char buf[16];
        int i,j;
        int height = [self contentHeight] - TEXT_HEIGHT - TEXT_GAP;
        for( i=0; i<MAX_DEVICES; i++ ) {
            save_controller[i] = NULL;
            maple_device_t device = maple_get_device(i,0);
            NSRect frame = NSMakeRect( TEXT_GAP, height -((TEXT_HEIGHT+TEXT_GAP)*i - 2),
                                       50, LABEL_HEIGHT );
            snprintf( buf, sizeof(buf), _("Slot %d."), i );
            NSTextField *label = [self addLabel: [NSString stringWithUTF8String: buf]
                                       withFrame: frame ];
            [label setAlignment: NSRightTextAlignment];
            frame = NSMakeRect( 50 + (TEXT_GAP*2), height - ((TEXT_HEIGHT+TEXT_GAP)*i),
                                150, TEXT_HEIGHT );
            NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame: frame pullsDown: NO];
            [popup addItemWithTitle: NS_("<empty>")];
            [popup setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
            [[popup itemAtIndex: 0] setTag: 0];
            for( j=0; devices[j] != NULL; j++ ) {
                [popup addItemWithTitle: [NSString stringWithUTF8String: devices[j]->name]];
                if( device != NULL && device->device_class == devices[j] ) {
                    [popup selectItemAtIndex: (j+1)];
                }
                [[popup itemAtIndex: (j+1)] setTag: (j+1)];
            }
            [popup setTarget: self];
            [popup setAction: @selector(deviceChanged:)];
            [popup setTag: i];
            [self addSubview: popup];
        }
        return self;
    }
}
- (void)deviceChanged: (id)sender
{
    int slot = [sender tag];
    int new_device_idx = [sender indexOfSelectedItem] - 1; 
    maple_device_class_t new_device_class = NULL;
    
    maple_device_t current = maple_get_device(slot,0);
    maple_device_t new_device = NULL;
    if( new_device_idx != -1 ) {
        new_device_class = maple_get_device_classes()[new_device_idx];
    }
    if( current == NULL ? new_device_class == NULL : current->device_class == new_device_class ) {
        // No change
        return;
    }
    if( current != NULL && current->device_class == &controller_class ) {
        save_controller[slot] = current->clone(current);
    }
    if( new_device_class == NULL ) {
        maple_detach_device(slot,0);
    } else {
        if( new_device_class == &controller_class && save_controller[slot] != NULL ) {
            new_device = save_controller[slot];
            save_controller[slot] = NULL;
        } else {
            new_device = maple_new_device( new_device_class->name );
        }
        maple_attach_device(new_device,slot,0);
    }

    lxdream_save_config();
}
@end
