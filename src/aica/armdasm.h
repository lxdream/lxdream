#ifndef armdasm_H
#define armdasm_H 1

#include "disasm.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

uint32_t arm_disasm_instruction( uint32_t pc, char *buf, int len, char * );
uint32_t armt_disasm_instruction( uint32_t pc, char *buf, int len, char * );
extern const struct cpu_desc_struct arm_cpu_desc;
extern const struct cpu_desc_struct armt_cpu_desc;

#ifdef __cplusplus
}
#endif

#endif
