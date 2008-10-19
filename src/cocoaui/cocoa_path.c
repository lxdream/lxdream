/**
 * $Id$
 *
 * Construct and manage the paths configuration pane
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

@interface LxdreamPrefsPathPane: LxdreamPrefsPane 
{
}
+ (LxdreamPrefsPathPane *)new;
@end

@implementation LxdreamPrefsPathPane
+ (LxdreamPrefsPathPane *)new
{
    return [[LxdreamPrefsPathPane alloc] initWithFrame: NSMakeRect(0,0,600,400)];
}
- (id)initWithFrame: (NSRect)frameRect
{
    if( [super initWithFrame: frameRect title: NS_("Paths")] == nil ) {
        return nil;
    } else {
        int i;
        int height = [self contentHeight] - TEXT_HEIGHT - TEXT_GAP;
        
        for( i=0; i<=CONFIG_KEY_MAX; i++ ) {
            const struct lxdream_config_entry *entry = lxdream_get_config_entry(i);
            if( entry->label != NULL ) {
                NSRect frame = NSMakeRect( TEXT_GAP, height -((TEXT_HEIGHT+TEXT_GAP)*i - 2), 
                                           150, LABEL_HEIGHT );
                NSTextField *label = cocoa_gui_add_label(self, NS_(entry->label), frame);
                [label setAlignment: NSRightTextAlignment];

                frame = NSMakeRect( 150 + (TEXT_GAP*2), 
                                    height -((TEXT_HEIGHT+TEXT_GAP)*i), 
                                    360, TEXT_HEIGHT ); 
                NSTextField *field = [[NSTextField alloc] initWithFrame: frame];
                [field setTag: i];
                [field setStringValue: [NSString stringWithCString: entry->value]]; 
                [field setDelegate: self];
                [field setAutoresizingMask: (NSViewMinYMargin|NSViewWidthSizable)];
                [self addSubview: label];
                [self addSubview: field];
            }
        }
    }
    return self;
}
- (void)controlTextDidEndEditing:(NSNotification *)notify
{
    int tag = [[notify object] tag];
    const char *str = [[[notify object] stringValue] UTF8String];
    const char *oldval = lxdream_get_config_value(tag);
    if( str[0] == '\0' )
        str = NULL;
    if( oldval == NULL ? str != NULL : (str == NULL || strcmp(oldval,str) != 0 ) ) {   
        lxdream_set_global_config_value(tag, str);
        lxdream_save_config();
        dreamcast_config_changed();
    }
}
@end


NSView *cocoa_gui_create_prefs_path_pane()
{
    return [LxdreamPrefsPathPane new];
}
