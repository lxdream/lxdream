#include <stdint.h>

#define PORT_R 1
#define PORT_W 2
#define PORT_MEM 4 /* store written value */
#define PORT_RW 3
#define PORT_MRW 7
#define UNDEFINED 0

struct mmio_region {
    char *id, *desc;
    uint32_t base;
    char *mem;
    struct mmio_port {
        char *id, *desc;
        int width;
        uint32_t offset;
        uint32_t default;
        int flags;
    } *ports;
};

#define _MACROIZE #define

#define MMIO_REGION_BEGIN(b,id,d) struct mmio_region mmio_region_##id = { #id, d, b, NULL, 
#define LONG_PORT( o,id,f,def,d ) { #id, desc, 32, o, def, f }, \
_MACROIZE port_##id o \
_MACROIZE reg_##id  (*(uint32_t *)(mmio_region_##id.mem + o))
#define WORD_PORT( o,id,f,def,d ) { #id, desc, 16, o, def, f },
#define BYTE_PORT( o,id,f,def,d ) { #id, desc, 8, o, def, f },
#define MMIO_REGION_END {NULL, NULL, 0, 0, 0} };

MMIO_REGION_BEGIN( 0xFF000000, MMU, "MMU Registers" )
    LONG_PORT( 0x000, PTEH, PORT_MRW, UNDEFINED, "Page table entry high" ),
    LONG_PORT( 0x004, PTEL, PORT_MRW, UNDEFINED, "Page table entry low" ),
MMIO_REGION_END

MMIO_REGION_BEGIN( BSC, 0xFF800000, "I/O Port Registers" )
    LONG_PORT( 0x000, BCR1, PORT_MRW, 0, "" ),
    WORD_PORT( 0x004, BCR2, PORT_MRW, 0x3FFC, "" ),
    LONG_PORT( 0x008, WCR1, PORT_MRW, 0x77777777, "" ),
    LONG_PORT( 0x00C, WCR2, PORT_MRW, 0xFFFEEFFF, "" ),
    LONG_PORT( 0x010, WCR3, PORT_MRW, 0x07777777, "" ),
    LONG_PORT( 0x02C, PCTRA, PORT_MRW, 0, "Port control register A" ),
    WORD_PORT( 0x030, PDTRA, PORT_RW, UNDEFINED, "Port data register A" ),
    LONG_PORT( 0x040, PCTRB, PORT_MRW, 0, "Port control register B" ),
    WORD_PORT( 0x044, PCTRB, PORT_RW, UNDEFINED, "Port data register B" ),
    WORD_PORT( 0x048, GPIOIC, PORT_MRW, 0, "GPIO interrupt control register" )
MMIO_REGION_END

MMIO_REGION_BEGIN( SCI, 0xFFE00000, "Serial Controller Registers" )
    
MMIO_REGION_END

MMIO_REGIN_BEGIN( SCIF, 0xFFE80000, "Serial Controller (FIFO) Registers" )
MMIO_REGION_END

