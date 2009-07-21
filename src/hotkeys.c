/**
 * $Id$
 *
 * Handles hotkeys for pause/continue, save states, quit, etc
 *
 * Copyright (c) 2009 wahrhaft
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
#include <assert.h>
#include <glib.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include "lxdream.h"
#include "dreamcast.h"
#include "display.h"
#include "hotkeys.h"
#include "gui.h"
#include "config.h"

static void hotkey_key_callback( void *data, uint32_t value, uint32_t pressure, gboolean isKeyDown );
static gboolean hotkey_config_changed( void *data, lxdream_config_group_t group, unsigned key,
                                       const gchar *oldval, const gchar *newval );

#define TAG_RESUME 0
#define TAG_STOP 1
#define TAG_RESET 2
#define TAG_EXIT 3
#define TAG_SAVE 4
#define TAG_LOAD 5
#define TAG_SELECT(i) (6+(i))

struct lxdream_config_group hotkeys_group = {
    "hotkeys", input_keygroup_changed, hotkey_key_callback, NULL, {
        {"resume", N_("Resume emulation"), CONFIG_TYPE_KEY, NULL, TAG_RESUME },
        {"stop", N_("Stop emulation"), CONFIG_TYPE_KEY, NULL, TAG_STOP },
        {"reset", N_("Reset emulator"), CONFIG_TYPE_KEY, NULL, TAG_RESET },
        {"exit", N_("Exit emulator"), CONFIG_TYPE_KEY, NULL, TAG_EXIT },
        {"save", N_("Save current quick save"), CONFIG_TYPE_KEY, NULL, TAG_SAVE },
        {"load", N_("Load current quick save"), CONFIG_TYPE_KEY, NULL, TAG_LOAD },
        {"state0", N_("Select quick save state 0"), CONFIG_TYPE_KEY, NULL, TAG_SELECT(0) },
        {"state1", N_("Select quick save state 1"), CONFIG_TYPE_KEY, NULL, TAG_SELECT(1) },
        {"state2", N_("Select quick save state 2"), CONFIG_TYPE_KEY, NULL, TAG_SELECT(2) },
        {"state3", N_("Select quick save state 3"), CONFIG_TYPE_KEY, NULL, TAG_SELECT(3) },
        {"state4", N_("Select quick save state 4"), CONFIG_TYPE_KEY, NULL, TAG_SELECT(4) },
        {"state5", N_("Select quick save state 5"), CONFIG_TYPE_KEY, NULL, TAG_SELECT(5) },
        {"state6", N_("Select quick save state 6"), CONFIG_TYPE_KEY, NULL, TAG_SELECT(6) },
        {"state7", N_("Select quick save state 7"), CONFIG_TYPE_KEY, NULL, TAG_SELECT(7) },
        {"state8", N_("Select quick save state 8"), CONFIG_TYPE_KEY, NULL, TAG_SELECT(8) },
        {"state9", N_("Select quick save state 9"), CONFIG_TYPE_KEY, NULL, TAG_SELECT(9) },
        {NULL, CONFIG_TYPE_NONE}} };

void hotkeys_init() 
{
    hotkeys_register_keys();
}

void hotkeys_register_keys()
{
    input_register_keygroup( &hotkeys_group );
}

void hotkeys_unregister_keys()
{
    input_unregister_keygroup( &hotkeys_group );
}

lxdream_config_group_t hotkeys_get_config()
{
    return &hotkeys_group;
}

static void hotkey_key_callback( void *data, uint32_t value, uint32_t pressure, gboolean isKeyDown )
{
    if( isKeyDown ) {
        switch(value) {
        case TAG_RESUME:
            if( !dreamcast_is_running() )
                gui_do_later(dreamcast_run);
            break;
        case TAG_STOP:
            if( dreamcast_is_running() )
                gui_do_later(dreamcast_stop);
            break;
        case TAG_RESET:
            dreamcast_reset();
            break;
        case TAG_EXIT:
            dreamcast_shutdown();
            exit(0);
            break;
        case TAG_SAVE:
            dreamcast_quick_save();
            break;
        case TAG_LOAD:
            dreamcast_quick_load();
            break;
        default:
            dreamcast_set_quick_state(value- TAG_SELECT(0) );
            break;
        }
    }
}
