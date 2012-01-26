
#include <stdlib.h>
#include "asic.h"
#include "lib.h"
#include "testdata.h"

#define SPUBASE 0xA05F7800
#define SPUBASERAM 0x00800000
#define SPUWAIT  (SPUBASE+0x90)
#define SPUMAGIC (SPUBASE+0xBC)


#define SPUDMAEXT(x) (SPUBASE+(0x20*(x)))
#define SPUDMAHOST(x) (SPUBASE+(0x20*(x))+0x04)
#define SPUDMASIZE(x) (SPUBASE+(0x20*(x))+0x08)
#define SPUDMADIR(x) (SPUBASE+(0x20*(x))+0x0C)
#define SPUDMAMODE(x) (SPUBASE+(0x20*(x))+0x10)
#define SPUDMACTL1(x) (SPUBASE+(0x20*(x))+0x14)
#define SPUDMACTL2(x) (SPUBASE+(0x20*(x))+0x18)
#define SPUDMASTOP(x) (SPUBASE+(0x20*(x))+0x1C)

void dump_spu_regs()
{
    fwrite_dump( stderr, (char *)(0xA05F7800), 0x100 );
}    

int dma_to_spu( int chan, uint32_t target, char *data, uint32_t size )
{
    long_write( SPUWAIT, 0 );
    long_write( SPUMAGIC, 0x4659404f );
    long_write( SPUDMACTL1(chan), 0 );
    long_write( SPUDMACTL2(chan), 0 );
    long_write( SPUDMAHOST(chan), ((uint32_t)data)&0x1FFFFFE0 );
    long_write( SPUDMASIZE(chan), size | 0x80000000 );
    long_write( SPUDMAEXT(chan), target );
    long_write( SPUDMADIR(chan), 0 );
    long_write( SPUDMAMODE(chan), 0 );
    
    long_write( SPUDMACTL1(chan), 1 );
    long_write( SPUDMACTL2(chan), 1 );
    if( asic_wait( EVENT_G2_DMA0 + chan ) != 0 ) {
	fprintf( stderr, "Timeout waiting for DMA event\n" );
	dump_spu_regs();
	return -1;
    }
    return 0;
}

int dma_from_spu( int chan, char *data, uint32_t src, uint32_t size )
{
    long_write( SPUWAIT, 0 );
    long_write( SPUMAGIC, 0x4659404f );
    long_write( SPUDMACTL1(chan), 0 );
    long_write( SPUDMACTL2(chan), 0 );
    long_write( SPUDMAHOST(chan), ((uint32_t)data)&0x1FFFFFE0 );
    long_write( SPUDMASIZE(chan), size | 0x80000000 );
    long_write( SPUDMAEXT(chan), src );
    long_write( SPUDMADIR(chan), 1 );
    long_write( SPUDMAMODE(chan), 5 );
    
    long_write( SPUDMACTL1(chan), 1 );
    long_write( SPUDMACTL2(chan), 1 );
    if( asic_wait( EVENT_G2_DMA0 + chan ) != 0 ) {
	fprintf( stderr, "Timeout waiting for DMA event\n" );
	dump_spu_regs();
	return -1;
    }
    return 0;
}

#define SPUTARGETADDR (SPUBASERAM+0x10000)
#define SPUTARGET ((char *)(SPUTARGETADDR))

int test_spu_dma_channel( int chan )
{
    char sampledata1[256+32];
    char sampledata2[256+32];
    char resultdata[256+32];

    int i;
    char *p1 = DMA_ALIGN(sampledata1), *p2 = DMA_ALIGN(sampledata2);
    char *r = DMA_ALIGN(resultdata);

    for( i=0; i<256; i++ ) {
	p1[i] = (char)(i*i);
	p2[i] = 256 - i;
    }

    if( dma_to_spu( chan, SPUTARGETADDR, p1, 256 ) != 0 ) {
	return -1;
    }

    if( memcmp( p1, SPUTARGET, 256 ) != 0 ) {
	fprintf( stderr, "First data mismatch:\n" );
	fwrite_diff( stderr, p1, 256, SPUTARGET, 256 );
	return -1;
    }

    if( dma_to_spu( chan, SPUTARGETADDR, p2, 256 ) != 0 ) {
	return -1;
    }

    if( memcmp( p2, SPUTARGET, 256 ) != 0 ) {
	fprintf( stderr, "Second data mismatch:\n" );
	fwrite_diff( stderr, p2, 256, SPUTARGET, 256 );
	return -1;
    }

    memset( r, 0, 256 );
    if( dma_from_spu( chan, r, SPUTARGETADDR, 256 ) != 0 ) {
	return -1;
    }

    if( memcmp( p2, r, 256 ) != 0 ) {
	fprintf( stderr, "Read data mismatch:\n" );
	fwrite_diff( stderr, p2, 256, r, 256 );
	return -1;
    }
}


int test_spu_dma()
{
    int i; 
    for( i=0; i<4; i++ ) {
	if( test_spu_dma_channel(i) != 0 ) {
	    return -1;
	}
    }
}

test_func_t tests[] = { test_spu_dma, NULL };

int main()
{
    return run_tests(tests);
}
