#include <stdlib.h>
#include <stdio.h>

typedef unsigned char u8;
typedef signed char i8;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned int u32;
typedef signed int i32;
typedef unsigned long long u64;
typedef signed long long i64;

#define TEST_REPS 64

void printu8(u8 a) { printf( "%08X ", (unsigned int)a ); }
void printi8(i8 a) { printf( "%08X ", (unsigned int)(u8)a ); }
void printu16(u16 a) { printf( "%08X ", (unsigned int)a ); }
void printi16(i16 a) { printf( "%08X ", (unsigned int)(u16)a ); }
void printu32(u32 a) { printf( "%08X ", (unsigned int)a ); }
void printi32(i32 a) { printf( "%08X ", (unsigned int)a ); }
void printu64(u64 a) { printf( "%08X%08X ", (unsigned int)(a>>32),(unsigned int)a ); }
void printi64(i64 a) { printf( "%08X%08X ", (unsigned int)(a>>32),(unsigned int)a ); }

#define TEST_TYPE(T) \
void test##T() { \
    T a, b, c, d, e, f; \
    read(0,&a,sizeof(a)); read(0,&b,sizeof(b)); read(0,&c,sizeof(c)); read(0,&d,sizeof(d)); \
    e = a * b - c * d;\
    if( b != 0 && c != 0 ) { f = a / b + 1/c;  }\
    T g = e - f / 2  + (u32)b;\
    T h = b * c / d; T i = a / -d; \
    printf( "%d| ", (int)sizeof(a) ); \
    print##T(a); print##T(b); print##T(c); print##T(d); \
    printf( ": " ); print##T(e); print##T(f); print##T(g); \
    print##T(h); print##T(i); \
    printf( "\n" ); \
}

TEST_TYPE(u8);
TEST_TYPE(i8);
TEST_TYPE(u16);
TEST_TYPE(i16);
TEST_TYPE(u32);
TEST_TYPE(i32);
TEST_TYPE(u64);
TEST_TYPE(i64);

int main(int argc, char *argv[]) {
  unsigned i;
  
  for(i=0;i<TEST_REPS;i++ ) { testu64(); }
  for(i=0;i<TEST_REPS;i++ ) { testi64(); }
  for(i=0;i<TEST_REPS;i++ ) { testu32(); }
  for(i=0;i<TEST_REPS;i++ ) { testi32(); }
  for(i=0;i<TEST_REPS;i++ ) { testu16(); }
  for(i=0;i<TEST_REPS;i++ ) { testi16(); }
  for(i=0;i<TEST_REPS;i++ ) { testu8(); }
  for(i=0;i<TEST_REPS;i++ ) { testi8(); }
  return 0;
}
