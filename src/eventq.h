/**
 * $Id$
 *
 * Simple implementation of one-shot timers. Effectively this allows IO
 * devices to wait until a particular time before completing. We expect 
 * there to be at least half a dozen or so continually scheduled events
 * (TMU and PVR2), peaking around 20+.
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

#include "dream.h"

#define NOT_SCHEDULED 0xFFFFFFFF /* Sentinel value */


typedef void (*event_func_t)(int eventid);

/**
 * Register the callback to be associated with the given event ID.
 * Note: These should be registered at init time and never changed.
 */
void register_event_callback( int eventid, event_func_t func );

/**
 * Schedule a new pending event.
 * @param eventid Unique ID identifying the event in question (used to remove it
 * at a later time). If the eventid is scheduled more than once, only the lastest
 * schedule for that ID will be valid.
 * @param nanosecs Nanoseconds from the current SH4 time at which the event
 * should occur.
 */
void event_schedule( int eventid, uint32_t nanosecs );

/**
 * Schedule a long-duration pending event
 */
void event_schedule_long( int eventid, uint32_t seconds, uint32_t nanosecs );

/**
 * Remove a previously created event without triggering it. This is usually
 * only used when an operation is aborted.
 */
void event_cancel( int eventid );

/**
 * Return the slice cycle time of the next event, or NOT_SCHEDULED
 * if no events are scheduled for this time slice.
 */
uint32_t event_get_next_time();

/**
 * Execute the event on the top of the queue, and remove it.
 */
void event_execute();

#define MAX_EVENT_ID 128

/* Events 1..96 are defined as the corresponding ASIC events. */


