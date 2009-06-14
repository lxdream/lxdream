/**
 * $Id$
 *
 * Core Cocoa-based user interface
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
 
#ifndef lxdream_cocoaui_H
#define lxdream_cocoaui_H

#import <AppKit/AppKit.h>
#include "lxdream.h"
#include "gettext.h"

#ifdef __cplusplus
extern "C" {
#endif
    
#define NS_(x) [NSString stringWithUTF8String: _(x)]

/* Standard sizing */
#define TEXT_HEIGHT 22
#define LABEL_HEIGHT 17
#define TEXT_GAP 10
    
/**
 * Convenience method to create a new text label in the specified parent.
 */
NSTextField *cocoa_gui_add_label(NSView *parent, NSString *title, NSRect frame);

@interface LxdreamVideoView : NSView
{
    BOOL isGrabbed;
    id delegate;
}
- (void) setDelegate: (id)other;
- (id)delegate;
- (void) setIsGrabbed: (BOOL)grabbed;
@end

@interface LxdreamMainWindow : NSWindow 
{
    LxdreamVideoView *video;
    NSTextField *status;
    BOOL isGrabbed;
    BOOL useGrab;
    BOOL isFullscreen;
    NSRect savedFrame;
}
- (id)initWithContentRect:(NSRect)contentRect;
- (void)setStatusText:(const gchar *)text;
- (void)updateTitle;
- (void)setRunning:(BOOL)isRunning;
- (BOOL)isGrabbed;
- (void)setIsGrabbed:(BOOL)grab;
- (void)setUseGrab: (BOOL)grab;
- (BOOL)isFullscreen;
- (void)setFullscreen: (BOOL)fs;
@end

@interface LxdreamPrefsPane : NSView
{
    int headerHeight;
}
- (id)initWithFrame: (NSRect)frameRect title:(NSString *)title;
- (int)contentHeight;
@end

@interface KeyBindingEditor: NSTextView
{
    BOOL isPrimed;
    NSString *lastValue;
}
@end

@interface KeyBindingField : NSTextField
{
}
@end

@interface LxdreamPrefsPanel : NSPanel
{
    NSArray *toolbar_ids;
    NSArray *toolbar_defaults;
    NSDictionary *toolbar_items;
    NSView *path_pane, *ctrl_pane;
    KeyBindingEditor *binding_editor;
}
- (id)initWithContentRect:(NSRect)contentRect;
@end


LxdreamMainWindow *cocoa_gui_create_main_window();
NSMenu *cocoa_gdrom_menu_new();
NSView *video_osx_create_drawable();
void cocoa_gui_show_preferences();
NSView *cocoa_gui_create_prefs_controller_pane();
NSView *cocoa_gui_create_prefs_path_pane();


#ifdef __cplusplus
}
#endif
    
#endif /* lxdream_cocoaui_H */