/**
 * $Id:  $
 *
 * Handles hotkeys for pause/continue, save states, quit, etc
 *
 * Copyright (c) 2009 wahrhaft.
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

#ifndef lxdream_hotkeys_H
#define lxdream_hotkeys_H

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

        void hotkeys_init();
        lxdream_config_entry_t hotkeys_get_config();
        void hotkeys_register_keys();
        void hotkeys_unregister_keys();

#ifdef __cplusplus
}
#endif

#endif
