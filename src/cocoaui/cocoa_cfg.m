/**
 * $Id$
 *
 * Construct and manage a configuration pane based on an underlying
 * configuration group.
 *
 * Copyright (c) 2009 Nathan Keynes.
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
#include "display.h"
#include "lxpaths.h"
#include "maple/maple.h"

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
- (BOOL)resignFirstResponder
{
    if( isPrimed ) {
        [self setString: lastValue];
        [self setPrimed: NO];
    }
    return [super resignFirstResponder];
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
- (void)updateMousesym: (int)button
{
    gchar *keysym = input_keycode_to_keysym( &system_mouse_driver, (button+1) );
    if( keysym != NULL ) {
        [self updateKeysym: keysym ];
        g_free(keysym);
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
    if( isPrimed ) {
        [self updateMousesym: 0];
    } else {
        [self setPrimed: YES];
        [super mouseDown: event];
    }
}
- (void)rightMouseDown: (NSEvent *)event
{
    if( isPrimed ) {
        [self updateMousesym: 1];
    }
}
- (void)otherMouseDown: (NSEvent *)event
{
    if( isPrimed ) {
        [self updateMousesym: [event buttonNumber]];
    }
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

/*************************** Configuration sub-view ***********************/

#define KEYBINDING_SIZE 110
#define DEFAULT_LABEL_WIDTH 150
#define RIGHT_MARGIN 40

@implementation ConfigurationView
- (id)initWithFrame: (NSRect)frameRect
{
    return [self initWithFrame: frameRect configGroup: NULL];
}
- (id)initWithFrame: (NSRect)frameRect configGroup: (lxdream_config_group_t)config
{
    if( [super initWithFrame: frameRect] == nil ) {
        return nil;
    } else {
        group = NULL;
        labelWidth = DEFAULT_LABEL_WIDTH;
        [self setConfigGroup: config];
        return self;
    }
}
- (BOOL)isFlipped
{
    return YES;
}
- (void)setLabelWidth: (int)width
{
    labelWidth = width;
}
- (void)removeSubviews
{
    [[self subviews] makeObjectsPerformSelector: @selector(removeFromSuperview)];
}
- (void)updateField: (int)binding
{
    const gchar *p = NULL;
    NSString *val1 = [fields[binding][0] stringValue];
     if( fields[binding][1] == NULL ) {
         p = [val1 UTF8String];
         lxdream_set_config_value( group, binding, p );
     } else {
         NSString *val2 = [fields[binding][1] stringValue];
         char buf[ [val1 length] + [val2 length] + 2 ];

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
         lxdream_set_config_value( group, binding, p );
     }
     lxdream_save_config();
}

- (void)controlTextDidChange: (NSNotification *)notify
{
    if( [[notify object] isKindOfClass: [KeyBindingField class]] ) {
        [self updateField: [[notify object] tag]];
    }
}
- (void)controlTextDidEndEditing: (NSNotification *)notify
{
    [self updateField: [[notify object] tag]];
}

- (void)openFileDialog: (id)sender
{
    int tag = [sender tag];
    /* NSString *text = [fields[tag][0] stringValue]; */
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    int result = [panel runModalForDirectory: nil file: nil types: nil];
    if( result == NSOKButton && [[panel filenames] count] > 0 ) {
        NSString *filename = [[panel filenames] objectAtIndex: 0];
        gchar *str = get_escaped_path( [filename UTF8String] );
        [fields[tag][0] setStringValue: [NSString stringWithUTF8String: str]];
        lxdream_set_global_config_value(tag,str);
        lxdream_save_config();
    }
}
- (void)openDirDialog: (id)sender
{
    int tag = [sender tag];
    /* NSString *text = [fields[tag][0] stringValue]; */
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setCanChooseDirectories: YES];
    [panel setCanCreateDirectories: YES];
    int result = [panel runModalForDirectory: nil file: nil types: nil];
    if( result == NSOKButton && [[panel filenames] count] > 0 ) {
        NSString *filename = [[panel filenames] objectAtIndex: 0];
        gchar *str = get_escaped_path( [filename UTF8String] );
        [fields[tag][0] setStringValue: [NSString stringWithUTF8String: str]];
        lxdream_set_global_config_value(tag,str);
        lxdream_save_config();
    }
}

- (void)setConfigGroup: (lxdream_config_group_t) config
{
    if( group == config ) {
        return;
    }
    int width = [self frame].size.width;
    int fieldWidth;

    group = config;
    [self removeSubviews];
    if( config != NULL && config->params[0].key != NULL ) {
        int count, i, y, x;

        for( count=0; config->params[count].label != NULL; count++ );
        int minWidth = labelWidth+KEYBINDING_SIZE*2+TEXT_GAP*4;
        if( minWidth > width ) {
            width = minWidth;
        }
        NSSize size = NSMakeSize( width, count*(TEXT_HEIGHT+TEXT_GAP)+TEXT_GAP);
        [self setFrameSize: size];
        [self scrollRectToVisible: NSMakeRect(0,0,1,1)];

        x = TEXT_GAP;
        y = TEXT_GAP;
        for( i=0; config->params[i].label != NULL; i++ ) {
            /* Add label */
            NSRect frame = NSMakeRect(x, y + 2, labelWidth, LABEL_HEIGHT);
            NSTextField *label = cocoa_gui_add_label(self, NS_(config->params[i].label), frame);
            [label setAlignment: NSRightTextAlignment];

            switch(config->params[i].type) {
            case CONFIG_TYPE_KEY:
                frame = NSMakeRect( x + labelWidth + TEXT_GAP, y, KEYBINDING_SIZE, TEXT_HEIGHT);
                fields[i][0] = [[KeyBindingField alloc] initWithFrame: frame];
                [fields[i][0] setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
                [fields[i][0] setTag: i];
                [fields[i][0] setDelegate: (id)self];
                [self addSubview: fields[i][0]];

                frame = NSMakeRect( x + labelWidth + KEYBINDING_SIZE + (TEXT_GAP*2), y, KEYBINDING_SIZE, TEXT_HEIGHT);
                fields[i][1] = [[KeyBindingField alloc] initWithFrame: frame];
                [fields[i][1] setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
                [fields[i][1] setTag: i];
                [fields[i][1] setDelegate: (id)self];
                [self addSubview: fields[i][1]];

                if( config->params[i].value != NULL ) {
                    gchar **parts = g_strsplit(config->params[i].value,",",3);
                    if( parts[0] != NULL ) {
                        [fields[i][0] setStringValue: [NSString stringWithCString: parts[0]]];
                        if( parts[1] != NULL ) {
                            [fields[i][1] setStringValue: [NSString stringWithCString: parts[1]]];
                        }
                    }
                    g_strfreev(parts);
                }
                break;
            case CONFIG_TYPE_FILE:
            case CONFIG_TYPE_PATH:
                fieldWidth = width - labelWidth - x - TEXT_HEIGHT - RIGHT_MARGIN - (TEXT_GAP*2);
                frame = NSMakeRect( x + labelWidth + TEXT_GAP, y, fieldWidth, TEXT_HEIGHT );
                NSTextField *field = [[NSTextField alloc] initWithFrame: frame];
                [field setTag: i];
                [field setStringValue: [NSString stringWithCString: config->params[i].value]];
                [field setDelegate: (id)self];
                [field setAutoresizingMask: (NSViewMinYMargin|NSViewWidthSizable)];

                frame = NSMakeRect( x+ labelWidth + fieldWidth + (TEXT_GAP*2), y,  TEXT_HEIGHT, TEXT_HEIGHT );
                NSButton *button = [[NSButton alloc] initWithFrame: frame];
                [button setTag: i];
                [button setTitle: @""];
                [button setButtonType: NSMomentaryPushInButton];
                [button setBezelStyle: NSRoundedDisclosureBezelStyle];
                [button setAutoresizingMask: (NSViewMinYMargin|NSViewMinXMargin)];
                [button setTarget: self];
                if( config->params[i].type == CONFIG_TYPE_FILE ) {
                    [button setAction: @selector(openFileDialog:)];
                } else {
                    [button setAction: @selector(openDirDialog:)];
                }

                [self addSubview: label];
                [self addSubview: field];
                [self addSubview: button];
                fields[i][0] = field;
                fields[i][1] = NULL;
            }
            y += (TEXT_HEIGHT + TEXT_GAP);
        }
    } else {
        [self setFrameSize: NSMakeSize(100,TEXT_HEIGHT+TEXT_GAP) ];
    }
}

- (void)setDevice: (maple_device_t)newDevice
{
    if( newDevice != NULL && !MAPLE_IS_VMU(newDevice) ) {
        [self setConfigGroup: maple_get_device_config(newDevice)];
    } else {
        [self setConfigGroup: NULL];
    }
}
@end

