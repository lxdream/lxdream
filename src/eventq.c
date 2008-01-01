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

#include <assert.h>
#include "dreamcast.h"
#include "eventq.h"
#include "asic.h"
#include "sh4core.h"

#define LONG_SCAN_PERIOD 1000000000 /* 1 second */

typedef struct event {
    uint32_t id;
    uint32_t seconds;
    uint32_t nanosecs;
    event_func_t func;

    struct event *next;
} *event_t;

static struct event events[MAX_EVENT_ID];

/**
 * Countdown to the next scan of the long-duration list (greater than 1 second).
 */
static int long_scan_time_remaining;

static event_t event_head;
static event_t long_event_head;

void event_reset();
void event_init();
uint32_t event_run_slice( uint32_t nanosecs );
void event_save_state( FILE *f );
int event_load_state( FILE * f );

struct dreamcast_module eventq_module = { "EVENTQ", event_init, event_reset, NULL, event_run_slice,
					NULL, event_save_state, event_load_state };

static void event_update_pending( ) 
{
    if( event_head == NULL ) {
	if( !(sh4r.event_types & PENDING_IRQ) ) {
	    sh4r.event_pending = NOT_SCHEDULED;
	}
	sh4r.event_types &= (~PENDING_EVENT);
    } else {
	if( !(sh4r.event_types & PENDING_IRQ) ) {
	    sh4r.event_pending = event_head->nanosecs;
	}
	sh4r.event_types |= PENDING_EVENT;
    }
}

uint32_t event_get_next_time( ) 
{
    if( event_head == NULL ) {
	return NOT_SCHEDULED;
    } else {
	return event_head->nanosecs;
    }
}

/**
 * Add the event to the short queue.
 */
static void event_enqueue( event_t event ) 
{
    if( event_head == NULL || event->nanosecs < event_head->nanosecs ) {
	event->next = event_head;
	event_head = event;
	event_update_pending();
    } else {
	event_t cur = event_head;
	event_t next = cur->next;
	while( next != NULL && event->nanosecs >= next->nanosecs ) {
	    cur = next;
	    next = cur->next;
	}
	event->next = next;
	cur->next = event;
    }
}

static void event_dequeue( event_t event )
{
    if( event_head == NULL ) {
	ERROR( "Empty event queue but should contain event %d", event->id );
    } else if( event_head == event ) {
	/* removing queue head */
	event_head = event_head->next;
	event_update_pending();
    } else {
	event_t cur = event_head;
	event_t next = cur->next;
	while( next != NULL ) {
	    if( next == event ) {
		cur->next = next->next;
		break;
	    }
	    cur = next;
	    next = cur->next;
	}
    }
}

static void event_dequeue_long( event_t event ) 
{
    if( long_event_head == NULL ) {
	ERROR( "Empty long event queue but should contain event %d", event->id );
    } else if( long_event_head == event ) {
	/* removing queue head */
	long_event_head = long_event_head->next;
    } else {
	event_t cur = long_event_head;
	event_t next = cur->next;
	while( next != NULL ) {
	    if( next == event ) {
		cur->next = next->next;
		break;
	    }
	    cur = next;
	    next = cur->next;
	}
    }
}

void register_event_callback( int eventid, event_func_t func )
{
    events[eventid].func = func;
}

void event_schedule( int eventid, uint32_t nanosecs )
{
    nanosecs += sh4r.slice_cycle;

    event_t event = &events[eventid];

    if( event->nanosecs != NOT_SCHEDULED ) {
	/* Event is already scheduled. Remove it from the list first */
	event_cancel(eventid);
    }

    event->id = eventid;
    event->seconds = 0;
    event->nanosecs = nanosecs;
    
    event_enqueue( event );
}

void event_schedule_long( int eventid, uint32_t seconds, uint32_t nanosecs ) {
    if( seconds == 0 ) {
	event_schedule( eventid, nanosecs );
    } else {
	event_t event = &events[eventid];

	if( event->nanosecs != NOT_SCHEDULED ) {
	    /* Event is already scheduled. Remove it from the list first */
	    event_cancel(eventid);
	}

	event->id = eventid;
	event->seconds = seconds;
	event->nanosecs = nanosecs;
	event->next = long_event_head;
	long_event_head = event;
    }
	
}

void event_cancel( int eventid )
{
    event_t event = &events[eventid];
    if( event->nanosecs == NOT_SCHEDULED ) {
	return; /* not scheduled */
    } else {
	event->nanosecs = NOT_SCHEDULED;
	if( event->seconds != 0 ) { /* long term event */
	    event_dequeue_long( event );
	} else {
	    event_dequeue( event );
	}
    }
}


void event_execute()
{
    /* Loop in case we missed some or got a couple scheduled for the same time */
    while( event_head != NULL && event_head->nanosecs <= sh4r.slice_cycle ) {
	event_t event = event_head;
	event_head = event->next;
	event->nanosecs = NOT_SCHEDULED;
	// Note: Make sure the internal state is consistent before calling the
	// user function, as it will (quite likely) enqueue another event.
	event->func( event->id );
    }

    event_update_pending();
}

void event_asic_callback( int eventid )
{
    asic_event( eventid );
}

void event_init()
{
    int i;
    for( i=0; i<MAX_EVENT_ID; i++ ) {
	events[i].id = i;
	events[i].nanosecs = NOT_SCHEDULED;
	if( i < 96 ) {
	    events[i].func = event_asic_callback;
	} else {
	    events[i].func = NULL;
	}
	events[i].next = NULL;
    }
    event_head = NULL;
    long_event_head = NULL;
    long_scan_time_remaining = LONG_SCAN_PERIOD;
}



void event_reset()
{
    int i;
    event_head = NULL;
    long_event_head = NULL;
    long_scan_time_remaining = LONG_SCAN_PERIOD;
    for( i=0; i<MAX_EVENT_ID; i++ ) {
	events[i].nanosecs = NOT_SCHEDULED;
    }
}

void event_save_state( FILE *f )
{
    int id, i;
    id = event_head == NULL ? -1 : event_head->id;
    fwrite( &id, sizeof(id), 1, f );
    id = long_event_head == NULL ? -1 : long_event_head->id;
    fwrite( &id, sizeof(id), 1, f );
    fwrite( &long_scan_time_remaining, sizeof(long_scan_time_remaining), 1, f );
    for( i=0; i<MAX_EVENT_ID; i++ ) {
	fwrite( &events[i].id, sizeof(uint32_t), 3, f ); /* First 3 words from structure */
	id = events[i].next == NULL ? -1 : events[i].next->id;
	fwrite( &id, sizeof(id), 1, f );
    }
}

int event_load_state( FILE *f )
{
    int id, i;
    fread( &id, sizeof(id), 1, f );
    event_head = id == -1 ? NULL : &events[id];
    fread( &id, sizeof(id), 1, f );
    long_event_head = id == -1 ? NULL : &events[id];
    fread( &long_scan_time_remaining, sizeof(long_scan_time_remaining), 1, f );
    for( i=0; i<MAX_EVENT_ID; i++ ) {
	fread( &events[i].id, sizeof(uint32_t), 3, f );
	fread( &id, sizeof(id), 1, f );
	events[i].next = id == -1 ? NULL : &events[id];
    }
    return 0;
}

/**
 * Scan all entries in the long queue, decrementing each by 1 second. Entries
 * that are now < 1 second are moved to the short queue.
 */
static void event_scan_long()
{
    while( long_event_head != NULL && --long_event_head->seconds == 0 ) {
	event_t event = long_event_head;
	long_event_head = event->next;
	event_enqueue(event);
    }

    if( long_event_head != NULL ) {
	event_t last = long_event_head;
	event_t cur = last->next;
	while( cur != NULL ) {
	    if( --cur->seconds == 0 ) {
		last->next = cur->next;
		event_enqueue(cur);
	    } else {
		last = cur;
	    }
	    cur = last->next;
	}
    }
}

/**
 * Decrement the event time on all pending events by the supplied nanoseconds.
 * It may or may not be faster to wrap around instead, but this has the benefit
 * of simplicity.
 */
uint32_t event_run_slice( uint32_t nanosecs )
{
    event_t event = event_head;
    while( event != NULL ) {
	if( event->nanosecs <= nanosecs ) {
	    event->nanosecs = 0;
	} else {
	    event->nanosecs -= nanosecs;
	}
	event = event->next;
    }

    long_scan_time_remaining -= nanosecs;
    if( long_scan_time_remaining <= 0 ) {
	long_scan_time_remaining += LONG_SCAN_PERIOD;
	event_scan_long();
    }

    event_update_pending();
    return nanosecs;
}

