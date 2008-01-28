/**
 * $Id: joy_linux.c,v 1.12 2007-11-08 11:54:16 nkeynes Exp $
 *
 * Linux joystick input device support
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

#ifndef lxdream_joy_linux_H
#define lxdream_joy_linux_H

/**
 * Initialize the linux joystick code, and scan for devices
 */
gboolean linux_joystick_init();

/**
 * Re-scan the available joystick devices, adding any new ones.
 */
gboolean linux_joystick_rescan();

/**
 * Shutdown the linux joystick system
 */
void linux_joystick_shutdown();



#endif /* !lxdream_joy_linux_H */
