#include <stdlib.h>
#include "dream.h"
#include "mem.h"

char *arm_mem = NULL;

void arm_mem_init() {
    arm_mem = mem_get_region_by_name( MEM_REGION_AUDIO );
    
}

int32_t arm_read_long( uint32_t addr ) {
    if( addr < 0x00200000 ) {
	return *(int32_t *)(arm_mem + addr);
	/* Main sound ram */
    } else if( addr >= 0x00800000 && addr <= 0x00803000 ) {
	/* Sound registers / scratch ram */
    } else {
	/* Undefined memory */
	ERROR( "Attempted long read to undefined page: %08X",
	       addr );
	return 0;
    }
}

int16_t arm_read_word( uint32_t addr ) {
    if( addr < 0x00200000 ) {
	return *(int16_t *)(arm_mem + addr);
	/* Main sound ram */
    } else {
	/* Undefined memory */
	ERROR( "Attempted word read to undefined page: %08X",
	       addr );
	return 0;
    }

}

int8_t arm_read_byte( uint32_t addr ) {
    if( addr < 0x00200000 ) {
	return *(int8_t *)(arm_mem + addr);
	/* Main sound ram */
    } else {
	/* Undefined memory */
	ERROR( "Attempted byte read to undefined page: %08X",
	       addr );
	return 0;
    }
}

uint32_t arm_read_long_user( uint32_t addr ) {

}

uint32_t arm_read_byte_user( uint32_t addr ) {

}
