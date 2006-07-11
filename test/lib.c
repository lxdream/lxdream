#include "lib.h"

void fwrite_dump( FILE *f, char *buf, int length)
{
    int i,j;
    for( i=0; i<length; i+=16 ) {
	fprintf( f, "%08X: ", i );
        for( j=0; j<16 && i+j < length; j+=4 ) {
	    unsigned int val = *((volatile unsigned int *)(buf+i+j));
	    fprintf( f, "%02X %02X %02X %02X  ", val&0xFF, (val>>8)&0xFF, (val>>16)&0xFF, (val>>24)&0xFF );
        }
        for( ;j<16; j+= 4 ) {
            fprintf( f, "             " );
        }
	for( j=0; j<16 && i+j < length; j++ ) {
	    fprintf( f, "%c", isprint(buf[i+j]) ? buf[i+j] : '.' );
	}
	fprintf( f, "\n");
    }
}
