/**
 * $Id$
 *
 * Construct and maintain the main window under cocoa.
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
#include <ApplicationServices/ApplicationServices.h>

#define STATUSBAR_HEIGHT 20

@interface LxdreamToolbarDelegate : NSObject {
    NSArray *identifiers;
    NSArray *defaults;
    NSDictionary *items;
}
- (NSToolbarItem *) createToolbarItem: (NSString *)id label: (NSString *) label 
    tooltip: (NSString *)tooltip icon: (NSString *)icon action: (SEL) action; 
@end

@implementation LxdreamToolbarDelegate
- (id) init
{
    NSToolbarItem *mount = [self createToolbarItem: @"GdromMount" label: @"Open Image" 
                            tooltip: @"Mount a cdrom disc" icon: @"tb-cdrom" 
                            action: @selector(mount_action:)];
    NSToolbarItem *reset = [self createToolbarItem: @"Reset" label: @"Reset"
                            tooltip: @"Reset dreamcast" icon: @"tb-reset"
                            action: @selector(reset_action:)];
    NSToolbarItem *pause = [self createToolbarItem: @"Pause" label: @"Pause"
                            tooltip: @"Pause dreamcast" icon: @"tb-pause"
                            action: @selector(pause_action:)];
    NSToolbarItem *run = [self createToolbarItem: @"Run" label: @"Resume"
                            tooltip: @"Resume" icon: @"tb-run"
                            action: @selector(run_action:)];
    NSToolbarItem *load = [self createToolbarItem: @"LoadState" label: @"Load State..."
                            tooltip: @"Load an lxdream save state" icon: @"tb-load"
                            action: @selector(load_action:)];
    NSToolbarItem *save = [self createToolbarItem: @"SaveState" label: @"Save State..."
                            tooltip: @"Create an lxdream save state" icon: @"tb-save"
                            action: @selector(save_action:)];
    [pause setEnabled: NO];
    identifiers = 
        [NSArray arrayWithObjects: @"GdromMount", @"Reset", @"Pause", @"Run", @"LoadState", @"SaveState", nil ];
    defaults = 
        [NSArray arrayWithObjects: @"GdromMount", @"Reset", @"Pause", @"Run", 
                     NSToolbarSeparatorItemIdentifier, @"LoadState", @"SaveState", nil ];
    NSArray *values = [NSArray arrayWithObjects: mount, reset, pause, run, load, save, nil ];
    items = [NSDictionary dictionaryWithObjects: values forKeys: identifiers];
    return self;
}

- (NSToolbarItem *) createToolbarItem: (NSString *)id label: (NSString *) label 
    tooltip: (NSString *)tooltip icon: (NSString *)icon action: (SEL) action 
{
    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier: id];
    [item setLabel: label];
    [item setToolTip: tooltip];
    [item setTarget: [NSApp delegate]];
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
    return [NSArray arrayWithObjects: @"Pause", @"Run", nil];
}

- (NSToolbarItem *) toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier
     willBeInsertedIntoToolbar:(BOOL)flag 
{
     return [items objectForKey: itemIdentifier];
}
@end

@implementation LxdreamMainWindow
- (id)initWithContentRect:(NSRect)videoRect 
{
    NSRect contentRect = NSMakeRect(videoRect.origin.x,videoRect.origin.y,
            videoRect.size.width,videoRect.size.height+STATUSBAR_HEIGHT);
    if( [super initWithContentRect: contentRect
           styleMask: ( NSTitledWindowMask | NSClosableWindowMask | 
               NSMiniaturizableWindowMask | NSResizableWindowMask |
               NSTexturedBackgroundWindowMask | NSUnifiedTitleAndToolbarWindowMask )
           backing: NSBackingStoreBuffered defer: NO ] == nil ) {
        return nil;
    } else {
        isGrabbed = NO;
        video = video_osx_create_drawable();
        [video setFrameOrigin: NSMakePoint(0.0,STATUSBAR_HEIGHT)];
        
        status = 
            [[NSTextField alloc] initWithFrame: NSMakeRect(0.0,0.0,videoRect.size.width,STATUSBAR_HEIGHT)];
        [status setStringValue: @"Idle"];
        [status setEditable: NO];
        [status setDrawsBackground: NO];
        [status setBordered: NO];
        [[self contentView] addSubview: video];
        [[self contentView] addSubview: status];
        [self makeFirstResponder: video];

        [self setAutorecalculatesContentBorderThickness: YES forEdge: NSMinYEdge ];
        [self setContentBorderThickness: 15.0 forEdge: NSMinYEdge];

        // Share the app delegate for the purposes of keeping it in one place
        [self setDelegate: [NSApp delegate]];
        [self setContentMinSize: contentRect.size];
        [self setAcceptsMouseMovedEvents: YES];

        NSString *title = [[NSString alloc] initWithCString: (APP_NAME " " APP_VERSION) encoding: NSASCIIStringEncoding]; 
        [self setTitle: title];

        NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier: @"LxdreamToolbar"];
        [toolbar setDelegate: [[LxdreamToolbarDelegate alloc] init]];
        [toolbar setDisplayMode: NSToolbarDisplayModeIconOnly];
        [toolbar setSizeMode: NSToolbarSizeModeSmall];
        [toolbar setSelectedItemIdentifier: @"Pause"];
        [self setToolbar: toolbar];
        return self;
    }
}

- (void)setStatusText: (const gchar *)text
{
    if( isGrabbed ) {
        gchar buf[128];
        snprintf( buf, sizeof(buf), "%s %s", text, _("(Press <ctrl><alt> to release grab)") );
        NSString *s = [NSString stringWithUTF8String: buf]; 
        [status setStringValue: s];
    } else {
        NSString *s = [NSString stringWithUTF8String: text];
        [status setStringValue: s];
    }   
}
- (void)setRunning:(BOOL)isRunning
{
    if( isRunning ) {
        [[self toolbar] setSelectedItemIdentifier: @"Run"];
        [self setStatusText: _("Running")];
    } else {
        [[self toolbar] setSelectedItemIdentifier: @"Pause"];
        [self setStatusText: _("Stopped")];
    }            
}
- (BOOL)isGrabbed
{
    return isGrabbed;
}
- (void)setIsGrabbed:(BOOL)grab
{
    if( grab != isGrabbed ) {
        isGrabbed = grab;
        [self setRunning: dreamcast_is_running() ? YES : NO];
    }
}

@end

NSWindow *cocoa_gui_create_main_window()
{
   NSRect contentRect = {{0,0},{640,480}};
   NSWindow *main_win = [[LxdreamMainWindow alloc] initWithContentRect: contentRect]; 
   return main_win;
}
