#ifndef sh4dasm_H
#define sh4dasm_H 1

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

#include <stdio.h>

int sh4_disasm_instruction( int pc, char *buf, int len );
void sh4_disasm_region( FILE *f, int from, int to, int load_addr );
    
#ifdef __cplusplus
}
#endif

#endif
