/**
 * $Id$
 *
 * Bounded, blocking queue for inter-thread communication. Note: consumer side is
 * re-entrant.
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

#ifndef lxdream_tqueue_H
#define lxdream_tqueue_H 1

#include "glib/gtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback function to be invoked on the consumer side.
 */
typedef int (*tqueue_callback)(void *);

/**
 * Add a message to the UI queue and return immediately.
 */
void tqueue_post_message( tqueue_callback callback, void *data );

/**
 * Add a message to the UI queue and wait for it to be handled.
 * @return the result from the handler function.
 */
int tqueue_send_message( tqueue_callback callback, void *data );

/************** Consumer thread **************/

/**
 * Process all messages in the queue, if any.
 */
void tqueue_process_all();

/**
 * Process the first message in the queue. If no messages are on the
 * queue, waits for the next one to be queued and then processes it.
 */
void tqueue_process_wait();

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_tqueue_H */
