/**
 * $Id$
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

static LxdreamPrefsPanel *prefs_panel = NULL;

@implementation LxdreamPrefsPane
- (NSTextField *)addLabel: (NSString *)text withFrame: (NSRect)frame
{
    NSTextField *label = [[NSTextField alloc] initWithFrame: frame];
    [label setStringValue: text];
    [label setBordered: NO];
    [label setDrawsBackground: NO];
    [label setEditable: NO];
    [label setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
    [self addSubview: label];
    return label;
}
- (int)contentHeight
{
    return [self frame].size.height - headerHeight;
}

- (id)initWithFrame: (NSRect)frameRect title:(NSString *)title
{
    if( [super initWithFrame: frameRect ] == nil ) {
        return nil;
    } else {
        int height = frameRect.size.height - TEXT_GAP;
        
        NSFont *titleFont = [NSFont fontWithName: @"Helvetica-Bold" size: 16.0];
        NSRect fontRect = [titleFont boundingRectForFont];
        int titleHeight = fontRect.size.height + [titleFont descender];
        NSTextField *label = [self addLabel: title withFrame: 
            NSMakeRect( TEXT_GAP, height-titleHeight, 
                        frameRect.size.width - (TEXT_GAP*2), titleHeight )];
        [label setFont: titleFont];
        height -= (titleHeight + TEXT_GAP);
        
        NSBox *rule = [[NSBox alloc] initWithFrame: NSMakeRect(1, height, frameRect.size.width-2, 1)];
        [rule setAutoresizingMask: (NSViewMinYMargin|NSViewWidthSizable)];
        [rule setBoxType: NSBoxSeparator];
        [self addSubview: rule];
        height -= TEXT_GAP;
      
        headerHeight = frameRect.size.height - height;
        return self;
    }
}
@end


@interface LxdreamPrefsPanel (Private)
- (void) initToolbar;
- (NSToolbarItem *) createToolbarItem: (NSString *)id label: (NSString *) label 
tooltip: (NSString *)tooltip icon: (NSString *)icon action: (SEL) action;
@end

@implementation LxdreamPrefsPanel

- (NSView *)createControlsPane
{
    NSView *pane = [[NSView alloc] initWithFrame: NSMakeRect(0,0,600,400)];
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
        [self initToolbar];
        path_pane = [LxdreamPrefsPathPane new];
        ctrl_pane = [LxdreamPrefsControllerPane new];
        
        [self setContentView: path_pane];
        return self;
    }
}
- (void)windowWillClose: (NSNotification *)notice
{
    prefs_panel = NULL;
}
- (void) initToolbar
{
    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier: @"LxdreamPrefsToolbar"];

    NSToolbarItem *paths = [self createToolbarItem: @"Paths" label: @"Paths" 
                            tooltip: @"Configure system paths" icon: @"tb-paths" 
                            action: @selector(paths_action:)];
    NSToolbarItem *ctrls = [self createToolbarItem: @"Controllers" label: @"Controllers"
                            tooltip: @"Configure controllers" icon: @"tb-ctrls"
                            action: @selector(controllers_action:)];
    toolbar_ids = [NSArray arrayWithObjects: @"Paths", @"Controllers", nil ];
    toolbar_defaults = [NSArray arrayWithObjects: @"Paths", @"Controllers", nil ]; 
    NSArray *values = [NSArray arrayWithObjects: paths, ctrls, nil ];
    toolbar_items = [NSDictionary dictionaryWithObjects: values forKeys: toolbar_ids];

    [toolbar setDelegate: self];
    [toolbar setDisplayMode: NSToolbarDisplayModeIconOnly];
    [toolbar setSizeMode: NSToolbarSizeModeSmall];
    [toolbar setSelectedItemIdentifier: @"Paths"];
    [self setToolbar: toolbar];
}

- (void)paths_action: (id)sender
{
    [self setContentView: path_pane];
}
- (void)controllers_action: (id)sender
{
    [self setContentView: ctrl_pane];
}

/***************************** Toolbar methods ***************************/
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
    return toolbar_ids;
}

- (NSArray *) toolbarDefaultItemIdentifiers: (NSToolbar *)toolbar
{
    return toolbar_defaults;
}

- (NSArray *)toolbarSelectableItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: @"Paths", @"Controllers", nil ]; 
}

- (NSToolbarItem *) toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier
willBeInsertedIntoToolbar:(BOOL)flag 
{
    return [toolbar_items objectForKey: itemIdentifier];
}
@end

void cocoa_gui_show_preferences() 
{
    if( prefs_panel == NULL ) {
        prefs_panel = [[LxdreamPrefsPanel alloc] initWithContentRect: NSMakeRect(0,0,600,400)];
    }
    [prefs_panel makeKeyAndOrderFront: prefs_panel];
}