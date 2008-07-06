/**
 * $Id: cocoa_win.c 723 2008-06-25 00:39:02Z nkeynes $
 *
 * Construct and manage the preferences panel under cocoa.
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

#include "cocoaui/cocoaui.h"
#include "lxdream.h"
#include "config.h"

@interface LxdreamPrefsToolbarDelegate : NSObject {
    NSArray *identifiers;
    NSArray *defaults;
    NSDictionary *items;
}
- (NSToolbarItem *) createToolbarItem: (NSString *)id label: (NSString *) label 
    tooltip: (NSString *)tooltip icon: (NSString *)icon action: (SEL) action; 
@end

@implementation LxdreamPrefsToolbarDelegate
- (id) init
{
    NSToolbarItem *paths = [self createToolbarItem: @"Paths" label: @"Paths" 
                            tooltip: @"Configure system paths" icon: @"tb-paths" 
                            action: @selector(paths_action:)];
    NSToolbarItem *ctrls = [self createToolbarItem: @"Controllers" label: @"Controllers"
                            tooltip: @"Configure controllers" icon: @"tb-ctrls"
                            action: @selector(controllers_action:)];
    identifiers = [NSArray arrayWithObjects: @"Paths", @"Controllers", nil ];
    defaults = [NSArray arrayWithObjects: @"Paths", @"Controllers", nil ]; 
    NSArray *values = [NSArray arrayWithObjects: paths, ctrls, nil ];
    items = [NSDictionary dictionaryWithObjects: values forKeys: identifiers];
    return self;
}

- (NSToolbarItem *) createToolbarItem: (NSString *)id label: (NSString *) label 
    tooltip: (NSString *)tooltip icon: (NSString *)icon action: (SEL) action 
{
    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier: id];
    [item setLabel: label];
    [item setToolTip: tooltip];
    [item setTarget: self];
    NSString *iconFile = [[NSBundle mainBundle] pathForResource:icon ofType:@"png"];
    NSImage *image = [[NSImage alloc] initWithContentsOfFile: iconFile];
    [item setImage: image];
    [item setAction: action];
    return item;
}

- (NSArray *) toolbarAllowedItemIdentifiers: (NSToolbar *)toolbar 
{
    return identifiers;
}

- (NSArray *) toolbarDefaultItemIdentifiers: (NSToolbar *)toolbar
{
    return defaults;
}

- (NSArray *)toolbarSelectableItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: @"Paths", @"Controllers", nil];
}

- (NSToolbarItem *) toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier
     willBeInsertedIntoToolbar:(BOOL)flag 
{
     return [items objectForKey: itemIdentifier];
}
- (void)paths_action: (id)sender
{
}
- (void)controllers_action: (id)sender
{
}
@end

@implementation LxdreamPrefsPanel
- (NSView *)createPathsPane
{
    NSView *pane = [NSView new];
    int i;
    for( i=0; i<=CONFIG_KEY_MAX; i++ ) {
        lxdream_config_entry_t entry = lxdream_get_config_entry(i);
        if( entry->label != NULL ) {
        }
    }
    return pane;
}
- (id)initWithContentRect:(NSRect)contentRect 
{
    if( [super initWithContentRect: contentRect
           styleMask: ( NSTitledWindowMask | NSClosableWindowMask | 
               NSMiniaturizableWindowMask | NSResizableWindowMask |
               NSUnifiedTitleAndToolbarWindowMask )
           backing: NSBackingStoreBuffered defer: NO ] == nil ) {
        return nil;
    } else {
        [self setTitle: NS_("Preferences")];
        [self setDelegate: self];
        NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier: @"LxdreamPrefsToolbar"];
        [toolbar setDelegate: [[LxdreamPrefsToolbarDelegate alloc] init]];
        [toolbar setDisplayMode: NSToolbarDisplayModeIconOnly];
        [toolbar setSizeMode: NSToolbarSizeModeSmall];
        [toolbar setSelectedItemIdentifier: @"Paths"];
        [self setToolbar: toolbar];
        [self setContentView: [self createPathsPane]];
        return self;
    }
}
- (void)windowWillClose: (NSNotification *)notice
{
    [NSApp stopModal];
}
@end
