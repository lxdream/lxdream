/**
 * $Id$
 *
 * Native shims for the Android front-end (implemented in Java).
 *
 * This is complicated by the fact that all the emulation runs in a
 * separate thread.
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

#include <jni.h>
#include <pthread.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <libisofs.h>
#include "dream.h"
#include "dreamcast.h"
#include "gui.h"
#include "config.h"
#include "lxpaths.h"
#include "tqueue.h"
#include "display.h"
#include "gdlist.h"
#include "gdrom/gdrom.h"
#include "hotkeys.h"
#include "serial.h"
#include "aica/audio.h"
#include "drivers/video_egl.h"
#include "maple/maple.h"
#include "vmu/vmulist.h"

struct surface_info {
    ANativeWindow *win;
    int width, height, format;
};

static struct surface_info current_surface;
static const char *appHome = NULL;

/**
 * Count of running nanoseconds - used to cut back on the GUI runtime
 */
static uint32_t android_gui_nanos = 0;
static uint32_t android_gui_ticks = 0;
static struct timeval android_gui_lasttv;


void android_gui_start( void )
{
    /* Dreamcast starting up hook */
}

void android_gui_stop( void )
{
    /* Dreamcast stopping hook */
}

void android_gui_reset( void )
{
    /* Dreamcast reset hook */
}

/**
 * The main emulation thread. (as opposed to the UI thread).
 */
void *android_thread_main(void *data)
{
    while(1) {
        tqueue_process_wait();
    }
    return NULL;
}

int android_set_surface(void *data)
{
    struct surface_info *surface = (struct surface_info *)data;
    video_egl_set_window(surface->win, surface->width, surface->height, surface->format);
    return 0;
}

int android_clear_surface(void *data)
{
    struct surface_info *surface = (struct surface_info *)data;

    if( dreamcast_is_running() ) {
        dreamcast_stop();
    }
    video_egl_clear_window();
    ANativeWindow_release(surface->win);
    surface->win = NULL;
    return 0;
}

static pthread_t dreamcast_thread;

void android_start_thread()
{
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    int status = pthread_create(&dreamcast_thread, &attr, android_thread_main, NULL);
    if( status != 0 ) {
        /* Handle errors */
    }
}

/** tqueue callback wrapper to get the right call type for simple events */
int android_callback_wrapper( void *fn )
{
    void (*cast_fn)(void) = fn;
    cast_fn();
}

int android_mount_disc( void *data )
{
    char *s = (char *)data;
    ERROR err;
    gboolean result = gdrom_mount_image( s, &err ); /* TODO: Report error */
    return result;
}

int android_toggle_run( void *data )
{
    if( dreamcast_is_running() ) {
        dreamcast_stop();
    } else {
        dreamcast_run();
    }
}

uint32_t android_gui_run_slice( uint32_t nanosecs )
{
    android_gui_nanos += nanosecs;
    if( android_gui_nanos > GUI_TICK_PERIOD ) { /* 10 ms */
        android_gui_nanos -= GUI_TICK_PERIOD;
        android_gui_ticks ++;
        uint32_t current_period = android_gui_ticks * GUI_TICK_PERIOD;

        // Run the event loop
        tqueue_process_all();

        struct timeval tv;
        gettimeofday(&tv,NULL);
        uint32_t ns = ((tv.tv_sec - android_gui_lasttv.tv_sec) * 1000000000) +
        (tv.tv_usec - android_gui_lasttv.tv_usec)*1000;
        if( (ns * 1.05) < current_period ) {
            // We've gotten ahead - sleep for a little bit
            struct timespec tv;
            tv.tv_sec = 0;
            tv.tv_nsec = current_period - ns;
            nanosleep(&tv, &tv);
        }

#if 0
        /* Update the display every 10 ticks (ie 10 times a second) and
         * save the current tv value */
        if( android_gui_ticks > 10 ) {
            android_gui_ticks -= 10;

            double speed = (float)( (double)current_period * 100.0 / ns );
            android_gui_lasttv.tv_sec = tv.tv_sec;
            android_gui_lasttv.tv_usec = tv.tv_usec;
            main_window_set_speed( main_win, speed );
        }
#endif
    }
    return nanosecs;


}

struct dreamcast_module android_gui_module = { "gui", NULL,
        android_gui_reset,
        android_gui_start,
        android_gui_run_slice,
        android_gui_stop,
        NULL, NULL };

gboolean gui_error_dialog( const char *fmt, ... )
{
    return FALSE; /* TODO */
}

void gui_update_state()
{
    /* TODO */
}

void gui_set_use_grab( gboolean grab )
{
    /* No implementation - mouse grab doesn't exist */
}

void gui_update_io_activity( io_activity_type activity, gboolean active )
{
    /* No implementation */
}

void gui_do_later( do_later_callback_t func )
{
    func(); /* TODO */
}

static void android_init( const char *appHomeDir )
{
    set_global_log_level("info");
    appHome = appHomeDir;
    const char *confFile = g_strdup_printf("%s/lxdreamrc", appHome);
    set_user_data_path(appHome);
    lxdream_set_config_filename( confFile );
    lxdream_make_config_dir( );
    lxdream_load_config( );
    iso_init();
    gdrom_list_init();
    vmulist_init();
    dreamcast_init(1);

    dreamcast_register_module( &android_gui_module );
    audio_init_driver(NULL);
    display_driver_t display_driver = get_display_driver_by_name(NULL);
    display_set_driver(display_driver);

    hotkeys_init();
    serial_init();
    maple_reattach_all();
    INFO( "%s! ready...", APP_NAME );
    android_start_thread();
}


/************************* Dreamcast native entry points **************************/

static char *getStringChars( JNIEnv *env, jstring str );

/**
 * Main initialization entry point. We need to do all the setup that main()
 * would normally do, as well as any UI specific setup.
 */
JNIEXPORT void JNICALL Java_org_lxdream_Dreamcast_init(JNIEnv * env, jclass obj,  jstring homeDir )
{
    android_init( getStringChars(env, homeDir) );
}

JNIEXPORT void JNICALL Java_org_lxdream_Dreamcast_run(JNIEnv * env, jclass obj)
{
    tqueue_post_message( android_callback_wrapper, dreamcast_run );
}

JNIEXPORT void JNICALL Java_org_lxdream_Dreamcast_toggleRun(JNIEnv * env, jclass obj)
{
    tqueue_post_message( android_toggle_run, NULL );
}

JNIEXPORT void JNICALL Java_org_lxdream_Dreamcast_reset(JNIEnv * env, jclass obj)
{
    tqueue_post_message( android_callback_wrapper, dreamcast_reset );
}

JNIEXPORT void JNICALL Java_org_lxdream_Dreamcast_stop(JNIEnv * env, jclass obj)
{
    /* Need to make sure this completely shuts down before we return */
    tqueue_send_message( android_callback_wrapper, dreamcast_stop );
}

JNIEXPORT jboolean JNICALL Java_org_lxdream_Dreamcast_isRunning(JNIEnv *env, jclass obj)
{
    return dreamcast_is_running();
}

JNIEXPORT jboolean JNICALL Java_org_lxdream_Dreamcast_isRunnable(JNIEnv *env, jclass obj)
{
    return dreamcast_can_run();
}

JNIEXPORT jboolean JNICALL Java_org_lxdream_Dreamcast_mount(JNIEnv *env, jclass obj, jstring str)
{
    char *s = getStringChars(env, str);
    return tqueue_send_message( android_mount_disc, s );
}

JNIEXPORT jboolean JNICALL Java_org_lxdream_Dreamcast_unmount(JNIEnv *env, jclass obj)
{
    tqueue_post_message( android_callback_wrapper, gdrom_unmount_disc );
}


/************************* LxdreamView native entry points **************************/

JNIEXPORT void JNICALL Java_org_lxdream_LxdreamView_setSurface(JNIEnv * env, jobject view, jobject surface, jint width, jint height)
{
    current_surface.win = ANativeWindow_fromSurface(env, surface);
    current_surface.width = width;
    current_surface.height = height;
    int fmt = ANativeWindow_getFormat(current_surface.win);
    if( fmt == WINDOW_FORMAT_RGB_565 ) {
        current_surface.format = COLFMT_RGB565;
    } else {
        current_surface.format = COLFMT_RGB888;
    }
    tqueue_post_message( android_set_surface, &current_surface );
}

JNIEXPORT void JNICALL Java_org_lxdream_LxdreamView_clearSurface(JNIEnv * env, jobject view, jobject surface)
{
    /* Need to make sure this completely shuts down before we return */
    tqueue_send_message( android_clear_surface, &current_surface );
}


/************************* JNI Support functions **************************/

static char *getStringChars( JNIEnv *env, jstring str )
{
    jboolean iscopy;
    const char *p = (*env)->GetStringUTFChars(env, str, &iscopy);
    char *result = strdup(p);
    (*env)->ReleaseStringUTFChars(env,str,p);
    return result;
}
