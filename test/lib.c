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

void fwrite_diff( FILE *f,  char *a, int a_length, char *b, int b_length )
{
    int i;
    fprintf( f, "Expected %d bytes:\n", a_length );
    fwrite_dump( f, a, a_length );
    fprintf( f, "but was %d bytes =>\n", b_length );
    fwrite_dump( f, b, b_length );
}

void fwrite_diff32( FILE *f, char *a, int a_length, char *b, int b_length )
{
    int i,j, k;
    int length = a_length > b_length ? a_length : b_length;
    fprintf( f, "Expected %d bytes, was %d bytes =>\n", a_length, b_length );
    
    for( i=0; i<length; i+=16 ) {
	for( k=0; k<32 && i+k < length; k+=4 ) {
	    if( i+k >= a_length || i+k >= b_length ||
		*((volatile unsigned int *)(a+i+k)) != *((volatile unsigned int *)(b+i+k)) ) {
		break;
	    }
	}
	if( k != 32 && i+k != length ) {
	    fprintf( f, "%08X: ", i );
	    for( j=0; j<16 && i+j < a_length; j+=4 ) {
		fprintf( f, "%08X ", *((volatile unsigned int *)(a+i+j)) );
	    }
	    
	    for(; j<16; j+=4 ) {
		fprintf( f, "         " );
	    }
	    
	    fprintf( f, "| " );
	    for( j=0; j<16 && i+j < b_length; j+=4 ) {
		fprintf( f, "%08X ", *((volatile unsigned int *)(b+i+j)) );
	    }

	    fprintf( f, "\n");
	}
    }
}
