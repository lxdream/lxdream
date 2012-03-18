/**
 * $Id$
 *
 * Bounded, blocking queue for inter-thread communication.
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

#include <assert.h>
#include <pthread.h>
#include "tqueue.h"

#define TQUEUE_LENGTH 64

typedef struct {
    tqueue_callback callback;
    void *data;
    gboolean synchronous;
} tqueue_entry;

struct {
    pthread_mutex_t mutex;
    pthread_cond_t consumer_wait;
    pthread_cond_t producer_sync_wait;
    pthread_cond_t producer_full_wait;
    int head;  /* next item returned by dequeue */
    int tail;  /* next item filled in by enqueue */
    int last_result; /* Result value of last dequeued callback */
    tqueue_entry tqueue[TQUEUE_LENGTH];
} tqueue = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0, -1};

/************** Producer thread **************/
#define TQUEUE_EMPTY() (tqueue.head == tqueue.tail)
#define TQUEUE_FULL() ((tqueue.head == tqueue.tail+1) || (tqueue.head == 0 && tqueue.tail == (TQUEUE_LENGTH-1)))

static void tqueue_enqueue( tqueue_callback callback, void *data, gboolean sync )
{
    assert( !TQUEUE_FULL() );
    tqueue.tqueue[tqueue.tail].callback = callback;
    tqueue.tqueue[tqueue.tail].data = data;
    tqueue.tqueue[tqueue.tail].synchronous = sync;
    tqueue.tail++;
    if( tqueue.tail == TQUEUE_LENGTH )
        tqueue.tail = 0;
}

/**
 * Add a message to the UI queue and return immediately.
 */
void tqueue_post_message( tqueue_callback callback, void *data )
{
    pthread_mutex_lock(&tqueue.mutex);
    if( TQUEUE_FULL() ) {
        /* Wait for the queue to clear */
        pthread_cond_wait(&tqueue.producer_full_wait, &tqueue.mutex);
    }
    tqueue_enqueue( callback, data, FALSE );
    pthread_cond_signal(&tqueue.consumer_wait);
    pthread_mutex_unlock(&tqueue.mutex);
}

/**
 * Add a message to the UI queue and wait for it to be handled.
 * @return the result from the handler function.
 */
int tqueue_send_message( tqueue_callback callback, void *data )
{
    int result;
    pthread_mutex_lock(&tqueue.mutex);
    if( TQUEUE_FULL() ) {
        /* Wait for the queue to clear */
        pthread_cond_wait(&tqueue.producer_full_wait, &tqueue.mutex);
    }
    tqueue_enqueue( callback, data, TRUE );
    pthread_cond_signal(&tqueue.consumer_wait);
    pthread_cond_wait(&tqueue.producer_sync_wait, &tqueue.mutex);
    result = tqueue.last_result;
    pthread_mutex_unlock(&tqueue.mutex);
    return result;
}

/************** Consumer thread **************/

/* Note: must be called with mutex locked */
static void tqueue_process_loop() {
    while( !TQUEUE_EMPTY() ) {
        gboolean wasFull = TQUEUE_FULL();
        tqueue_callback callback = tqueue.tqueue[tqueue.head].callback;
        void *data = tqueue.tqueue[tqueue.head].data;
        gboolean sync = tqueue.tqueue[tqueue.head].synchronous;
        tqueue.head++;
        if( tqueue.head == TQUEUE_LENGTH )
            tqueue.head = 0;

        if( wasFull ) {
            pthread_cond_signal( &tqueue.producer_full_wait );
        }

        pthread_mutex_unlock(&tqueue.mutex);
        int result = callback(data);
        pthread_mutex_lock(&tqueue.mutex);
        if( sync ) {
            tqueue.last_result = result;
            pthread_cond_signal( &tqueue.producer_sync_wait );
        }
    }
}

/**
 * Process all messages in the queue, if any.
 */
void tqueue_process_all()
{
    pthread_mutex_lock(&tqueue.mutex);
    if( !TQUEUE_EMPTY() ) {
        tqueue_process_loop();
    }
    pthread_mutex_unlock(&tqueue.mutex);
}

/**
 * Process the first message in the queue. If no messages are on the
 * queue, waits for the next one to be queued and then processes it.
 */
void tqueue_process_wait()
{
    pthread_mutex_lock(&tqueue.mutex);
    if( TQUEUE_EMPTY() ) {
        pthread_cond_wait( &tqueue.consumer_wait, &tqueue.mutex );
    }
    tqueue_process_loop();
    pthread_mutex_unlock(&tqueue.mutex);
}
