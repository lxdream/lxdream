#ifndef cdi_H
#define cdi_H 1

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

#include <stdio.h>

typedef struct cdi_handle *cdi_t;

cdi_t cdi_open( char *filename );

#ifdef __cplusplus
}
#endif

#endif
