
#ifndef dream_disasm_H
#define dream_disasm_H 1

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Disassembly function pointer typedef.
 *
 * @param pc Address to disassemble
 * @param buffer String buffer to write disassembly into
 * @param buflen Maximum length of buffer
 * @return next address to disassemble
 */
typedef uint32_t (*disasm_func_t)(uint32_t pc, char *buffer, int buflen, char *opcode );

typedef int (*is_valid_page_t)(uint32_t pc);

#define REG_INT 0
#define REG_FLT 1
#define REG_SPECIAL 2

/**
 * Structure that defines a single register in a CPU for display purposes.
 */
typedef struct reg_desc_struct {
    char *name;
    int type;
    void *value;
} reg_desc_t;

typedef struct cpu_desc_struct {
  char *name; /* CPU Name */
  disasm_func_t disasm_func; /* Disassembly function */
  size_t instr_size; /* Size of instruction */
  char *regs; /* Pointer to start of registers */
  size_t regs_size; /* Size of register structure in bytes */
  const struct reg_desc_struct *regs_info; /* Description of all registers */
  uint32_t *pc; /* Pointer to PC register */
  uint32_t *icount; /* Pointer to instruction counter */
  /* Memory map? */
    is_valid_page_t valid_page_func; /* Test for valid memory page */
} *cpu_desc_t;

#ifdef __cplusplus
}
#endif

#endif /* !dream_disasm_H */
