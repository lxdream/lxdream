/**
 * $Id$
 *
 * Window management using EGL.
 *
 * Copyright (c) 2012 Nathan Keynes.
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


#ifndef lxdream_video_egl_H
#define lxdream_video_egl_H 1

#include "glib/gtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <EGL/egl.h>

gboolean video_egl_set_window(EGLNativeWindowType window, int width, int height, int format);
void video_egl_clear_window();

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_video_egl_H */
