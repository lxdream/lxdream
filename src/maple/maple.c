#include <assert.h>
#include "dream.h"
#include "mem.h"
#include "asic.h"
#include "maple.h"

void maple_handle_buffer( uint32_t address ) {
    uint32_t *buf = (uint32_t *)mem_get_region(address);
    if( buf == NULL ) {
        ERROR( "Invalid or unmapped buffer passed to maple (0x%08X)", address );
    } else {
        int last, port, length, cmd, recv_addr, send_addr, add_length;
        int i = 0;
        do {
            last = buf[i]>>31; /* indicates last packet */
            port = (buf[i]>>16)&0x03;
            length = buf[i]&0x0F; 
            uint32_t return_address = buf[i+1];
            cmd = buf[i+2]&0xFF;
            recv_addr = (buf[i+2]>>8)&0xFF;
            send_addr = (buf[i+2]>>16)&0xFF;
            add_length = (buf[i+2]>>24)&0xFF;
            char *return_buf = mem_get_region(return_address);
            
        } while( !last );
    }
}
