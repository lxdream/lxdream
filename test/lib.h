#ifndef dc_lib_H
#define dc_lib_H

#include <stdio.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef volatile unsigned int vuint32_t;
typedef volatile unsigned short vuint16_t;

#define ASIC_STATUS0    0xA05F6900
#define ASIC_STATUS1    0xA05F6904
#define ASIC_STATUS2    0xA05F6908
#define ASIC_IRQB0      0xA05F6920
#define ASIC_IRQC2      0xA05f6938
#define IRQB0_MASK      0x0007B000

#define long_read(A)      (*((volatile unsigned long*)(A)))
#define long_write(A, V) ( (*((volatile unsigned long*)(A))) = (V) )
#define word_read(A)      (*((volatile unsigned short*)(A)))
#define word_write(A, V) ( (*((volatile unsigned short*)(A))) = (V) )
#define byte_read(A)       (unsigned int)(*((volatile unsigned char*)(A)))
#define byte_write(A, V) ( (*((volatile unsigned char*)(A))) = (V) )

int asic_wait(int event);
void asic_clear(void);
void asic_dump(FILE *f);

void fwrite_dump(FILE *f, char *buf, int length);
void fwrite_diff(FILE *f, char *expect, int exp_length, char *buf, int length);
void fwrite_diff32(FILE *f, char *expect, int exp_length, char *buf, int length);

void *align32(char *buf );
void write_asic_status(void);
void reset_asic_status(void);
void g2_fifo_wait( void );

void spu_memload( uint32_t dst, const void *src, int length );
void spu_memread( void *dst, uint32_t src, int length );
void dump_spu_memory( unsigned int from, int length );

struct spudma_struct {
    uint32_t g2_addr;
    uint32_t sh4_addr;
    uint32_t count;
    uint32_t direction;
    uint32_t mode;
    uint32_t enable;
    uint32_t status;
    uint32_t blah;
};
extern struct spudma_struct *spudma;

#define get_asic_status() (long_read(ASIC_STATUS0)&EVENT_MAPLE_MASK)

int is_start_pressed();

#define CHECK_IEQUALS( a, b ) if( a != b ) { fprintf(stderr, "Assertion failed at %s:%d: expected %08X, but was %08X\n", __FILE__, __LINE__, a, b ); return -1; }

#endif
