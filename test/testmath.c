#include <stdio.h>
#include <assert.h>
#include <math.h>

#define write_string(x) printf(x)
#define write_int(x) printf( "%08X", x )

int do_fsca(int angle, float *a, float *b);
float do_fsrra( float param );
float do_fipr( float *vectora, float *vectorb );
float do_fipr2( float *vectora, float *vectorb );
void do_ftrv( float *matrix, float *vector );
void do_ftrv2( float *matrix, float *vector );

unsigned int get_fpscr();
void set_fpscr( unsigned int fpscr );

#define MAX_FLOAT_DIFF 0.00001


int compare_float( float a, float b )
{
    if( a == b )
	return 1;
    float diff = (a>b?(a-b):(b-a));
    return diff < MAX_FLOAT_DIFF;
}

#define TEST_FEQUALS( a, b ) if( !compare_float(a,b) ) { printf( "Assertion failed at %s.%d: expected %.8f but was %.8f\n", __FILE__, __LINE__, a, b ); return 1; }

int test_fsca( int angle, float expect_a, float expect_b )
{
    float a = 10.0;
    float b = 10.0;
    do_fsca(angle, &a, &b );
    
    TEST_FEQUALS(expect_a, a);
    TEST_FEQUALS(expect_b, b);
}

int test_fsrra( float f, float expect )
{
    float f2 = do_fsrra( f );
    TEST_FEQUALS( expect, f2 );
}

void test_coercion( float f )
{
    double d = (double)f;
    float q = (float)d;
    unsigned int i = (unsigned int)d;
    signed int j = (signed int)q;

    printf( "Coerce: %08X %08X%08X %08X %08X %08X\n", *((unsigned int *)&f),
    	    *((unsigned int *)&d),  *(((unsigned int *)&d)+1),
    	    *((unsigned int *)&q), i, j );
}

void test_doublearr( int len )
{
    double arr[len];
    unsigned int *iarr = (unsigned int *)&arr;
    int i;
    arr[0] = 2.5;
    for( i=1; i<len; i++ ) {
	arr[i] = (arr[i-1] * arr[i-1] + arr[0]) / 1.25 - 0.19;
    }

    printf( "arr: " );
    for( i=0; i<len; i++ ) {
	printf( "%08X", *iarr++ );
	printf( "%08X ", *iarr++ );
    }
    printf( "\n" );
}

void test_floatarr( int len )
{
    float arr[len];
    unsigned int *iarr = (unsigned int *)&arr;
    int i;
    arr[0] = 2.5;
    for( i=1; i<len; i++ ) {
	arr[i] = (arr[i-1] * arr[i-1] + arr[0]) / 1.25 - 0.19;
    }

    write_string( "arr: " );
    for( i=0; i<len; i++ ) {
	write_int( *iarr++ );
	write_string( " " );
    }
    write_string( "\n" );
}

float __attribute__((aligned(8))) matrix[16] = { 1.26, 2.34, 5.67, 3.497, -1.23, 43.43, -45.68, 9.12, 
			 12.1, 34.301, -297.354, 0.05, 0.123, 23.34, 9.99, 33.321 };
float __attribute__((aligned(8))) vec1[4] = { 5.65, 9.98, -34.12, 0.043 };
float __attribute__((aligned(8))) vec2[4] = { 3.45, -123, 4.54, 98.0909 };
float __attribute__((aligned(8))) vec3[4] = { 1.25, 2.25, 3.75, 5.12 };
float __attribute__((aligned(8))) vec4[4] = {-0.25, 8.9, -2.3, 9.19 };

void test_fipr( )
{
    float r = do_fipr( vec3, vec4 );
    write_string( "fipr: " );
    write_int( *(unsigned int *)&r );
    write_string( "\n" );
    
    r = do_fipr2( vec4, vec3 );
    write_string( "fipr: " );
    write_int( *(unsigned int *)&r );
    write_string( "\n" );
    
}

void test_ftrv( )
{
    
    do_ftrv( matrix, vec1 );
    write_string( "ftrv: " );
    write_int( *(unsigned int *)&vec1[0] );
    write_int( *(unsigned int *)&vec1[1] );
    write_int( *(unsigned int *)&vec1[2] );
    write_int( *(unsigned int *)&vec1[3] );
    write_string( "\n" );
    
    do_ftrv2( matrix, vec2 );
    
    write_string( "ftrv: " );
    write_int( *(unsigned int *)&vec2[0] );
    write_int( *(unsigned int *)&vec2[1] );
    write_int( *(unsigned int *)&vec2[2] );
    write_int( *(unsigned int *)&vec2[3] );
    write_string( "\n" );
    
}
    

int main()
{
	write_int( get_fpscr() );
	write_string( "\n" );
	test_fsca( 0x00000000, 0, 1 );
	test_fsca( 0x00011234, 0.43205646, 0.90184659 );
	test_fsca( 0xFEDCBA98, -0.99121743, -0.13224234 );
	test_fsrra( 25.0, 0.2 );
	test_fsrra( 0.05, 4.4721361 );
	test_fsrra( -12.345, nanf() );
	test_coercion( -3.1415926 );
	test_coercion( 123456789012346567.890 );
	test_coercion( -2147483648.0 );
	test_coercion( 5234.1234 );
	test_doublearr( 5 );
	test_floatarr( 5 );
	test_fipr();
	test_ftrv();
	return 1;
}
	
