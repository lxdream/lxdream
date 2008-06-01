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

#import <AppKit/AppKit.h>
#include <glib/gi18n.h>

#ifndef lxdream_cocoaui_H
#define lxdream_cocoaui_H

#define NS_(x) [NSString stringWithUTF8String: _(x)]

NSWindow *cocoa_gui_create_main_window();
NSView *video_osx_create_drawable();

@interface LxdreamMainWindow : NSWindow 
{
    NSView *video;
    NSTextField *status;
    BOOL isGrabbed;
}
- (id)initWithContentRect:(NSRect)contentRect;
- (void)setStatusText:(const gchar *)text;
- (void)setRunning:(BOOL)isRunning;
- (BOOL)isGrabbed;
- (void)setIsGrabbed:(BOOL)grab;
@end


#endif /* lxdream_cocoaui_H */