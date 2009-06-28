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
#include "lxpaths.h"
#include "display.h"
#include "maple/maple.h"
#include "vmu/vmulist.h"

#include <glib/gstrfuncs.h>

#define FIRST_SECONDARY_DEVICE MAPLE_PORTS

#define FIRST_VMU_TAG 0x1000
#define LOAD_VMU_TAG -1
#define CREATE_VMU_TAG -2

#define KEYBINDING_SIZE 110

static void cocoa_config_keysym_hook(void *data, const gchar *keysym);
static gboolean cocoa_config_vmulist_hook(vmulist_change_type_t type, int idx, void *data);

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
- (void)resignFirstResponder
{
    if( isPrimed ) {
        [self setString: lastValue];
        [self setPrimed: NO];
    }
    [super resignFirstResponder];
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

/*************************** Key-binding sub-view ***********************/

#define MAX_KEY_BINDINGS 32

@interface ControllerKeyBindingView : NSView
{
    maple_device_t device;
    NSTextField *field[MAX_KEY_BINDINGS][2];
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
    const gchar *p = NULL;
    int binding = [[notify object] tag];
    NSString *val1 = [field[binding][0] stringValue];
    if( field[binding][1] == NULL ) {
        p = [val1 UTF8String];
    } else {
        NSString *val2 = [field[binding][1] stringValue];
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
    }
    maple_set_device_config_value( device, binding, p ); 
    lxdream_save_config();
}
- (void)setDevice: (maple_device_t)newDevice
{
    device = newDevice;
    [self removeSubviews];
    if( device != NULL && !MAPLE_IS_VMU(device) ) {
        lxdream_config_entry_t config = maple_get_device_config(device);
        if( config != NULL ) {
            int count, i, y, x;

            for( count=0; config[count].key != NULL; count++ );
            x = TEXT_GAP;
            NSSize size = NSMakeSize(85+KEYBINDING_SIZE*2+TEXT_GAP*4, count*(TEXT_HEIGHT+TEXT_GAP)+TEXT_GAP);
            [self setFrameSize: size];
            [self scrollRectToVisible: NSMakeRect(0,0,1,1)]; 
            y = TEXT_GAP;
            for( i=0; config[i].key != NULL; i++ ) {
                /* Add label */
                NSRect frame = NSMakeRect(x, y + 2, 85, LABEL_HEIGHT);
                NSTextField *label = cocoa_gui_add_label(self, NS_(config[i].label), frame);
                [label setAlignment: NSRightTextAlignment];
                
                switch(config[i].type) {
                case CONFIG_TYPE_KEY:
                    frame = NSMakeRect( x + 85 + TEXT_GAP, y, KEYBINDING_SIZE, TEXT_HEIGHT);
                    field[i][0] = [[KeyBindingField alloc] initWithFrame: frame];
                    [field[i][0] setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
                    [field[i][0] setTag: i];
                    [field[i][0] setDelegate: self];
                    [self addSubview: field[i][0]];

                    frame = NSMakeRect( x + 85 + KEYBINDING_SIZE + (TEXT_GAP*2), y, KEYBINDING_SIZE, TEXT_HEIGHT);
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
                    break;
                case CONFIG_TYPE_FILE:
                case CONFIG_TYPE_PATH:
                    frame = NSMakeRect( x + 85 + TEXT_GAP, y, KEYBINDING_SIZE*2+TEXT_GAP, TEXT_HEIGHT);
                    field[i][0] = [[NSTextField alloc] initWithFrame: frame];
                    [field[i][0] setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
                    [field[i][0] setTag: i];
                    [field[i][0] setDelegate: self];
                    [self addSubview: field[i][0]];
                    if( config[i].value != NULL ) {
                        [field[i][0] setStringValue: [NSString stringWithCString: config[i].value]];
                    }
                    field[i][1] = NULL;
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
static NSButton *addRadioButton( int port, int sub, int x, int y, id parent )
{
    char buf[16];
    
    if( sub == 0 ) {
        snprintf( buf, sizeof(buf), _("Port %c."), 'A'+port );
    } else {
        snprintf( buf, sizeof(buf), _("VMU %d."), sub );
    }

    NSButton *radio = [[NSButton alloc] initWithFrame: NSMakeRect( x, y, 60, TEXT_HEIGHT )];
    [radio setTitle: [NSString stringWithUTF8String: buf]];
    [radio setTag: MAPLE_DEVID(port,sub) ];
    [radio setButtonType: NSRadioButton];
    [radio setAlignment: NSRightTextAlignment];
    [radio setTarget: parent];
    [radio setAction: @selector(radioChanged:)];
    [radio setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
    [parent addSubview: radio];
    return radio;
}

static void setDevicePopupSelection( NSPopUpButton *popup, maple_device_t device )
{
    if( device == NULL ) {
        [popup selectItemAtIndex: 0];
    } else if( MAPLE_IS_VMU(device) ) {
        int idx = vmulist_get_index_by_filename( MAPLE_VMU_NAME(device) );
        if( idx == -1 ) {
            [popup selectItemAtIndex: 0];
        } else {
            [popup selectItemWithTag: FIRST_VMU_TAG + idx];
        }
    } else {
        const struct maple_device_class **devices = maple_get_device_classes();
        int i;
        for( i=0; devices[i] != NULL; i++ ) {
            if( devices[i] == device->device_class ) {
                [popup selectItemWithTag: i+1];
                return;
            }
        }
        // Should never get here, but if so...
        [popup selectItemAtIndex: 0];
    }
}

static void buildDevicePopupMenu( NSPopUpButton *popup, maple_device_t device, BOOL primary )
{
    int j;
    const struct maple_device_class **devices = maple_get_device_classes();

    [popup removeAllItems];
    [popup addItemWithTitle: NS_("<empty>")];
    [[popup itemAtIndex: 0] setTag: 0];
    for( j=0; devices[j] != NULL; j++ ) {
        int isPrimaryDevice = devices[j]->flags & MAPLE_TYPE_PRIMARY;
        if( primary ? isPrimaryDevice : (!isPrimaryDevice && !MAPLE_IS_VMU_CLASS(devices[j])) ) {
            [popup addItemWithTitle: [NSString stringWithUTF8String: devices[j]->name]];
            if( device != NULL && device->device_class == devices[j] ) {
                [popup selectItemAtIndex: ([popup numberOfItems]-1)];
            }
            [[popup itemAtIndex: ([popup numberOfItems]-1)] setTag: (j+1)];
        }
    }
    
    if( !primary ) {
        BOOL vmu_selected = NO;
        const char *vmu_name;
        if( device != NULL && MAPLE_IS_VMU(device) ) {
            vmu_selected = YES;
            vmu_name = MAPLE_VMU_NAME(device);
        }
        if( [popup numberOfItems] > 0 ) {
            [[popup menu] addItem: [NSMenuItem separatorItem]];
        }
        
        unsigned int vmu_count = vmulist_get_size();
        for( j=0; j<vmu_count; j++ ) {
            const char *name = vmulist_get_name(j);
            [popup addItemWithTitle: [NSString stringWithUTF8String: name]];
            if( vmu_selected && strcmp(vmu_name, vmulist_get_filename(j)) == 0 ) {
                [popup selectItemAtIndex: ([popup numberOfItems]-1)];
            }
            [[popup itemAtIndex: ([popup numberOfItems]-1)] setTag: FIRST_VMU_TAG + j];
        }
        
        [popup addItemWithTitle: NS_("Load VMU...")];
        [[popup itemAtIndex: ([popup numberOfItems]-1)] setTag: LOAD_VMU_TAG];
        [popup addItemWithTitle: NS_("Create VMU...")];
        [[popup itemAtIndex: ([popup numberOfItems]-1)] setTag: CREATE_VMU_TAG];
    }
    
}

static NSPopUpButton *addDevicePopup( int port, int sub, int x, int y, maple_device_t device, BOOL primary, id parent )
{
    NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame: NSMakeRect(x,y,150,TEXT_HEIGHT) 
                                                  pullsDown: NO];
    [popup setAutoresizingMask: (NSViewMinYMargin|NSViewMaxXMargin)];
    buildDevicePopupMenu(popup,device,primary);

    [popup setTarget: parent];
    [popup setAction: @selector(deviceChanged:)];
    [popup setTag: MAPLE_DEVID(port,sub) ];
    [parent addSubview: popup];    
    return popup;
}

@interface VMULoadValidator : NSObject
{
}
- (BOOL)panel:(id) sender isValidFilename: (NSString *)filename;
@end

@implementation VMULoadValidator 
- (BOOL)panel:(id) sender isValidFilename: (NSString *)filename
{
    const char *c_fn = [filename UTF8String];
    vmu_volume_t vol = vmu_volume_load( c_fn );
    if( vol != NULL ) {
        vmulist_add_vmu(c_fn, vol);
        return YES;
    } else {
        ERROR( "Unable to load VMU file (not a valid VMU)" );
        return NO;
    }
}

@end

@interface VMUCreateValidator : NSObject
{
}
- (BOOL)panel:(id) sender isValidFilename: (NSString *)filename;
@end

@implementation VMUCreateValidator 
- (BOOL)panel:(id) sender isValidFilename: (NSString *)filename
{
    const char *vmu_filename = [filename UTF8String];
    int idx = vmulist_create_vmu(vmu_filename, FALSE);
    if( idx == -1 ) {
        ERROR( "Unable to create file: %s\n", strerror(errno) );
        return NO;
    } else {
        return YES;
    }
}
@end


@interface LxdreamPrefsControllerPane: LxdreamPrefsPane
{
    struct maple_device *save_controller[MAPLE_MAX_DEVICES];
    NSButton *radio[MAPLE_MAX_DEVICES];
    NSPopUpButton *popup[MAPLE_MAX_DEVICES];
    ControllerKeyBindingView *key_bindings;
}
+ (LxdreamPrefsControllerPane *)new;
- (void)vmulistChanged: (id)sender;
@end

static gboolean cocoa_config_vmulist_hook(vmulist_change_type_t type, int idx, void *data)
{
    LxdreamPrefsControllerPane *pane = (LxdreamPrefsControllerPane *)data;
    [pane vmulistChanged: nil];
    return TRUE;
}

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
        int i,j;
        int y = [self contentHeight] - TEXT_HEIGHT - TEXT_GAP;

        memset( radio, 0, sizeof(radio) );
        memset( save_controller, 0, sizeof(save_controller) );
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
        
        for( i=0; i<MAPLE_PORTS; i++ ) {
            maple_device_t device = maple_get_device(i,0);

            radio[i] = addRadioButton(i,0,TEXT_GAP,y,self);
            popup[i] = addDevicePopup(i,0,60 + (TEXT_GAP*2),y,device, YES,self);
            y -= (TEXT_HEIGHT+TEXT_GAP);
            
            int j,max = device == NULL ? 0 : MAPLE_SLOTS(device->device_class);
            for( j=1; j<=MAPLE_USER_SLOTS; j++ ) {
                radio[MAPLE_DEVID(i,j)] = addRadioButton(i, j, TEXT_GAP*2, y, self);
                popup[MAPLE_DEVID(i,j)] = addDevicePopup(i, j, 60 + TEXT_GAP*2, y, maple_get_device(i,j), NO, self);
                y -= (TEXT_HEIGHT+TEXT_GAP);
                if( j > max ) {
                    [radio[MAPLE_DEVID(i,j)] setEnabled: NO];
                    [popup[MAPLE_DEVID(i,j)] setEnabled: NO];
                }
            }
        }
        
        [radio[0] setState: NSOnState];
        
        register_vmulist_change_hook(cocoa_config_vmulist_hook, self);
        return self;
    }
}
- (void)dealloc
{
    unregister_vmulist_change_hook(cocoa_config_vmulist_hook,self);
    [super dealloc];
}
- (void)vmulistChanged: (id)sender
{
    int i;
    for( i=FIRST_SECONDARY_DEVICE; i<MAPLE_MAX_DEVICES; i++ ) {
        if( popup[i] != NULL ) {
            buildDevicePopupMenu(popup[i], maple_get_device(MAPLE_DEVID_PORT(i), MAPLE_DEVID_SLOT(i)), NO );
        }
    }
}
- (void)radioChanged: (id)sender
{
    int tag = [sender tag];
    int i;
    for( i=0; i<MAPLE_MAX_DEVICES; i++ ) {
        if( i != tag && radio[i] != NULL ) {
            [radio[i] setState: NSOffState];
        }
    }
    [key_bindings setDevice: maple_get_device(MAPLE_DEVID_PORT(tag),MAPLE_DEVID_SLOT(tag))];
}
- (void)deviceChanged: (id)sender
{
    int tag = [sender tag];
    int port = MAPLE_DEVID_PORT(tag);
    int slot = MAPLE_DEVID_SLOT(tag);
    int new_device_idx = [[sender selectedItem] tag], i; 
    maple_device_class_t new_device_class = NULL;
    const gchar *vmu_filename = NULL;
    
    for( i=0; i<MAPLE_MAX_DEVICES; i++ ) {
        if( radio[i] != NULL ) {
            if( i == tag ) {
                [radio[i] setState: NSOnState];
            } else {
                [radio[i] setState: NSOffState];
            }
        }
    }
    
    maple_device_t current = maple_get_device(port,slot);
    maple_device_t new_device = NULL;
    if( new_device_idx == LOAD_VMU_TAG ) {
        NSArray *array = [NSArray arrayWithObjects: @"vmu", nil];
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        VMULoadValidator *valid = [[VMULoadValidator alloc] autorelease];
        [panel setDelegate: valid];
        int result = [panel runModalForDirectory: [NSString stringWithUTF8String: get_gui_path(CONFIG_VMU_PATH)]
               file: nil types: array];
        if( result == NSOKButton ) {
            vmu_filename = [[panel filename] UTF8String];
            int idx = vmulist_get_index_by_filename(vmu_filename);
            [sender selectItemWithTag: (FIRST_VMU_TAG+idx)];
            new_device_class = &vmu_class;
            set_gui_path(CONFIG_VMU_PATH, [[panel directory] UTF8String]);
        } else {
            /* Cancelled - restore previous value */
            setDevicePopupSelection( sender, current );
            return;
        }
    } else if( new_device_idx == CREATE_VMU_TAG ) {
        NSSavePanel *panel = [NSSavePanel savePanel];
        [panel setTitle: NS_("Create VMU")];
        [panel setCanCreateDirectories: YES];
        [panel setRequiredFileType: @"vmu"];
        VMUCreateValidator *valid = [[VMUCreateValidator alloc] autorelease];
        [panel setDelegate: valid];
        int result = [panel runModalForDirectory: [NSString stringWithUTF8String: get_gui_path(CONFIG_VMU_PATH)]
               file: nil];
        if( result == NSFileHandlingPanelOKButton ) {
            /* Validator has already created the file by now */
            vmu_filename = [[panel filename] UTF8String];
            int idx = vmulist_get_index_by_filename(vmu_filename);
            [sender selectItemWithTag: (FIRST_VMU_TAG+idx)];
            new_device_class = &vmu_class;
            set_gui_path(CONFIG_VMU_PATH, [[panel directory] UTF8String]);
        } else {
            setDevicePopupSelection( sender, current );
            return;
        }
    } else if( new_device_idx >= FIRST_VMU_TAG ) {
        vmu_filename = vmulist_get_filename( new_device_idx - FIRST_VMU_TAG );
        new_device_class = &vmu_class;
    } else if( new_device_idx > 0) {
        new_device_class = maple_get_device_classes()[new_device_idx-1];
    }
    
    if( current == NULL ? new_device_class == NULL : 
        (current->device_class == new_device_class && 
                (!MAPLE_IS_VMU(current) || MAPLE_VMU_HAS_NAME(current, vmu_filename))) ) {
        // No change
        [key_bindings setDevice: current];
        return;
    }
    if( current != NULL && current->device_class == &controller_class ) {
        save_controller[tag] = current->clone(current);
    }
    if( new_device_class == NULL ) {
        maple_detach_device(port,slot);
        if( slot == 0 ) {
            /* If we detached the top-level dev, any children are automatically detached */
            for( i=1; i<=MAPLE_USER_SLOTS; i++ ) {
                [popup[MAPLE_DEVID(port,i)] selectItemWithTag: 0];
            }
        }
    } else {
        if( new_device_class == &controller_class && save_controller[tag] != NULL ) {
            new_device = save_controller[tag];
            save_controller[tag] = NULL;
        } else {
            new_device = maple_new_device( new_device_class->name );
        }
        if( MAPLE_IS_VMU(new_device) ) {
            /* Remove the VMU from any other attachment point */
            for( i=0; i<MAPLE_MAX_DEVICES; i++ ) {
                maple_device_t dev = maple_get_device(MAPLE_DEVID_PORT(i),MAPLE_DEVID_SLOT(i));
                if( dev != NULL && MAPLE_IS_VMU(dev) && MAPLE_VMU_HAS_NAME(dev,vmu_filename) ) {
                    maple_detach_device(MAPLE_DEVID_PORT(i),MAPLE_DEVID_SLOT(i));
                    [popup[i] selectItemWithTag: 0];
                }
            }
            MAPLE_SET_VMU_NAME(new_device,vmu_filename);
        }
        maple_attach_device(new_device,port,slot);
    }
    [key_bindings setDevice: maple_get_device(port,slot)];
    
    if( slot == 0 ) { /* Change primary */
        int max = new_device_class == NULL ? 0 : MAPLE_SLOTS(new_device_class);
        for( i=1; i<=MAPLE_USER_SLOTS; i++ ) {
            if( i <= max ) {
                [radio[MAPLE_DEVID(port,i)] setEnabled: YES];
                [popup[MAPLE_DEVID(port,i)] setEnabled: YES];
            } else {
                [radio[MAPLE_DEVID(port,i)] setEnabled: NO];
                [popup[MAPLE_DEVID(port,i)] setEnabled: NO];
            }                
        }
    }
    lxdream_save_config();
}
@end

NSView *cocoa_gui_create_prefs_controller_pane()
{
    return [LxdreamPrefsControllerPane new];
}
