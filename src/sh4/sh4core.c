#include <math.h>
#include "dream.h"
#include "sh4core.h"
#include "sh4mmio.h"
#include "mem.h"
#include "intc.h"

struct sh4_registers sh4r;

static int running = 0;

void sh4_init(void)
{
    register_io_regions( mmio_list_sh4mmio );
}

void sh4_reset(void)
{
    sh4r.pc    = 0xA0000000;
    sh4r.new_pc= 0xA0000002;
    sh4r.vbr   = 0x00000000;
    sh4r.fpscr = 0x00040001;
    sh4r.sr    = 0x700000F0;
    sh4r.icount= 0;
    /* Everything else is undefined anyway, so don't bother setting it */
    intc_reset();
}

void sh4_set_pc( int pc )
{
    sh4r.pc = pc;
    sh4r.new_pc = pc+2;
}

void sh4_stop(void)
{
    running = 0;
}

void sh4_run(void)
{
    running = 1;
    while( running ) {
        sh4_execute_instruction();
    }
}

void sh4_runfor(uint32_t count)
{
    running = 1;
    while( running && count--) {
        sh4_execute_instruction();
    }
}

int sh4_isrunning(void)
{
    return running;
}

void sh4_runto( uint32_t target_pc, uint32_t count )
{
    running = 1;
    do {
        sh4_execute_instruction();
    } while( running && sh4r.pc != target_pc && count-- );
    if( count == 0 )
        running = 0;
}

#define UNDEF(ir) do{ ERROR( "Raising exception on undefined instruction at %08x, opcode = %04x", sh4r.pc, ir ); sh4_stop(); RAISE( EXC_ILLEGAL, EXV_ILLEGAL ); }while(0)
#define UNIMP(ir) do{ ERROR( "Halted on unimplemented instruction at %08x, opcode = %04x", sh4r.pc, ir ); sh4_stop(); return; }while(0)

#define RAISE( x, v ) do{ \
    if( sh4r.vbr == 0 ) { \
        ERROR( "%08X: VBR not initialized while raising exception %03X, halting", sh4r.pc, x ); \
        sh4_stop(); \
    } else { \
        sh4r.spc = sh4r.pc + 2; \
        sh4r.ssr = sh4_read_sr(); \
        sh4r.sgr = sh4r.r[15]; \
        MMIO_WRITE(MMU,EXPEVT,x); \
        sh4r.pc = sh4r.vbr + v; \
        sh4r.new_pc = sh4r.pc + 2; \
        sh4_load_sr( sh4r.ssr |SR_MD|SR_BL|SR_RB ); \
    } \
    return; } while(0)

#define MEM_READ_BYTE( addr ) mem_read_byte(addr)
#define MEM_READ_WORD( addr ) mem_read_word(addr)
#define MEM_READ_LONG( addr ) mem_read_long(addr)
#define MEM_WRITE_BYTE( addr, val ) mem_write_byte(addr, val)
#define MEM_WRITE_WORD( addr, val ) mem_write_word(addr, val)
#define MEM_WRITE_LONG( addr, val ) mem_write_long(addr, val)

#define MEM_FP_READ( addr, reg ) if( IS_FPU_DOUBLESIZE() ) { \
    ((uint32_t *)FR)[(reg)&0xE0] = mem_read_long(addr); \
    ((uint32_t *)FR)[(reg)|1] = mem_read_long(addr+4); \
} else ((uint32_t *)FR)[reg] = mem_read_long(addr)

#define MEM_FP_WRITE( addr, reg ) if( IS_FPU_DOUBLESIZE() ) { \
    mem_write_long( addr, ((uint32_t *)FR)[(reg)&0xE0] ); \
    mem_write_long( addr+4, ((uint32_t *)FR)[(reg)|1] ); \
} else mem_write_long( addr, ((uint32_t *)FR)[reg] )

#define FP_WIDTH (IS_FPU_DOUBLESIZE() ? 8 : 4)

#define EXC_POWER_RESET  0x000 /* vector special */
#define EXC_MANUAL_RESET 0x020
#define EXC_ILLEGAL      0x180
#define EXV_ILLEGAL      0x100
#define EXC_TRAP         0x160
#define EXV_TRAP         0x100
#define EXC_FPDISABLE    0x800
#define EXV_FPDISABLE    0x100

#define CHECK( x, c, v ) if( !x ) RAISE( c, v )
#define CHECKPRIV() CHECK( IS_SH4_PRIVMODE(), EXC_ILLEGAL, EXV_ILLEGAL )
#define CHECKFPUEN() CHECK( IS_FPU_ENABLED(), EXC_FPDISABLE, EXV_FPDISABLE )
#define CHECKDEST(p) if( (p) == 0 ) { ERROR( "%08X: Branch/jump to NULL, CPU halted", sh4r.pc ); sh4_stop(); return; }

static void sh4_switch_banks( )
{
    uint32_t tmp[8];

    memcpy( tmp, sh4r.r, sizeof(uint32_t)*8 );
    memcpy( sh4r.r, sh4r.r_bank, sizeof(uint32_t)*8 );
    memcpy( sh4r.r_bank, tmp, sizeof(uint32_t)*8 );
}

static void sh4_load_sr( uint32_t newval )
{
    if( (newval ^ sh4r.sr) & SR_RB )
        sh4_switch_banks();
    sh4r.sr = newval;
    sh4r.t = (newval&SR_T) ? 1 : 0;
    sh4r.s = (newval&SR_S) ? 1 : 0;
    sh4r.m = (newval&SR_M) ? 1 : 0;
    sh4r.q = (newval&SR_Q) ? 1 : 0;
    intc_mask_changed();
}

static uint32_t sh4_read_sr( void )
{
    /* synchronize sh4r.sr with the various bitflags */
    sh4r.sr &= SR_MQSTMASK;
    if( sh4r.t ) sh4r.sr |= SR_T;
    if( sh4r.s ) sh4r.sr |= SR_S;
    if( sh4r.m ) sh4r.sr |= SR_M;
    if( sh4r.q ) sh4r.sr |= SR_Q;
    return sh4r.sr;
}
/* function for external use */
void sh4_raise_exception( int code, int vector )
{
    RAISE(code, vector);
}

static void sh4_accept_interrupt( void )
{
    uint32_t code = intc_accept_interrupt();

    sh4r.ssr = sh4_read_sr();
    sh4r.spc = sh4r.pc;
    sh4r.sgr = sh4r.r[15];
    sh4_load_sr( sh4r.ssr|SR_BL|SR_MD|SR_RB );
    MMIO_WRITE( MMU, INTEVT, code );
    sh4r.pc = sh4r.vbr + 0x600;
    sh4r.new_pc = sh4r.pc + 2;
}

void sh4_execute_instruction( void )
{
    int pc = sh4r.pc;
    unsigned short ir = MEM_READ_WORD(pc);
    uint32_t tmp;
    uint64_t tmpl;
    
#define R0 sh4r.r[0]
#define FR0 (FR[0])
#define RN(ir) sh4r.r[(ir&0x0F00)>>8]
#define RN_BANK(ir) sh4r.r_bank[(ir&0x0070)>>4]
#define RM(ir) sh4r.r[(ir&0x00F0)>>4]
#define DISP4(ir) (ir&0x000F) /* 4-bit displacements are *NOT* sign-extended */
#define DISP8(ir) (ir&0x00FF)
#define PCDISP8(ir) SIGNEXT8(ir&0x00FF)
#define IMM8(ir) SIGNEXT8(ir&0x00FF)
#define UIMM8(ir) (ir&0x00FF) /* Unsigned immmediate */
#define DISP12(ir) SIGNEXT12(ir&0x0FFF)
#define FVN(ir) ((ir&0x0C00)
#define FVM(ir) ((ir&0x0300)>>8)
#define FRN(ir) (FR[(ir&0x0F00)>>8])
#define FRM(ir) (FR[(ir&0x00F0)>>4])
#define FRNi(ir) (((uint32_t *)FR)[(ir&0x0F00)>>8])
#define FRMi(ir) (((uint32_t *)FR)[(ir&0x00F0)>>4])
#define DRN(ir) (((double *)FR)[(ir&0x0E00)>>9])
#define DRM(ir) (((double *)FR)[(ir&0x00E0)>>5])
#define DRNi(ir) (((uint64_t *)FR)[(ir&0x0E00)>>9])
#define DRMi(ir) (((uint64_t *)FR)[(ir&0x00E0)>>5])
#define FRNn(ir) ((ir&0x0F00)>>8)
#define FRMn(ir) ((ir&0x00F0)>>4)
#define FPULf   *((float *)&sh4r.fpul)
#define FPULi    (sh4r.fpul)

    if( SH4_INT_PENDING() ) sh4_accept_interrupt();
                 
    sh4r.icount++;
    
    switch( (ir&0xF000)>>12 ) {
        case 0: /* 0000nnnnmmmmxxxx */
            switch( ir&0x000F ) {
                case 2:
                    switch( (ir&0x00F0)>>4 ) {
                        case 0: /* STC     SR, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4_read_sr();
                            break;
                        case 1: /* STC     GBR, Rn */
                            RN(ir) = sh4r.gbr;
                            break;
                        case 2: /* STC     VBR, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4r.vbr;
                            break;
                        case 3: /* STC     SSR, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4r.ssr;
                            break;
                        case 4: /* STC     SPC, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4r.spc;
                            break;
                        case 8: case 9: case 10: case 11: case 12: case 13:
                        case 14: case 15:/* STC     Rm_bank, Rn */
                            CHECKPRIV();
                            RN(ir) = RN_BANK(ir);
                            break;
                        default: UNDEF(ir);
                    }
                    break;
                case 3:
                    switch( (ir&0x00F0)>>4 ) {
                        case 0: /* BSRF    Rn */
                            CHECKDEST( pc + 4 + RN(ir) );
                            sh4r.pr = sh4r.pc + 4;
                            sh4r.pc = sh4r.new_pc;
                            sh4r.new_pc = pc + 4 + RN(ir);
                            return;
                        case 2: /* BRAF    Rn */
                            CHECKDEST( pc + 4 + RN(ir) );
                            sh4r.pc = sh4r.new_pc;
                            sh4r.new_pc = pc + 4 + RN(ir);
                            return;
                        case 8: /* PREF    [Rn] */
                        case 9: /* OCBI    [Rn] */
                        case 10:/* OCBP    [Rn] */
                        case 11:/* OCBWB   [Rn] */
                            /* anything? */
                            break;
                        case 12:/* MOVCA.L R0, [Rn] */
                            UNIMP(ir);
                        default: UNDEF(ir);
                    }
                    break;
                case 4: /* MOV.B   Rm, [R0 + Rn] */
                    MEM_WRITE_BYTE( R0 + RN(ir), RM(ir) );
                    break;
                case 5: /* MOV.W   Rm, [R0 + Rn] */
                    MEM_WRITE_WORD( R0 + RN(ir), RM(ir) );
                    break;
                case 6: /* MOV.L   Rm, [R0 + Rn] */
                    MEM_WRITE_LONG( R0 + RN(ir), RM(ir) );
                    break;
                case 7: /* MUL.L   Rm, Rn */
                    sh4r.mac = (sh4r.mac&0xFFFFFFFF00000000) |
                        (RM(ir) * RN(ir));
                    break;
                case 8: 
                    switch( (ir&0x0FF0)>>4 ) {
                        case 0: /* CLRT    */
                            sh4r.t = 0;
                            break;
                        case 1: /* SETT    */
                            sh4r.t = 1;
                            break;
                        case 2: /* CLRMAC  */
                            sh4r.mac = 0;
                            break;
                        case 3: /* LDTLB   */
                            break;
                        case 4: /* CLRS    */
                            sh4r.s = 0;
                            break;
                        case 5: /* SETS    */
                            sh4r.s = 1;
                            break;
                        default: UNDEF(ir);
                    }
                    break;
                case 9: 
                    if( (ir&0x00F0) == 0x20 ) /* MOVT    Rn */
                        RN(ir) = sh4r.t;
                    else if( ir == 0x0019 ) /* DIV0U   */
                        sh4r.m = sh4r.q = sh4r.t = 0;
                    else if( ir == 0x0009 )
                        /* NOP     */;
                    else UNDEF(ir);
                    break;
                case 10:
                    switch( (ir&0x00F0) >> 4 ) {
                        case 0: /* STS     MACH, Rn */
                            RN(ir) = sh4r.mac >> 32;
                            break;
                        case 1: /* STS     MACL, Rn */
                            RN(ir) = (uint32_t)sh4r.mac;
                            break;
                        case 2: /* STS     PR, Rn */
                            RN(ir) = sh4r.pr;
                            break;
                        case 3: /* STC     SGR, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4r.sgr;
                            break;
                        case 5:/* STS      FPUL, Rn */
                            RN(ir) = sh4r.fpul;
                            break;
                        case 6: /* STS     FPSCR, Rn */
                            RN(ir) = sh4r.fpscr;
                            break;
                        case 15:/* STC     DBR, Rn */
                            CHECKPRIV();
                            RN(ir) = sh4r.dbr;
                            break;
                        default: UNDEF(ir);
                    }
                    break;
                case 11:
                    switch( (ir&0x0FF0)>>4 ) {
                        case 0: /* RTS     */
                            CHECKDEST( sh4r.pr );
                            sh4r.pc = sh4r.new_pc;
                            sh4r.new_pc = sh4r.pr;
                            return;
                        case 1: /* SLEEP   */
                            running = 0;
                            break;
                        case 2: /* RTE     */
                            CHECKPRIV();
                            CHECKDEST( sh4r.spc );
                            sh4r.pc = sh4r.new_pc;
                            sh4r.new_pc = sh4r.spc;
                            sh4_load_sr( sh4r.ssr );
                            return;
                        default:UNDEF(ir);
                    }
                    break;
                case 12:/* MOV.B   [R0+R%d], R%d */
                    RN(ir) = MEM_READ_BYTE( R0 + RM(ir) );
                    break;
                case 13:/* MOV.W   [R0+R%d], R%d */
                    RN(ir) = MEM_READ_WORD( R0 + RM(ir) );
                    break;
                case 14:/* MOV.L   [R0+R%d], R%d */
                    RN(ir) = MEM_READ_LONG( R0 + RM(ir) );
                    break;
                case 15:/* MAC.L   [Rm++], [Rn++] */
                    tmpl = ( SIGNEXT32(MEM_READ_LONG(RM(ir))) *
                                  SIGNEXT32(MEM_READ_LONG(RN(ir))) );
                    if( sh4r.s ) {
                        /* 48-bit Saturation. Yuch */
                        tmpl += SIGNEXT48(sh4r.mac);
                        if( tmpl < 0xFFFF800000000000 )
                            tmpl = 0xFFFF800000000000;
                        else if( tmpl > 0x00007FFFFFFFFFFF )
                            tmpl = 0x00007FFFFFFFFFFF;
                        sh4r.mac = (sh4r.mac&0xFFFF000000000000) |
                            (tmpl&0x0000FFFFFFFFFFFF);
                    } else sh4r.mac = tmpl;
                    
                    RM(ir) += 4;
                    RN(ir) += 4;
                    
                    break;
                default: UNDEF(ir);
            }
            break;
        case 1: /* 0001nnnnmmmmdddd */
            /* MOV.L   Rm, [Rn + disp4*4] */
            MEM_WRITE_LONG( RN(ir) + (DISP4(ir)<<2), RM(ir) );
            break;
        case 2: /* 0010nnnnmmmmxxxx */
            switch( ir&0x000F ) {
                case 0: /* MOV.B   Rm, [Rn] */
                    MEM_WRITE_BYTE( RN(ir), RM(ir) );
                    break;
                case 1: /* MOV.W   Rm, [Rn] */
                    MEM_WRITE_WORD( RN(ir), RM(ir) );
                    break;
                case 2: /* MOV.L   Rm, [Rn] */
                    MEM_WRITE_LONG( RN(ir), RM(ir) );
                    break;
                case 3: UNDEF(ir);
                    break;
                case 4: /* MOV.B   Rm, [--Rn] */
                    RN(ir) --;
                    MEM_WRITE_BYTE( RN(ir), RM(ir) );
                    break;
                case 5: /* MOV.W   Rm, [--Rn] */
                    RN(ir) -= 2;
                    MEM_WRITE_WORD( RN(ir), RM(ir) );
                    break;
                case 6: /* MOV.L   Rm, [--Rn] */
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), RM(ir) );
                    break;
                case 7: /* DIV0S   Rm, Rn */
                    sh4r.q = RN(ir)>>31;
                    sh4r.m = RM(ir)>>31;
                    sh4r.t = sh4r.q ^ sh4r.m;
                    break;
                case 8: /* TST     Rm, Rn */
                    sh4r.t = (RN(ir)&RM(ir) ? 0 : 1);
                    break;
                case 9: /* AND     Rm, Rn */
                    RN(ir) &= RM(ir);
                    break;
                case 10:/* XOR     Rm, Rn */
                    RN(ir) ^= RM(ir);
                    break;
                case 11:/* OR      Rm, Rn */
                    RN(ir) |= RM(ir);
                    break;
                case 12:/* CMP/STR Rm, Rn */
                    /* set T = 1 if any byte in RM & RN is the same */
                    tmp = RM(ir) ^ RN(ir);
                    sh4r.t = ((tmp&0x000000FF)==0 || (tmp&0x0000FF00)==0 ||
                              (tmp&0x00FF0000)==0 || (tmp&0xFF000000)==0)?1:0;
                    break;
                case 13:/* XTRCT   Rm, Rn */
                    RN(ir) = (RN(ir)>>16) | (RM(ir)<<16);
                    break;
                case 14:/* MULU.W  Rm, Rn */
                    sh4r.mac = (sh4r.mac&0xFFFFFFFF00000000) |
                        (uint32_t)((RM(ir)&0xFFFF) * (RN(ir)&0xFFFF));
                    break;
                case 15:/* MULS.W  Rm, Rn */
                    sh4r.mac = (sh4r.mac&0xFFFFFFFF00000000) |
                        (uint32_t)(SIGNEXT32(RM(ir)&0xFFFF) * SIGNEXT32(RN(ir)&0xFFFF));
                    break;
            }
            break;
        case 3: /* 0011nnnnmmmmxxxx */
            switch( ir&0x000F ) {
                case 0: /* CMP/EQ  Rm, Rn */
                    sh4r.t = ( RM(ir) == RN(ir) ? 1 : 0 );
                    break;
                case 2: /* CMP/HS  Rm, Rn */
                    sh4r.t = ( RN(ir) >= RM(ir) ? 1 : 0 );
                    break;
                case 3: /* CMP/GE  Rm, Rn */
                    sh4r.t = ( ((int32_t)RN(ir)) >= ((int32_t)RM(ir)) ? 1 : 0 );
                    break;
                case 4: { /* DIV1    Rm, Rn */
                    /* This is just from the sh4p manual with some
                     * simplifications (someone want to check it's correct? :)
                     * Why they couldn't just provide a real DIV instruction...
                     * Please oh please let the translator batch these things
                     * up into a single DIV... */
                    uint32_t tmp0, tmp1, tmp2, dir;

                    dir = sh4r.q ^ sh4r.m;
                    sh4r.q = (RN(ir) >> 31);
                    tmp2 = RM(ir);
                    RN(ir) = (RN(ir) << 1) | sh4r.t;
                    tmp0 = RN(ir);
                    if( dir ) {
                        RN(ir) += tmp2;
                        tmp1 = (RN(ir)<tmp0 ? 1 : 0 );
                    } else {
                        RN(ir) -= tmp2;
                        tmp1 = (RN(ir)>tmp0 ? 1 : 0 );
                    }
                    sh4r.q ^= sh4r.m ^ tmp1;
                    sh4r.t = ( sh4r.q == sh4r.m ? 1 : 0 );
                    break; }
                case 5: /* DMULU.L Rm, Rn */
                    sh4r.mac = ((uint64_t)RM(ir)) * ((uint64_t)RN(ir));
                    break;
                case 6: /* CMP/HI  Rm, Rn */
                    sh4r.t = ( RN(ir) > RM(ir) ? 1 : 0 );
                    break;
                case 7: /* CMP/GT  Rm, Rn */
                    sh4r.t = ( ((int32_t)RN(ir)) > ((int32_t)RM(ir)) ? 1 : 0 );
                    break;
                case 8: /* SUB     Rm, Rn */
                    RN(ir) -= RM(ir);
                    break;
                case 10:/* SUBC    Rm, Rn */
                    tmp = RN(ir);
                    RN(ir) = RN(ir) - RM(ir) - sh4r.t;
                    sh4r.t = (RN(ir) > tmp || (RN(ir) == tmp && sh4r.t == 1));
                    break;
                case 11:/* SUBV    Rm, Rn */
                    UNIMP(ir);
                    break;
                case 12:/* ADD     Rm, Rn */
                    RN(ir) += RM(ir);
                    break;
                case 13:/* DMULS.L Rm, Rn */
                    sh4r.mac = SIGNEXT32(RM(ir)) * SIGNEXT32(RN(ir));
                    break;
                case 14:/* ADDC    Rm, Rn */
                    tmp = RN(ir);
                    RN(ir) += RM(ir) + sh4r.t;
                    sh4r.t = ( RN(ir) < tmp || (RN(ir) == tmp && sh4r.t != 0) ? 1 : 0 );
                    break;
                case 15:/* ADDV    Rm, Rn */
                    UNIMP(ir);
                    break;
                default: UNDEF(ir);
            }
            break;
        case 4: /* 0100nnnnxxxxxxxx */
            switch( ir&0x00FF ) {
                case 0x00: /* SHLL    Rn */
                    sh4r.t = RN(ir) >> 31;
                    RN(ir) <<= 1;
                    break;
                case 0x01: /* SHLR    Rn */
                    sh4r.t = RN(ir) & 0x00000001;
                    RN(ir) >>= 1;
                    break;
                case 0x02: /* STS.L   MACH, [--Rn] */
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), (sh4r.mac>>32) );
                    break;
                case 0x03: /* STC.L   SR, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), sh4_read_sr() );
                    break;
                case 0x04: /* ROTL    Rn */
                    sh4r.t = RN(ir) >> 31;
                    RN(ir) <<= 1;
                    RN(ir) |= sh4r.t;
                    break;
                case 0x05: /* ROTR    Rn */
                    sh4r.t = RN(ir) & 0x00000001;
                    RN(ir) >>= 1;
                    RN(ir) |= (sh4r.t << 31);
                    break;
                case 0x06: /* LDS.L   [Rn++], MACH */
                    sh4r.mac = (sh4r.mac & 0x00000000FFFFFFFF) |
                        (((uint64_t)MEM_READ_LONG(RN(ir)))<<32);
                    RN(ir) += 4;
                    break;
                case 0x07: /* LDC.L   [Rn++], SR */
                    CHECKPRIV();
                    sh4_load_sr( MEM_READ_LONG(RN(ir)) );
                    RN(ir) +=4;
                    break;
                case 0x08: /* SHLL2   Rn */
                    RN(ir) <<= 2;
                    break;
                case 0x09: /* SHLR2   Rn */
                    RN(ir) >>= 2;
                    break;
                case 0x0A: /* LDS     Rn, MACH */
                    sh4r.mac = (sh4r.mac & 0x00000000FFFFFFFF) |
                        (((uint64_t)RN(ir))<<32);
                    break;
                case 0x0B: /* JSR     [Rn] */
                    CHECKDEST( RN(ir) );
                    sh4r.pc = sh4r.new_pc;
                    sh4r.new_pc = RN(ir);
                    sh4r.pr = pc + 4;
                    return;
                case 0x0E: /* LDC     Rn, SR */
                    CHECKPRIV();
                    sh4_load_sr( RN(ir) );
                    break;
                case 0x10: /* DT      Rn */
                    RN(ir) --;
                    sh4r.t = ( RN(ir) == 0 ? 1 : 0 );
                    break;
                case 0x11: /* CMP/PZ  Rn */
                    sh4r.t = ( ((int32_t)RN(ir)) >= 0 ? 1 : 0 );
                    break;
                case 0x12: /* STS.L   MACL, [--Rn] */
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), (uint32_t)sh4r.mac );
                    break;
                case 0x13: /* STC.L   GBR, [--Rn] */
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), sh4r.gbr );
                    break;
                case 0x15: /* CMP/PL  Rn */
                    sh4r.t = ( ((int32_t)RN(ir)) > 0 ? 1 : 0 );
                    break;
                case 0x16: /* LDS.L   [Rn++], MACL */
                    sh4r.mac = (sh4r.mac & 0xFFFFFFFF00000000) |
                        (uint64_t)((uint32_t)MEM_READ_LONG(RN(ir)));
                    RN(ir) += 4;
                    break;
                case 0x17: /* LDC.L   [Rn++], GBR */
                    sh4r.gbr = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x18: /* SHLL8   Rn */
                    RN(ir) <<= 8;
                    break;
                case 0x19: /* SHLR8   Rn */
                    RN(ir) >>= 8;
                    break;
                case 0x1A: /* LDS     Rn, MACL */
                    sh4r.mac = (sh4r.mac & 0xFFFFFFFF00000000) |
                        (uint64_t)((uint32_t)(RN(ir)));
                    break;
                case 0x1B: /* TAS.B   [Rn] */
                    tmp = MEM_READ_BYTE( RN(ir) );
                    sh4r.t = ( tmp == 0 ? 1 : 0 );
                    MEM_WRITE_BYTE( RN(ir), tmp | 0x80 );
                    break;
                case 0x1E: /* LDC     Rn, GBR */
                    sh4r.gbr = RN(ir);
                    break;
                case 0x20: /* SHAL    Rn */
                    sh4r.t = RN(ir) >> 31;
                    RN(ir) <<= 1;
                    break;
                case 0x21: /* SHAR    Rn */
                    sh4r.t = RN(ir) & 0x00000001;
                    RN(ir) = ((int32_t)RN(ir)) >> 1;
                    break;
                case 0x22: /* STS.L   PR, [--Rn] */
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), sh4r.pr );
                    break;
                case 0x23: /* STC.L   VBR, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), sh4r.pr );
                    break;
                case 0x24: /* ROTCL   Rn */
                    tmp = RN(ir) >> 31;
                    RN(ir) <<= 1;
                    RN(ir) |= sh4r.t;
                    sh4r.t = tmp;
                    break;
                case 0x25: /* ROTCR   Rn */
                    tmp = RN(ir) & 0x00000001;
                    RN(ir) >>= 1;
                    RN(ir) |= (sh4r.t << 31 );
                    sh4r.t = tmp;
                    break;
                case 0x26: /* LDS.L   [Rn++], PR */
                    sh4r.pr = MEM_READ_LONG( RN(ir) );
                    RN(ir) += 4;
                    break;
                case 0x27: /* LDC.L   [Rn++], VBR */
                    CHECKPRIV();
                    sh4r.vbr = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x28: /* SHLL16  Rn */
                    RN(ir) <<= 16;
                    break;
                case 0x29: /* SHLR16  Rn */
                    RN(ir) >>= 16;
                    break;
                case 0x2A: /* LDS     Rn, PR */
                    sh4r.pr = RN(ir);
                    break;
                case 0x2B: /* JMP     [Rn] */
                    CHECKDEST( RN(ir) );
                    sh4r.pc = sh4r.new_pc;
                    sh4r.new_pc = RN(ir);
                    return;
                case 0x2E: /* LDC     Rn, VBR */
                    CHECKPRIV();
                    sh4r.vbr = RN(ir);
                    break;
                case 0x32: /* STC.L   SGR, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), sh4r.sgr );
                    break;
                case 0x33: /* STC.L   SSR, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), sh4r.ssr );
                    break;
                case 0x37: /* LDC.L   [Rn++], SSR */
                    CHECKPRIV();
                    sh4r.ssr = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x3E: /* LDC     Rn, SSR */
                    CHECKPRIV();
                    sh4r.ssr = RN(ir);
                    break;
                case 0x43: /* STC.L   SPC, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), sh4r.spc );
                    break;
                case 0x47: /* LDC.L   [Rn++], SPC */
                    CHECKPRIV();
                    sh4r.spc = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x4E: /* LDC     Rn, SPC */
                    CHECKPRIV();
                    sh4r.spc = RN(ir);
                    break;
                case 0x52: /* STS.L   FPUL, [--Rn] */
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), sh4r.fpul );
                    break;
                case 0x56: /* LDS.L   [Rn++], FPUL */
                    sh4r.fpul = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x5A: /* LDS     Rn, FPUL */
                    sh4r.fpul = RN(ir);
                    break;
                case 0x62: /* STS.L   FPSCR, [--Rn] */
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), sh4r.fpscr );
                    break;
                case 0x66: /* LDS.L   [Rn++], FPSCR */
                    sh4r.fpscr = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0x6A: /* LDS     Rn, FPSCR */
                    sh4r.fpscr = RN(ir);
                    break;
                case 0xF2: /* STC.L   DBR, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), sh4r.dbr );
                    break;
                case 0xF6: /* LDC.L   [Rn++], DBR */
                    CHECKPRIV();
                    sh4r.dbr = MEM_READ_LONG(RN(ir));
                    RN(ir) +=4;
                    break;
                case 0xFA: /* LDC     Rn, DBR */
                    CHECKPRIV();
                    sh4r.dbr = RN(ir);
                    break;
                case 0x83: case 0x93: case 0xA3: case 0xB3: case 0xC3:
                case 0xD3: case 0xE3: case 0xF3: /* STC.L   Rn_BANK, [--Rn] */
                    CHECKPRIV();
                    RN(ir) -= 4;
                    MEM_WRITE_LONG( RN(ir), RN_BANK(ir) );
                    break;
                case 0x87: case 0x97: case 0xA7: case 0xB7: case 0xC7:
                case 0xD7: case 0xE7: case 0xF7: /* LDC.L   [Rn++], Rn_BANK */
                    CHECKPRIV();
                    RN_BANK(ir) = MEM_READ_LONG( RN(ir) );
                    RN(ir) += 4;
                    break;
                case 0x8E: case 0x9E: case 0xAE: case 0xBE: case 0xCE:
                case 0xDE: case 0xEE: case 0xFE: /* LDC     Rm, Rn_BANK */
                    CHECKPRIV();
                    RN_BANK(ir) = RM(ir);
                    break;
                default:
                    if( (ir&0x000F) == 0x0F ) {
                        /* MAC.W   [Rm++], [Rn++] */
                        tmp = SIGNEXT16(MEM_READ_WORD(RM(ir))) *
                            SIGNEXT16(MEM_READ_WORD(RN(ir)));
                        if( sh4r.s ) {
                            /* FIXME */
                            UNIMP(ir);
                        } else sh4r.mac += SIGNEXT32(tmp);
                        RM(ir) += 2;
                        RN(ir) += 2;
                    } else if( (ir&0x000F) == 0x0C ) {
                        /* SHAD    Rm, Rn */
                        tmp = RM(ir);
                        if( (tmp & 0x80000000) == 0 ) RN(ir) <<= (tmp&0x1f);
                        else if( (tmp & 0x1F) == 0 )  ((int32_t)RN(ir)) >>=31;
                        else ((int32_t)RN(ir)) >>= (((~RM(ir)) & 0x1F)+1);
                    } else if( (ir&0x000F) == 0x0D ) {
                        /* SHLD    Rm, Rn */
                        tmp = RM(ir);
                        if( (tmp & 0x80000000) == 0 ) RN(ir) <<= (tmp&0x1f);
                        else if( (tmp & 0x1F) == 0 ) RN(ir) = 0;
                        else RN(ir) >>= (((~tmp) & 0x1F)+1);
                    } else UNDEF(ir);
            }
            break;
        case 5: /* 0101nnnnmmmmdddd */
            /* MOV.L   [Rm + disp4*4], Rn */
            RN(ir) = MEM_READ_LONG( RM(ir) + (DISP4(ir)<<2) );
            break;
        case 6: /* 0110xxxxxxxxxxxx */
            switch( ir&0x000f ) {
                case 0: /* MOV.B   [Rm], Rn */
                    RN(ir) = MEM_READ_BYTE( RM(ir) );
                    break;
                case 1: /* MOV.W   [Rm], Rn */
                    RN(ir) = MEM_READ_WORD( RM(ir) );
                    break;
                case 2: /* MOV.L   [Rm], Rn */
                    RN(ir) = MEM_READ_LONG( RM(ir) );
                    break;
                case 3: /* MOV     Rm, Rn */
                    RN(ir) = RM(ir);
                    break;
                case 4: /* MOV.B   [Rm++], Rn */
                    RN(ir) = MEM_READ_BYTE( RM(ir) );
                    RM(ir) ++;
                    break;
                case 5: /* MOV.W   [Rm++], Rn */
                    RN(ir) = MEM_READ_WORD( RM(ir) );
                    RM(ir) += 2;
                    break;
                case 6: /* MOV.L   [Rm++], Rn */
                    RN(ir) = MEM_READ_LONG( RM(ir) );
                    RM(ir) += 4;
                    break;
                case 7: /* NOT     Rm, Rn */
                    RN(ir) = ~RM(ir);
                    break;
                case 8: /* SWAP.B  Rm, Rn */
                    RN(ir) = (RM(ir)&0xFFFF0000) | ((RM(ir)&0x0000FF00)>>8) |
                        ((RM(ir)&0x000000FF)<<8);
                    break;
                case 9: /* SWAP.W  Rm, Rn */
                    RN(ir) = (RM(ir)>>16) | (RM(ir)<<16);
                    break;
                case 10:/* NEGC    Rm, Rn */
                    tmp = 0 - RM(ir);
                    RN(ir) = tmp - sh4r.t;
                    sh4r.t = ( 0<tmp || tmp<RN(ir) ? 1 : 0 );
                    break;
                case 11:/* NEG     Rm, Rn */
                    RN(ir) = 0 - RM(ir);
                    break;
                case 12:/* EXTU.B  Rm, Rn */
                    RN(ir) = RM(ir)&0x000000FF;
                    break;
                case 13:/* EXTU.W  Rm, Rn */
                    RN(ir) = RM(ir)&0x0000FFFF;
                    break;
                case 14:/* EXTS.B  Rm, Rn */
                    RN(ir) = SIGNEXT8( RM(ir)&0x000000FF );
                    break;
                case 15:/* EXTS.W  Rm, Rn */
                    RN(ir) = SIGNEXT16( RM(ir)&0x0000FFFF );
                    break;
            }
            break;
        case 7: /* 0111nnnniiiiiiii */
            /* ADD    imm8, Rn */
            RN(ir) += IMM8(ir);
            break;
        case 8: /* 1000xxxxxxxxxxxx */
            switch( (ir&0x0F00) >> 8 ) {
                case 0: /* MOV.B   R0, [Rm + disp4] */
                    MEM_WRITE_BYTE( RM(ir) + DISP4(ir), R0 );
                    break;
                case 1: /* MOV.W   R0, [Rm + disp4*2] */
                    MEM_WRITE_WORD( RM(ir) + (DISP4(ir)<<1), R0 );
                    break;
                case 4: /* MOV.B   [Rm + disp4], R0 */
                    R0 = MEM_READ_BYTE( RM(ir) + DISP4(ir) );
                    break;
                case 5: /* MOV.W   [Rm + disp4*2], R0 */
                    R0 = MEM_READ_WORD( RM(ir) + (DISP4(ir)<<1) );
                    break;
                case 8: /* CMP/EQ  imm, R0 */
                    sh4r.t = ( R0 == IMM8(ir) ? 1 : 0 );
                    break;
                case 9: /* BT      disp8 */
                    if( sh4r.t ) {
                        CHECKDEST( sh4r.pc + (PCDISP8(ir)<<1) + 4 )
                        sh4r.pc += (PCDISP8(ir)<<1) + 4;
                        sh4r.new_pc = sh4r.pc + 2;
                        return;
                    }
                    break;
                case 11:/* BF      disp8 */
                    if( !sh4r.t ) {
                        CHECKDEST( sh4r.pc + (PCDISP8(ir)<<1) + 4 )
                        sh4r.pc += (PCDISP8(ir)<<1) + 4;
                        sh4r.new_pc = sh4r.pc + 2;
                        return;
                    }
                    break;
                case 13:/* BT/S    disp8 */
                    if( sh4r.t ) {
                        CHECKDEST( sh4r.pc + (PCDISP8(ir)<<1) + 4 )
                        sh4r.pc = sh4r.new_pc;
                        sh4r.new_pc = pc + (PCDISP8(ir)<<1) + 4;
                        return;
                    }
                    break;
                case 15:/* BF/S    disp8 */
                    if( !sh4r.t ) {
                        CHECKDEST( sh4r.pc + (PCDISP8(ir)<<1) + 4 )
                        sh4r.pc = sh4r.new_pc;
                        sh4r.new_pc = pc + (PCDISP8(ir)<<1) + 4;
                        return;
                    }
                    break;
                default: UNDEF(ir);
            }
            break;
        case 9: /* 1001xxxxxxxxxxxx */
            /* MOV.W   [disp8*2 + pc + 4], Rn */
            RN(ir) = MEM_READ_WORD( pc + 4 + (DISP8(ir)<<1) );
            break;
        case 10:/* 1010dddddddddddd */
            /* BRA     disp12 */
            CHECKDEST( sh4r.pc + (DISP12(ir)<<1) + 4 );
            sh4r.pc = sh4r.new_pc;
            sh4r.new_pc = pc + 4 + (DISP12(ir)<<1);
            return;
        case 11:/* 1011dddddddddddd */
            /* BSR     disp12 */
            CHECKDEST( sh4r.pc + (DISP12(ir)<<1) + 4 )
            sh4r.pr = pc + 4;
            sh4r.pc = sh4r.new_pc;
            sh4r.new_pc = pc + 4 + (DISP12(ir)<<1);
            return;
        case 12:/* 1100xxxxdddddddd */
        switch( (ir&0x0F00)>>8 ) {
                case 0: /* MOV.B  R0, [GBR + disp8] */
                    MEM_WRITE_BYTE( sh4r.gbr + DISP8(ir), R0 );
                    break;
                case 1: /* MOV.W  R0, [GBR + disp8*2] */
                    MEM_WRITE_WORD( sh4r.gbr + (DISP8(ir)<<1), R0 );
                    break;
                case  2: /*MOV.L   R0, [GBR + disp8*4] */
                    MEM_WRITE_LONG( sh4r.gbr + (DISP8(ir)<<2), R0 );
                    break;
                case 3: /* TRAPA   imm8 */
                    MMIO_WRITE( MMU, TRA, UIMM8(ir) );
                    sh4r.pc = sh4r.new_pc;  /* RAISE ends the instruction */
                    sh4r.new_pc += 2;
                    RAISE( EXC_TRAP, EXV_TRAP );
                    break;
                case 4: /* MOV.B   [GBR + disp8], R0 */
                    R0 = MEM_READ_BYTE( sh4r.gbr + DISP8(ir) );
                    break;
                case 5: /* MOV.W   [GBR + disp8*2], R0 */
                    R0 = MEM_READ_WORD( sh4r.gbr + (DISP8(ir)<<1) );
                    break;
                case 6: /* MOV.L   [GBR + disp8*4], R0 */
                    R0 = MEM_READ_LONG( sh4r.gbr + (DISP8(ir)<<2) );
                    break;
                case 7: /* MOVA    disp8 + pc&~3 + 4, R0 */
                    R0 = (pc&0xFFFFFFFC) + (DISP8(ir)<<2) + 4;
                    break;
                case 8: /* TST     imm8, R0 */
                    sh4r.t = (R0 & UIMM8(ir) ? 0 : 1);
                    break;
                case 9: /* AND     imm8, R0 */
                    R0 &= UIMM8(ir);
                    break;
                case 10:/* XOR     imm8, R0 */
                    R0 ^= UIMM8(ir);
                    break;
                case 11:/* OR      imm8, R0 */
                    R0 |= UIMM8(ir);
                    break;
                case 12:/* TST.B   imm8, [R0+GBR] */
                    sh4r.t = ( MEM_READ_BYTE(R0 + sh4r.gbr) & UIMM8(ir) ? 0 : 1 );
                    break;
                case 13:/* AND.B   imm8, [R0+GBR] */
                    MEM_WRITE_BYTE( R0 + sh4r.gbr,
                                    UIMM8(ir) & MEM_READ_BYTE(R0 + sh4r.gbr) );
                    break;
                case 14:/* XOR.B   imm8, [R0+GBR] */
                    MEM_WRITE_BYTE( R0 + sh4r.gbr,
                                    UIMM8(ir) ^ MEM_READ_BYTE(R0 + sh4r.gbr) );
                    break;
                case 15:/* OR.B    imm8, [R0+GBR] */
                    MEM_WRITE_BYTE( R0 + sh4r.gbr,
                                    UIMM8(ir) | MEM_READ_BYTE(R0 + sh4r.gbr) );
                    break;
            }
            break;
        case 13:/* 1101nnnndddddddd */
            /* MOV.L   [disp8*4 + pc&~3 + 4], Rn */
            RN(ir) = MEM_READ_LONG( (pc&0xFFFFFFFC) + (DISP8(ir)<<2) + 4 );
            break;
        case 14:/* 1110nnnniiiiiiii */
            /* MOV     imm8, Rn */
            RN(ir) = IMM8(ir);
            break;
        case 15:/* 1111xxxxxxxxxxxx */
            CHECKFPUEN();
            switch( ir&0x000F ) {
                case 0: /* FADD    FRm, FRn */
                    FRN(ir) += FRM(ir);
                    break;
                case 1: /* FSUB    FRm, FRn */
                    FRN(ir) -= FRM(ir);
                    break;
                case 2: /* FMUL    FRm, FRn */
                    FRN(ir) = FRN(ir) * FRM(ir);
                    break;
                case 3: /* FDIV    FRm, FRn */
                    FRN(ir) = FRN(ir) / FRM(ir);
                    break;
                case 4: /* FCMP/EQ FRm, FRn */
                    sh4r.t = ( FRN(ir) == FRM(ir) ? 1 : 0 );
                    break;
                case 5: /* FCMP/GT FRm, FRn */
                    sh4r.t = ( FRN(ir) > FRM(ir) ? 1 : 0 );
                    break;
                case 6: /* FMOV.S  [Rm+R0], FRn */
                    MEM_FP_READ( RM(ir) + R0, FRNn(ir) );
                    break;
                case 7: /* FMOV.S  FRm, [Rn+R0] */
                    MEM_FP_WRITE( RN(ir) + R0, FRMn(ir) );
                    break;
                case 8: /* FMOV.S  [Rm], FRn */
                    MEM_FP_READ( RM(ir), FRNn(ir) );
                    break;
                case 9: /* FMOV.S  [Rm++], FRn */
                    MEM_FP_READ( RM(ir), FRNn(ir) );
                    RM(ir) += FP_WIDTH;
                    break;
                case 10:/* FMOV.S  FRm, [Rn] */
                    MEM_FP_WRITE( RN(ir), FRMn(ir) );
                    break;
                case 11:/* FMOV.S  FRm, [--Rn] */
                    RN(ir) -= FP_WIDTH;
                    MEM_FP_WRITE( RN(ir), FRMn(ir) );
                    break;
                case 12:/* FMOV    FRm, FRn */
                    if( IS_FPU_DOUBLESIZE() ) {
                        DRN(ir) = DRM(ir);
                    } else {
                        FRN(ir) = FRM(ir);
                    }
                    break;
                case 13:
                    switch( (ir&0x00F0) >> 4 ) {
                        case 0: /* FSTS    FPUL, FRn */
                            FRN(ir) = FPULf;
                            break;
                        case 1: /* FLDS    FRn, FPUL */
                            FPULf = FRN(ir);
                            break;
                        case 2: /* FLOAT   FPUL, FRn */
                            FRN(ir) = (float)FPULi;
                            break;
                        case 3: /* FTRC    FRn, FPUL */
                            FPULi = (uint32_t)FRN(ir);
                            /* FIXME: is this sufficient? */
                            break;
                        case 4: /* FNEG    FRn */
                            FRN(ir) = -FRN(ir);
                            break;
                        case 5: /* FABS    FRn */
                            FRN(ir) = fabsf(FRN(ir));
                            break;
                        case 6: /* FSQRT   FRn */
                            FRN(ir) = sqrtf(FRN(ir));
                            break;
                        case 8: /* FLDI0   FRn */
                            FRN(ir) = 0.0;
                            break;
                        case 9: /* FLDI1   FRn */
                            FRN(ir) = 1.0;
                            break;
                        case 10: /* FCNVSD FPUL, DRn */
                            if( IS_FPU_DOUBLEPREC() )
                                DRN(ir) = (double)FPULf;
                            else UNDEF(ir);
                            break;
                        case 11: /* FCNVDS DRn, FPUL */
                            if( IS_FPU_DOUBLEPREC() ) 
                                FPULf = (float)DRN(ir);
                            else UNDEF(ir);
                            break;
                        case 14:/* FIPR    FVn, FVn */
                            UNIMP(ir);
                            break;
                        case 15:
                            if( FVM(ir) == 1 )
                            /* FTRV    XMTRX,FVn */
                                UNIMP(ir);
                            else if( ir == 0xFBFD )
                            /* FRCHG   */
                                sh4r.fpscr ^= FPSCR_FR;
                            else if( ir == 0xF3FD )
                                sh4r.fpscr ^= FPSCR_SZ;
                            /* FSCHG   */
                            break;
                        default: UNDEF(ir);
                    }
                    break;
                case 14:/* FMAC    FR0, FRm, FRn */
                    FRN(ir) += FRM(ir)*FR0;
                    break;
                default: UNDEF(ir);
            }
            break;
    }
    sh4r.pc = sh4r.new_pc;
    sh4r.new_pc += 2;
}
