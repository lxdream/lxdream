/**
 * $Id$
 *
 * CD Bootstrap header parsing. Mostly for informational purposes.
 *
 * Copyright (c) 2005 Nathan Keynes.
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

/*
 * IP.BIN related code. Ref: http://mc.pp.se/dc/ip0000.bin.html
 */
#ifndef lxdream_bootstrap_H
#define lxdream_bootstrap_H 1

#include "lxdream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dump the bootstrap info to the output log for infomational/debugging
 * purposes.
 */
void bootstrap_dump(void *data, gboolean detail);

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_bootstrap_H */
