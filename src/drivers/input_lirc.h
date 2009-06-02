/**
 * $Id:  $
 *
 * LIRC input device support
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

#ifndef lxdream_input_lirc_H
#define lxdream_input_lirc_H

#ifdef __cplusplus
extern "C" {
#endif
/**
 * Initialize LIRC
 */
void input_lirc_create();

/**
 * Shutdown LIRC
 */
void input_lirc_shutdown();

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_input_lirc_H */
