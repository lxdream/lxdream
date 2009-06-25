/**
 * $Id:  $
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

static void hotkey_resume_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown );
static void hotkey_stop_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown );
static void hotkey_reset_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown );
static void hotkey_exit_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown );
static void hotkey_state_select_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown );
static void hotkey_state_save_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown );
static void hotkey_state_load_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown );


struct lxdream_config_entry hotkeys_config[] = {
        {"resume", N_("Resume emulation"), CONFIG_TYPE_KEY},
        {"stop", N_("Stop emulation"), CONFIG_TYPE_KEY},
        {"reset", N_("Reset emulator"), CONFIG_TYPE_KEY},
        {"exit", N_("Exit emulator"), CONFIG_TYPE_KEY},
        {"save", N_("Save current quick save"), CONFIG_TYPE_KEY},
        {"load", N_("Load current quick save"), CONFIG_TYPE_KEY},
        {"state0", N_("Select quick save state 0"), CONFIG_TYPE_KEY},
        {"state1", N_("Select quick save state 1"), CONFIG_TYPE_KEY},
        {"state2", N_("Select quick save state 2"), CONFIG_TYPE_KEY},
        {"state3", N_("Select quick save state 3"), CONFIG_TYPE_KEY},
        {"state4", N_("Select quick save state 4"), CONFIG_TYPE_KEY},
        {"state5", N_("Select quick save state 5"), CONFIG_TYPE_KEY},
        {"state6", N_("Select quick save state 6"), CONFIG_TYPE_KEY},
        {"state7", N_("Select quick save state 7"), CONFIG_TYPE_KEY},
        {"state8", N_("Select quick save state 8"), CONFIG_TYPE_KEY},
        {"state9", N_("Select quick save state 9"), CONFIG_TYPE_KEY},
        {NULL, CONFIG_TYPE_NONE}
};

void hotkeys_init() 
{
    hotkeys_register_keys();
}

void hotkeys_register_keys()
{
    input_register_key(hotkeys_config[0].value, &hotkey_resume_callback, NULL, 0);
    input_register_key(hotkeys_config[1].value, &hotkey_stop_callback, NULL, 0);
    input_register_key(hotkeys_config[2].value, &hotkey_reset_callback, NULL, 0);
    input_register_key(hotkeys_config[3].value, &hotkey_exit_callback, NULL, 0);
    input_register_key(hotkeys_config[4].value, &hotkey_state_save_callback, NULL, 0);
    input_register_key(hotkeys_config[5].value, &hotkey_state_load_callback, NULL, 0);
    for (int i = 0; i < 10; i++)
    {
        input_register_key(hotkeys_config[6 + i].value, &hotkey_state_select_callback, NULL, i);
    }
}

void hotkeys_unregister_keys()
{
    input_unregister_key(hotkeys_config[0].value, &hotkey_resume_callback, NULL, 0);
    input_unregister_key(hotkeys_config[1].value, &hotkey_stop_callback, NULL, 0);
    input_unregister_key(hotkeys_config[2].value, &hotkey_reset_callback, NULL, 0);
    input_unregister_key(hotkeys_config[3].value, &hotkey_exit_callback, NULL, 0);
    input_unregister_key(hotkeys_config[4].value, &hotkey_state_save_callback, NULL, 0);
    input_unregister_key(hotkeys_config[5].value, &hotkey_state_load_callback, NULL, 0);
    for (int i = 0; i < 10; i++)
    {
        input_unregister_key(hotkeys_config[6 + i].value, &hotkey_state_select_callback, NULL, i);
    }
}

lxdream_config_entry_t hotkeys_get_config()
{
    return hotkeys_config;
}


static void hotkey_resume_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown )
{
    if (isKeyDown && !dreamcast_is_running() ) {
        gui_do_later(dreamcast_run);
    }
}

static void hotkey_stop_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown )
{
    if (isKeyDown && dreamcast_is_running() ) {
        gui_do_later(dreamcast_stop);
    }
}

static void hotkey_reset_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown )
{
    if (isKeyDown) {
        dreamcast_reset();
    }
}

static void hotkey_exit_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown )
{
    if (isKeyDown) {
        dreamcast_shutdown();
    }
    exit(0);
}

static void hotkey_state_select_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown )
{
    if (isKeyDown) {
        dreamcast_set_quick_state(value);
    }
}

static void hotkey_state_save_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown )
{
    if (isKeyDown) {
        dreamcast_quick_save();
    }
}

static void hotkey_state_load_callback( void *mdev, uint32_t value, uint32_t pressure, gboolean isKeyDown )
{
    if (isKeyDown) {
        dreamcast_quick_load();
    }
}


