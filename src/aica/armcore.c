
#include "armcore.h"

struct arm_registers armr;

/* NB: The arm (one assumes) has a different memory map, but for the meantime... */

#define MEM_READ_BYTE( addr ) mem_read_byte(addr)
#define MEM_READ_WORD( addr ) mem_read_word(addr)
#define MEM_READ_LONG( addr ) mem_read_long(addr)
#define MEM_WRITE_BYTE( addr, val ) mem_write_byte(addr, val)
#define MEM_WRITE_WORD( addr, val ) mem_write_word(addr, val)
#define MEM_WRITE_LONG( addr, val ) mem_write_long(addr, val)

#define PC armr.r[15];

void arm_execute_instruction( void ) 
{
  uint32_t ir = MEM_READ_LONG(PC);

#define COND(ir) (ir>>28)

  
}

void arm_execute_thumb_instruction( void )
{
  


}
