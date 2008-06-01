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

static gboolean video_osx_init();
static void video_osx_shutdown();
static void video_osx_display_blank( uint32_t colour );
static uint16_t video_osx_resolve_keysym( const gchar *keysym );
static uint16_t video_osx_keycode_to_dckeysym(uint16_t keycode);

struct display_driver display_osx_driver = { "osx", video_osx_init, video_osx_shutdown,
                    video_osx_resolve_keysym,
                    video_osx_keycode_to_dckeysym,
                    NULL,
                    NULL, NULL, NULL, NULL, NULL, 
                    video_osx_display_blank, NULL };


static NSView *video_view = NULL;
int video_width = 640;
int video_height = 480;

#define MAX_MASK_KEYCODE 128

@interface LxdreamVideoView : NSView
{
    BOOL isGrabbed;
    int buttonMask;
    int flagsMask[MAX_MASK_KEYCODE];
}
- (BOOL)isOpaque;
- (BOOL)isFlipped;
- (void)drawRect: (NSRect) rect;
@end

@implementation LxdreamVideoView
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
    if ( isGrabbed && ([event modifierFlags] & NSControlKeyMask) && ([event modifierFlags] & NSAlternateKeyMask) ) {
        // Release the display grab
        isGrabbed = NO;
        [NSCursor unhide];
        CGAssociateMouseAndMouseCursorPosition(YES);
        [((LxdreamMainWindow *)[self window]) setIsGrabbed: NO];
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
    } else { // take display grab
        isGrabbed = YES;
        [NSCursor hide];
        CGAssociateMouseAndMouseCursorPosition(NO);
        [((LxdreamMainWindow *)[self window]) setIsGrabbed: YES];
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
        input_event_mouse( buttonMask, [event deltaX], [event deltaY] );
    }
}
- (void)mouseDragged: (NSEvent *) event
{
    if( isGrabbed ) {
        input_event_mouse( buttonMask, [event deltaX], [event deltaY] );
    }
}
- (void)rightMouseDragged: (NSEvent *) event
{
    if( isGrabbed ) {
        input_event_mouse( buttonMask, [event deltaX], [event deltaY] );
    }
}
- (void)otherMouseDragged: (NSEvent *) event
{
    if( isGrabbed ) {
        input_event_mouse( buttonMask, [event deltaX], [event deltaY] );
    }
}

@end

NSView *video_osx_create_drawable()
{
    NSRect contentRect = {{0,0},{640,480}};
    video_view = [[LxdreamVideoView alloc] initWithFrame: contentRect];
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

