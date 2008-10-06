/**
 * $Id: cocoaui.c 863 2008-09-06 05:21:57Z nkeynes $
 *
 * Wrappers for system-dependent functions (mainly path differences)
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

#include <string.h>

#include "lxdream.h"

/* Bundle builds need to use different paths from ordinary builds, since
 * the message catalogs, default config, etc are all bundle-relative.
 * Otherwise paths use the standard unix install paths
 */
#ifdef OSX_BUNDLE

#include <AppKit/AppKit.h>

static char *bundle_resource_path = NULL;

static char *get_bundle_resource_path()
{
    if( bundle_resource_path == NULL ) {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
        bundle_resource_path = strdup([resourcePath UTF8String]);
        [pool release];
    }
    return bundle_resource_path;    
}

const char *get_sysconf_path()
{
    return get_bundle_resource_path();
}

const char *get_locale_path()
{
    return get_bundle_resource_path();
}

#else

const char *get_sysconf_path()
{
    return PACKAGE_CONF_DIR;
}

const char *get_locale_path()
{
    return PACKAGE_LOCALE_DIR;
}

#endif
