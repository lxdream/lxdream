#ifndef sh4dasm_H
#define sh4dasm_H 1

#include "disasm.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

uint32_t sh4_disasm_instruction( uint32_t pc, char *buf, int len );
void sh4_disasm_region( FILE *f, int from, int to, int load_addr );

extern struct cpu_desc_struct sh4_cpu_desc;

#ifdef __cplusplus
}
#endif

#endif
