#include <assert.h>
#include <ctype.h>
#include "ide.h"
void debug_dump_buffer(char *buf, int length)
{
    int i,j;
    for( i=0; i<length; i+=16 ) {
	printf( "%08X: ", i );
        for( j=0; j<16 && i+j < length; j+=4 ) {
	    unsigned int val = *((volatile unsigned int *)(buf+i+j));
	    printf( "%02X %02X %02X %02X  ", val&0xFF, (val>>8)&0xFF, (val>>16)&0xFF, (val>>24)&0xFF );
        }
	for( j=0; j<16 && i+j < length; j++ ) {
	    printf( "%c", isprint(buf[i+j]) ? buf[i+j] : '.' );
	}
	printf("\n");
    }
}

void debug_dump_int_buffer(volatile char *buf, int length)
{
    int i,j;
    for( i=0; i<length; i+=16 ) {
	printf( "%08X:", i );
        for( j=0; j<16 && i+j < length; j+=4 ) {
	    printf( " %08X", *((volatile unsigned int *)(buf+i+j)) );
        }
	printf( "\n" );
    }
}


char buf[2048*7 + 32];

int test_ide_read_bootstrap()
{
    struct gdrom_session session;
    int length;
    char *p;

    ide_init();

    if( ide_test_ready() != 0 ) {
	printf( "ERROR - Test ready failed\n" );
	return -1;
    }

    if( ide_spinup() != 0 ) {
	printf( "ERROR - Spinup failed\n" );
	return -1;
    }

    /*
    length = ide_unknown71( buf, sizeof(buf) );
    if( length == -1 ) {
	printf( "ERROR - 0x71 failed\n" );
	ide_print_sense_error();
	return -1;
    }
    debug_dump_buffer(buf,length);
    */
    if( ide_get_session( 0, &session ) == -1 ) {
	printf( "ERROR - Get session(0) failed\n" );
	return -1;
    }
    if( ide_get_session( session.track, &session ) == -1 ) {
	printf( "ERROR - Get session(%d) failed\n", session.track );
	return -1;
    }

    p = (char *)((((unsigned int)buf) & 0xFFFFFFE0) + 0x20);
    printf( "--- DMA buffer: %08X\n",  p );
    length = ide_read_sector_dma( session.lba, 2, 0x28, p, 2048*2 );
    if( length != 2048*2 ) {
	printf( "ERROR - Got incorrect read length, expected %d but was %d\n", 2048, length );
	return -1;
    }
    debug_dump_buffer( p, 2048*2 );
    
    return 0;
}


int main( int argc, char *argv[] )
{
    ide_dump_registers();
    ide_spinup();
    ide_read_something();
//    test_ide_read_bootstrap();
}
