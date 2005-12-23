
#ifndef dreamcast_H
#define dreamcast_H 1

#include <stdlib.h>
#include <stdio.h>
#include <glib/gtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DREAMCAST_SAVE_MAGIC "%!-DreamOn!Save\0"
#define DREAMCAST_SAVE_VERSION 0x00010000

#define TIMESLICE_LENGTH 1000 /* microseconds */

#define STATE_RUNNING 1
#define STATE_STOPPING 2
#define STATE_STOPPED 3 

void dreamcast_init(void);
void dreamcast_reset(void);
void dreamcast_run(void);
void dreamcast_stop(void);

int dreamcast_save_state( const gchar *filename );
int dreamcast_load_state( const gchar *filename );

int open_file( gchar *filename );
int load_bin_file( gchar *filename );

#ifdef __cplusplus
}
#endif

#endif /* !dream_machine_H */
