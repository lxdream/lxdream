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
#include "display.h"
#include "maple/maple.h"

#include <glib/gstrfuncs.h>

#define MAX_DEVICES 4

static void cocoa_config_keysym_hook(void *data, const gchar *keysym);

@interface KeyBindingEditor (Private)
- (void)updateKeysym: (const gchar *)sym;
@end

@implementation KeyBindingEditor
- (id)init
{
    self = [super init];
    isPrimed = NO;
    lastValue = nil;
    [self setFieldEditor: YES];
    [self setEditable: FALSE];
    return self;
}
- (void)dealloc
{
    if( lastValue != nil ) {
        [lastValue release];
        lastValue = nil;
    }
    [super dealloc];
}
- (void)setPrimed: (BOOL)primed
{
    if( primed != isPrimed ) {
        isPrimed = primed;
        if( primed ) {
            lastValue = [[NSString stringWithString: [self string]] retain];
            [self setString: @"<press key>"];
            input_set_keysym_hook(cocoa_config_keysym_hook, self);            
        } else {
            [lastValue release];
            lastValue = nil;
            input_set_keysym_hook(NULL,NULL);
        }
    }
}
- (void)fireBindingChanged
{
    id delegate = [self delegate];
    if( delegate != nil && [delegate respondsToSelector:@selector(textDidChange:)] ) {
        [delegate textDidChange: [NSNotification notificationWithName: NSTextDidChangeNotification object: self]];
    }
}
        
- (void)updateKeysym: (const gchar *)sym
{
    if( sym != NULL ) {
        [self setString: [NSString stringWithCString: sym]];
        [self setPrimed: NO];
        [self fireBindingChanged];
    }
}
- (void)keyPressed: (int)keycode
{
    gchar *keysym = input_keycode_to_keysym(NULL, keycode);
    if( keysym != NULL ) {
        [self updateKeysym: keysym];
        g_free(keysym);
    }
}
- (void)insertText:(id)string
{
    // Do nothing
}
- (void)mouseDown: (NSEvent *)event
{
    [self setPrimed: YES];
    [super mouseDown: event];
}
- (void)keyDown: (NSEvent *) event
{
    NSString *chars = [event characters];
    if( isPrimed ) {
        if( chars != NULL && [chars length] == 1 && [chars characterAtIndex: 0] == 27 ) {
            // Escape char = abort change
            [self setString: lastValue];
            [self setPrimed: NO];
        } else {
            [self keyPressed: ([event keyCode]+1)];
        }
    } else {
        if( chars != NULL && [chars length] == 1 ) {
            int ch = [chars characterAtIndex: 0];
            switch( ch ) {
            case 0x7F:
                [self setString: @""]; 
                [self fireBindingChanged];
                break;
            case '\r':
                [self setPrimed: YES];
                break;
            default:
                [super keyDown: event];
                break;
            }
        } else {
            [super keyDown: event];
        }
    }
}
- (void)flagsChanged: (NSEvent *) event
{
    if( isPrimed ) {
        [self keyPressed: ([event keyCode]+1)];
    }
    [super flagsChanged: event];
}
@end

static void cocoa_config_keysym_hook(void *data, const gchar *keysym)
{
    KeyBindingEditor *editor = (KeyBindingEditor *)data;
    [editor updateKeysym: keysym];
}


@implementation KeyBindingField
@end

/*************************** Key-binding sub-view ***********************/

#define MAX_KEY_BINDINGS 32

@interface ControllerKeyBindingView : NSView
{
    maple_device_t device;
    KeyBindingField *field[MAX_KEY_BINDINGS][2];
}
- (id)initWithFrame: (NSRect)frameRect;
- (void)setDevice: (maple_device_t)device;
@end

@implementation ControllerKeyBindingView
- (id)initWithFrame: (NSRect)frameRect
{
    if( [super initWithFrame: frameRect] == nil ) {
        return nil;
    } else {
        device = NULL;
        return self;
    }
}
- (BOOL)isFlipped
{
    return YES;
}
- (void)removeSubviews
{
    [[self subviews] makeObjectsPerformSelector: @selector(removeFromSuperview)];
}
- (void)controlTextDidChange: (NSNotification *)notify
{
    int binding = [[notify object] tag];
    NSString *val1 = [field[binding][0] stringValue];
    NSString *val2 = [field[binding][1] stringValue];
    char buf[ [val1 length] + [val2 length] + 2 ];
    const gchar *p = NULL;
    
    if( [val1 length] == 0 ) {
        if( [val2 length] != 0 ) {
            p = [val2 UTF8String];
        }
    } else if( [val2 length] == 0 ) {
        p = [val1 UTF8String];
    } else {
        sprintf( buf, "%s,%s", [val1 UTF8String], [val2 UTF8String] );
        p = buf;
    }
    maple_set_device_config_value( device, binding, p ); 
    lxdream_save_config();
}
- (void)setDevice: (maple_device_t)newDevice
{
    device = newDevice;
    [self removeSubviews];
    if( device != NULL ) {
        lxdream_config_entry_t config = maple_get_device_config(device);
        if( config != NULL ) {
            int count, i, y, x;

            for( count=0; config[count].key != NULL; count++ );
            x = TEXT_GAP;
            NSSize size = NSMakeSize(85*3+TEXT_GAP*4, count*(TEXT_HEIGHT+TEXT_GAP)+TEXT_GAP);
            [self setFrameSize: size];
            [self scrollRectToVisible: NSMakeRect(0,0,1,1)]; 
            y = TEXT_GAP;
            for( i=0; config[i].key != NULL; i++ ) {
                NSRect frame = NSMakeRect(x, y + 2, 85, LABEL_HEIGHT);
                NSTextField *label = cocoa_gui_add_label(self, NS_(config[i].label), frame);
                [label setAlignment: NSRightTextAlignment];

                frame = NSMakeRect( x + 85 + TEXT_GAP, y, 85, TEXT_HEIGHT);
                field[i][0] = [[KeyBindingField alloc] initWithFrame: frame];
                [field[i][0] setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
                [field[i][0] setTag: i];
                [field[i][0] setDelegate: self];
                [self addSubview: field[i][0]];
                
                frame = NSMakeRect( x + (85*2) + (TEXT_GAP*2), y, 85, TEXT_HEIGHT);
                field[i][1] = [[KeyBindingField alloc] initWithFrame: frame];
                [field[i][1] setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
                [field[i][1] setTag: i];
                [field[i][1] setDelegate: self];
                [self addSubview: field[i][1]];

                if( config[i].value != NULL ) {
                    gchar **parts = g_strsplit(config[i].value,",",3);
                    if( parts[0] != NULL ) {
                        [field[i][0] setStringValue: [NSString stringWithCString: parts[0]]];
                        if( parts[1] != NULL ) {
                            [field[i][1] setStringValue: [NSString stringWithCString: parts[1]]];
                        }
                    }
                    g_strfreev(parts);
                }
                
                y += (TEXT_HEIGHT + TEXT_GAP);
            }
        } else {
            [self setFrameSize: NSMakeSize(100,TEXT_HEIGHT+TEXT_GAP) ];
        }
    } else {
        [self setFrameSize: NSMakeSize(100,TEXT_HEIGHT+TEXT_GAP) ];
    }
}
@end

/*************************** Top-level controller pane ***********************/

@interface LxdreamPrefsControllerPane: LxdreamPrefsPane
{
    struct maple_device *save_controller[4];
    NSButton *radio[4];
    ControllerKeyBindingView *key_bindings;
}
+ (LxdreamPrefsControllerPane *)new;
@end

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
        int y = [self contentHeight] - TEXT_HEIGHT - TEXT_GAP;

        NSBox *rule = [[NSBox alloc] initWithFrame: 
                NSMakeRect(210+(TEXT_GAP*3), 1, 1, [self contentHeight] + TEXT_GAP - 2)];
        [rule setAutoresizingMask: (NSViewMaxXMargin|NSViewHeightSizable)];
        [rule setBoxType: NSBoxSeparator];
        [self addSubview: rule];
        
        NSRect bindingFrame = NSMakeRect(210+(TEXT_GAP*4), 0,
                   frameRect.size.width - (210+(TEXT_GAP*4)), [self contentHeight] + TEXT_GAP );
        NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame: bindingFrame];
        key_bindings = [[ControllerKeyBindingView alloc] initWithFrame: bindingFrame ];
        [scrollView setAutoresizingMask: (NSViewWidthSizable|NSViewHeightSizable)];
        [scrollView setDocumentView: key_bindings];
        [scrollView setDrawsBackground: NO];
        [scrollView setHasVerticalScroller: YES];
        [scrollView setAutohidesScrollers: YES];
 
        [self addSubview: scrollView];
        [key_bindings setDevice: maple_get_device(0,0)];
        
        for( i=0; i<MAX_DEVICES; i++ ) {
            int x = TEXT_GAP;
            save_controller[i] = NULL;
            maple_device_t device = maple_get_device(i,0);

            snprintf( buf, sizeof(buf), _("Slot %d."), i );
            radio[i] = [[NSButton alloc] initWithFrame: NSMakeRect( x, y, 60, TEXT_HEIGHT )];
            [radio[i] setTitle: [NSString stringWithUTF8String: buf]];
            [radio[i] setTag: i];
            [radio[i] setButtonType: NSRadioButton];
            [radio[i] setAlignment: NSRightTextAlignment];
            [radio[i] setTarget: self];
            [radio[i] setAction: @selector(radioChanged:)];
            [radio[i] setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
            [self addSubview: radio[i]];
            x += 60 + TEXT_GAP;

            NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame: NSMakeRect(x,y,150,TEXT_HEIGHT) 
                                                          pullsDown: NO];
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
            y -= (TEXT_HEIGHT+TEXT_GAP);
        }
        
        [radio[0] setState: NSOnState];
        return self;
    }
}
- (void)radioChanged: (id)sender
{
    int slot = [sender tag];
    int i;
    for( i=0; i<MAX_DEVICES; i++ ) {
        if( i != slot ) {
            [radio[i] setState: NSOffState];
        }
    }
    [key_bindings setDevice: maple_get_device(slot,0)];
}
- (void)deviceChanged: (id)sender
{
    int slot = [sender tag];
    int new_device_idx = [sender indexOfSelectedItem] - 1, i; 
    maple_device_class_t new_device_class = NULL;
    
    for( i=0; i<MAX_DEVICES; i++ ) {
        if( i == slot ) {
            [radio[i] setState: NSOnState];
        } else {
            [radio[i] setState: NSOffState];
        }
    }
    
    maple_device_t current = maple_get_device(slot,0);
    maple_device_t new_device = NULL;
    if( new_device_idx != -1 ) {
        new_device_class = maple_get_device_classes()[new_device_idx];
    }
    if( current == NULL ? new_device_class == NULL : current->device_class == new_device_class ) {
        // No change
        [key_bindings setDevice: current];
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
    [key_bindings setDevice: maple_get_device(slot,0)];
    lxdream_save_config();
}
@end

NSView *cocoa_gui_create_prefs_controller_pane()
{
    return [LxdreamPrefsControllerPane new];
}
