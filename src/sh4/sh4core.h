/*
 * Header for the basic sh4 emulator core
 */
#ifndef sh4core_H
#define sh4core_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

struct sh4_registers {
    uint32_t r[16];
    uint32_t r_bank[8]; /* hidden banked registers */
    uint32_t sr, gbr, ssr, spc, sgr, dbr, vbr;
    uint32_t pr, pc, fpul, fpscr;
    uint64_t mac;
    uint32_t m, q, s, t; /* really boolean - 0 or 1 */
    float fr[2][16];

    int32_t store_queue[16]; /* technically 2 banks of 32 bytes */
    
    uint32_t new_pc; /* Not a real register, but used to handle delay slots */
    uint32_t icount; /* Also not a real register, instruction counter */
    uint32_t int_pending; /* flag set by the INTC = pending priority level */
    int in_delay_slot; /* flag to indicate the current instruction is in
                             * a delay slot (certain rules apply) */
};

extern struct sh4_registers sh4r;

/* Public functions */

void sh4_init( void );
void sh4_reset( void );
void sh4_run( void );
void sh4_runto( uint32_t pc, uint32_t count );
void sh4_runfor( uint32_t count );
int sh4_isrunning( void );
void sh4_stop( void );
void sh4_set_pc( int );
void sh4_execute_instruction( void );
void sh4_raise_exception( int, int );

void run_timers( int );

#define SIGNEXT4(n) ((((int32_t)(n))<<28)>>28)
#define SIGNEXT8(n) ((int32_t)((int8_t)(n)))
#define SIGNEXT12(n) ((((int32_t)(n))<<20)>>20)
#define SIGNEXT16(n) ((int32_t)((int16_t)(n)))
#define SIGNEXT32(n) ((int64_t)((int32_t)(n)))
#define SIGNEXT48(n) ((((int64_t)(n))<<16)>>16)

/* Status Register (SR) bits */
#define SR_MD    0x40000000 /* Processor mode ( User=0, Privileged=1 ) */ 
#define SR_RB    0x20000000 /* Register bank (priviledged mode only) */
#define SR_BL    0x10000000 /* Exception/interupt block (1 = masked) */
#define SR_FD    0x00008000 /* FPU disable */
#define SR_M     0x00000200
#define SR_Q     0x00000100
#define SR_IMASK 0x000000F0 /* Interrupt mask level */
#define SR_S     0x00000002 /* Saturation operation for MAC instructions */
#define SR_T     0x00000001 /* True/false or carry/borrow */
#define SR_MASK  0x700083F3
#define SR_MQSTMASK 0xFFFFFCFC /* Mask to clear the flags we're keeping separately */

#define IS_SH4_PRIVMODE() (sh4r.sr&SR_MD)
#define SH4_INTMASK() ((sh4r.sr&SR_IMASK)>>4)
#define SH4_INT_PENDING() (sh4r.int_pending && !sh4r.in_delay_slot)

#define FPSCR_FR     0x00200000 /* FPU register bank */
#define FPSCR_SZ     0x00100000 /* FPU transfer size (0=32 bits, 1=64 bits) */
#define FPSCR_PR     0x00080000 /* Precision (0=32 bites, 1=64 bits) */
#define FPSCR_DN     0x00040000 /* Denormalization mode (1 = treat as 0) */
#define FPSCR_CAUSE  0x0003F000
#define FPSCR_ENABLE 0x00000F80
#define FPSCR_FLAG   0x0000007C
#define FPSCR_RM     0x00000003 /* Rounding mode (0=nearest, 1=to zero) */

#define IS_FPU_DOUBLEPREC() (sh4r.fpscr&FPSCR_PR)
#define IS_FPU_DOUBLESIZE() (sh4r.fpscr&FPSCR_SZ)
#define IS_FPU_ENABLED() ((sh4r.sr&SR_FD)==0)

#define FR sh4r.fr[(sh4r.fpscr&FPSCR_FR)>>21]
#define XF sh4r.fr[((~sh4r.fpscr)&FPSCR_FR)>>21]

/* Exceptions (for use with sh4_raise_exception) */

#define EX_ILLEGAL_INSTRUCTION 0x180, 0x100
#define EX_SLOT_ILLEGAL        0x1A0, 0x100
#define EX_TLB_MISS_READ       0x040, 0x400
#define EX_TLB_MISS_WRITE      0x060, 0x400
#define EX_INIT_PAGE_WRITE     0x080, 0x100
#define EX_TLB_PROT_READ       0x0A0, 0x100
#define EX_TLB_PROT_WRITE      0x0C0, 0x100
#define EX_DATA_ADDR_READ      0x0E0, 0x100
#define EX_DATA_ADDR_WRITE     0x100, 0x100
#define EX_FPU_EXCEPTION       0x120, 0x100
#define EX_TRAPA               0x160, 0x100
#define EX_BREAKPOINT          0x1E0, 0x100
#define EX_FPU_DISABLED        0x800, 0x100
#define EX_SLOT_FPU_DISABLED   0x820, 0x100

#define SH4_WRITE_STORE_QUEUE(addr,val) sh4r.store_queue[(addr>>2)&0xF] = val;

#ifdef __cplusplus
}
#endif
#endif
