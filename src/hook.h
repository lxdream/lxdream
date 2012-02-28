/**
 * $Id$
 *
 * This file defines some useful generic macros for hooks
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

#ifndef lxdream_hook_H
#define lxdream_hook_H 1

#include <assert.h>

/**
 * Hook functions are generally useful, so we'd like to limit the overhead (and 
 * opportunity for stupid bugs) by minimizing the amount of code involved. Glib
 * has GHook (and of course signals), but they don't actually simplify anything
 * at this level.
 * 
 * Hence, the gratuitous macro abuse here.
 * 
 * Usage:
 * 
 * In header file:
 * 
 * DECLARE_HOOK( hook_name, hook_fn_type );
 * 
 * In implementation file:
 * 
 * DEFINE_HOOK( hook_name, hook_fn_type );
 *
 */
#define DECLARE_HOOK( name, fn_type ) \
    void register_##name( fn_type fn, void *user_data ); \
    void unregister_##name( fn_type fn, void *user_data )

#define FOREACH_HOOK( h, name ) struct name##_hook_struct *h; for( h = name##_hook_list; h != NULL; h = h->next )

#define CALL_HOOKS0( name ) FOREACH_HOOK(h,name) { h->fn(h->user_data); }
#define CALL_HOOKS( name, args... ) FOREACH_HOOK(h, name) { h->fn(args, h->user_data); } 
    
#define DEFINE_HOOK( name, fn_type ) \
    struct name##_hook_struct { \
        fn_type fn; \
        void *user_data; \
        struct name##_hook_struct *next; \
    } *name##_hook_list = NULL; \
    void register_##name( fn_type fn, void *user_data ) { \
        struct name##_hook_struct *h = malloc(sizeof(struct name##_hook_struct)); \
        assert(h != NULL); \
        h->fn = fn; \
        h->user_data = user_data; \
        h->next = name##_hook_list; \
        name##_hook_list = h; \
    } \
    void unregister_##name( fn_type fn, void *user_data ) { \
        struct name##_hook_struct *last = NULL, *h = name##_hook_list; \
        while( h != NULL )  { \
            if( h->fn == fn && h->user_data == user_data ) { \
                if( last == NULL ) { \
                    name##_hook_list = h->next; \
                } else { \
                    last->next = h->next; \
                } \
                free( h ); \
            } \
            last = h; \
            h = h->next; \
        }\
    }
    

#endif /* !lxdream_hook_H */
