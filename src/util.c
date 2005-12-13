#include "dream.h"
#include "modules.h"

void fwrite_string( char *s, FILE *f )
{
    uint32_t len = 0;
    if( s == NULL ) {
	fwrite( &len, sizeof(len), 1, f );
    } else {
	len = strlen(s)+1;
	fwrite( &len, sizeof(len), 1, f );
	fwrite( s, len, 1, f );
    }
}

int fread_string( char *s, int maxlen, FILE *f ) 
{
    uint32_t len;
    fread( &len, sizeof(len), 1, f );
    if( len != 0 ) {
	fread( s, len > maxlen ? maxlen : len, 1, f );
    }
    return len;
}
