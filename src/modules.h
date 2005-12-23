
#ifndef dreamcast_modules_H
#define dreamcast_modules_H 1

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Basic module structure defining the common operations across all
 * modules, ie start, stop, reset, etc. Nothing here is time-sensitive.
 */
typedef struct dreamcast_module {
    char *name;
    /**
     * Perform all initial module setup (ie register / allocate any
     * memory required, etc). Only called once during DreamOn startup
     */
    void (*init)();
    /**
     * Reset the module into it's initial system boot state. Will be called
     * once after init(), as well as whenever the user requests a reset.
     */
    void (*reset)();
    /**
     * Set the module into a running state (may be NULL)
     */
    void (*start)();
    /**
     * Execute one time-slice worth of operations, for the given number of
     * micro-seconds.
     */
    void (*run_time_slice)( int microsecs );
    /**
     * Set the module into a stopped state (may be NULL)
     */
    void (*stop)();
    /**
     * Save the module state to the FILE stream. May be NULL, in which case
     * the module is considered to have no state.
     */
    void (*save)(FILE *);
    /**
     * Load the saved module state from the FILE stream. May be NULL, in which
     * case reset() will be called instead.
     * @return 0 on success, nonzero on failure.
     */
    int (*load)(FILE *);
} *dreamcast_module_t;

void dreamcast_register_module( dreamcast_module_t );

extern struct dreamcast_module mem_module;
extern struct dreamcast_module sh4_module;
extern struct dreamcast_module asic_module;
extern struct dreamcast_module pvr2_module;
extern struct dreamcast_module aica_module;
extern struct dreamcast_module ide_module;
extern struct dreamcast_module maple_module;
extern struct dreamcast_module pvr2_module;
extern struct dreamcast_module gui_module;

void fwrite_string( char *s, FILE *f );
int fread_string( char *s, int maxlen, FILE *f );

#ifdef __cplusplus
}
#endif

#endif /* !dreamcast_modules_H */
