
#ifndef dreamcast_H
#define dreamcast_H 1

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void dreamcast_init(void);
void dreamcast_reset(void);
void dreamcast_stop(void);

#define DREAMCAST_SAVE_MAGIC "%!-DreamOn!Save\0"
#define DREAMCAST_SAVE_VERSION 0x00010000

void dreamcast_save_state( FILE *f );
int dreamcast_load_state( FILE *f );

#ifdef __cplusplus
}
#endif

#endif /* !dream_machine_H */
