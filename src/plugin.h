/**
 * $Id$
 *
 * Plugin declarations and support. 
 * 
 * Note plugins mainly exist to make binary packagers' lives easier, 
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

#ifndef lxdream_plugin_H
#define lxdream_plugin_H

#include "lxdream.h"

#ifdef __cplusplus
extern "C" {
#endif

enum plugin_type {
    PLUGIN_NONE = 0,
    PLUGIN_AUDIO_DRIVER = 1,
    PLUGIN_INPUT_DRIVER = 2,
};

#define PLUGIN_MIN_TYPE 1
#define PLUGIN_MAX_TYPE PLUGIN_INPUT_DRIVER
    
struct plugin_struct {
    enum plugin_type type;
    const char *name;
    const char *version;
    
    /**
     * Plugin registration function, called at load time (dynamic modules) or 
     * startup (static modules). This should register with the appropriate 
     * driver list. 
     * @return TRUE on success, FALSE on failure (although exactly how this 
     * can fail is unclear).
     */
    gboolean (*register_plugin)(void);
};

#define CONSTRUCTOR __attribute__((constructor))

#ifdef PLUGIN
#define DEFINE_PLUGIN(type,name,fn) struct plugin_struct lxdream_plugin_entry = { type, name, VERSION, fn }
#define AUDIO_DRIVER(name, driver) static gboolean __lxdream_plugin_init(void) { return audio_register_driver(&(driver)); }  \
    DEFINE_PLUGIN(PLUGIN_AUDIO_DRIVER, name, __lxdream_plugin_init)

#else /* !ENABLE_SHARED */
#define AUDIO_DRIVER(name,driver) static void CONSTRUCTOR __lxdream_static_constructor(void) { audio_register_driver(&(driver)); } 
#define DEFINE_PLUGIN(type,name,fn) static void CONSTRUCTOR __lxdream_static_constructor(void) { fn(); }

#endif /* ENABLE_SHARED */

gboolean plugin_init();

#ifdef __cplusplus
}
#endif
    
#endif /* !lxdream_plugin_H */
