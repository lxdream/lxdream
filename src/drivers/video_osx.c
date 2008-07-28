/**
 * $Id$
 *
 * The OS/X side of the video support (responsible for actually displaying / 
 * rendering frames)
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

#include <stdlib.h>
#include <string.h>
#include "lxdream.h"
#include "display.h"
#include "dckeysyms.h"
#include "cocoaui/cocoaui.h"
#include "drivers/video_nsgl.h"
#include "drivers/video_gl.h"
#include "pvr2/pvr2.h"
#import <AppKit/AppKit.h>

#include "mac_keymap.h"

#define MOUSE_X_SCALE 5
#define MOUSE_Y_SCALE 5

static gboolean video_osx_init();
static void video_osx_shutdown();
static void video_osx_display_blank( uint32_t colour );
static uint16_t video_osx_resolve_keysym( const gchar *keysym );
static uint16_t video_osx_keycode_to_dckeysym(uint16_t keycode);
static gchar *video_osx_keycode_to_keysym(uint16_t keycode);

struct display_driver display_osx_driver = { 
        "osx",
        N_("OS X Cocoa GUI-based OpenGL driver"),
        video_osx_init, video_osx_shutdown,
        video_osx_resolve_keysym,
        video_osx_keycode_to_dckeysym,
        video_osx_keycode_to_keysym,
        NULL, NULL, NULL, NULL, NULL, 
        video_osx_display_blank, NULL };


static NSView *video_view = NULL;
int video_width = 640;
int video_height = 480;

#define MAX_MASK_KEYCODE 128

@interface LxdreamOSXView : LxdreamVideoView
{
    int buttonMask;
    int flagsMask[MAX_MASK_KEYCODE];
}
@end

@implementation LxdreamVideoView
- (void)setIsGrabbed: (BOOL)grabbed
{
    isGrabbed = grabbed;
}
- (void) setDelegate: (id)other
{
    delegate = other;
}
- (id)delegate 
{
    return delegate;
}
@end

@implementation LxdreamOSXView
//--------------------------------------------------------------------
- (id)initWithFrame: (NSRect)contentRect
{
    if( [super initWithFrame: contentRect] != nil ) {
        int i;
        isGrabbed = NO;
        buttonMask = 0;
        for( i=0; i<MAX_MASK_KEYCODE; i++ ) {
            flagsMask[i] = 0;
        }
        return self;
    }
    return nil;
}
- (void)requestGrab
{
    if( delegate && [delegate respondsToSelector: @selector(viewRequestedGrab:)] )
        [delegate performSelector: @selector(viewRequestedGrab:) withObject: self];
}
- (void)requestUngrab
{
    if( delegate && [delegate respondsToSelector: @selector(viewRequestedUngrab:)] )
        [delegate performSelector: @selector(viewRequestedUngrab:) withObject: self];
}
- (BOOL)isOpaque
{
    return YES;
}
- (BOOL)acceptsFirstResponder
{
    return YES;
}
- (BOOL)isFlipped
{
    return YES;
}
//--------------------------------------------------------------------
- (void)drawRect: (NSRect) rect
{
    NSSize size = [self frame].size;
    if( video_width != size.width || video_height != size.height ) {
        video_width = size.width;
        video_height = size.height;
        video_nsgl_update();
    }
    pvr2_redraw_display();
}
- (void)keyDown: (NSEvent *) event
{
    if( ![event isARepeat] ) {
        input_event_keydown( NULL, [event keyCode]+1, 1 );
    }
}
- (void)keyUp: (NSEvent *) event
{
    input_event_keyup( NULL, [event keyCode]+1, 1 );
}
- (void)flagsChanged: (NSEvent *) event
{
    int keycode = [event keyCode];
    if( ([event modifierFlags] & NSControlKeyMask) && ([event modifierFlags] & NSAlternateKeyMask) ) {
        [self requestUngrab];
    }

    if( flagsMask[keycode] == 0 ) {
        input_event_keydown( NULL, keycode+1, 1 );
        flagsMask[keycode] = 1;
    } else {
        input_event_keyup( NULL, keycode+1, 1 );
        flagsMask[keycode] = 0;
    }
}
- (void)mouseDown: (NSEvent *) event
{
    if( isGrabbed ) { 
        buttonMask |= 1;
        input_event_mouse( buttonMask, 0, 0 );
    } else {
        [self requestGrab];
    }
}
- (void)mouseUp: (NSEvent *)event
{
    buttonMask &= ~1;
    input_event_mouse( buttonMask, 0, 0 );
}

- (void)rightMouseDown: (NSEvent *) event
{
    buttonMask |= 2;
    input_event_mouse( buttonMask, 0, 0 );
}
- (void)rightMouseUp: (NSEvent *)event
{
    buttonMask &= ~2;
    input_event_mouse( buttonMask, 0, 0 );
}
- (void)otherMouseDown: (NSEvent *) event
{
    buttonMask |= (1<< [event buttonNumber] );
    input_event_mouse( buttonMask, 0, 0 );
}
- (void)otherMouseUp: (NSEvent *) event
{
    buttonMask &= ~(1<< [event buttonNumber] );
    input_event_mouse( buttonMask, 0, 0 );
}
- (void)mouseMoved: (NSEvent *) event
{
    if( isGrabbed ) {
        input_event_mouse( buttonMask, [event deltaX] * MOUSE_X_SCALE, [event deltaY] * MOUSE_Y_SCALE );
    }
}
- (void)mouseDragged: (NSEvent *) event
{
    if( isGrabbed ) {
        input_event_mouse( buttonMask, [event deltaX] * MOUSE_X_SCALE, [event deltaY] * MOUSE_Y_SCALE );
    }
}
- (void)rightMouseDragged: (NSEvent *) event
{
    if( isGrabbed ) {
        input_event_mouse( buttonMask, [event deltaX] * MOUSE_X_SCALE, [event deltaY] * MOUSE_Y_SCALE );
    }
}
- (void)otherMouseDragged: (NSEvent *) event
{
    if( isGrabbed ) {
        input_event_mouse( buttonMask, [event deltaX] * MOUSE_X_SCALE, [event deltaY] * MOUSE_Y_SCALE );
    }
}

@end

NSView *video_osx_create_drawable()
{
    NSRect contentRect = {{0,0},{640,480}};
    video_view = [[LxdreamOSXView alloc] initWithFrame: contentRect];
    [video_view setAutoresizingMask: (NSViewWidthSizable|NSViewHeightSizable)];
    return video_view;
}

static gboolean video_osx_init()
{
    if( video_view == NULL ) {
        return FALSE;
    }
    if( !video_nsgl_init_driver(video_view, &display_osx_driver) ) {
        return FALSE;
    }
    pvr2_setup_gl_context();
    return TRUE;
}

static void video_osx_shutdown()
{
}

static void video_osx_display_blank( uint32_t colour )
{
}

static int mac_keymap_cmp(const void *a, const void *b)
{
    const gchar *key = a;
    const struct mac_keymap_struct *kb = b;
    return strcasecmp(key, kb->name);
}

static uint16_t video_osx_resolve_keysym( const gchar *keysym )
{
    struct mac_keymap_struct *result = bsearch( keysym, mac_keysyms, mac_keysym_count, sizeof(struct mac_keymap_struct), mac_keymap_cmp );
    if( result == NULL ) {
        return 0;
    } else {
        return result->keycode + 1;
    }
}

static uint16_t video_osx_keycode_to_dckeysym(uint16_t keycode)
{
    if( keycode < 1 || keycode > 128 ) {
        return DCKB_NONE;
    } else {
        return mac_keycode_to_dckeysym[keycode-1];
    }
}

static gchar *video_osx_keycode_to_keysym(uint16_t keycode)
{
    if( keycode < 1 || keycode > 128 ) {
        return NULL;
    } else {
        return g_strdup(mac_keysyms_by_keycode[keycode-1]);
    }
}