#include <stdlib.h>
#include <stdio.h>

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef unsigned int uint32_t;
typedef signed int int32_t;
typedef unsigned long long uint64_t;
typedef signed long long int64_t;

#define TEST_REPS 64

void printuint8_t(uint8_t a) { printf( "%08X ", (unsigned int)a ); }
void printint8_t(int8_t a) { printf( "%08X ", (unsigned int)(uint8_t)a ); }
void printuint16_t(uint16_t a) { printf( "%08X ", (unsigned int)a ); }
void printint16_t(int16_t a) { printf( "%08X ", (unsigned int)(uint16_t)a ); }
void printuint32_t(uint32_t a) { printf( "%08X ", (unsigned int)a ); }
void printint32_t(int32_t a) { printf( "%08X ", (unsigned int)a ); }
void printuint64_t(uint64_t a) { printf( "%08X%08X ", (unsigned int)(a>>32),(unsigned int)a ); }
void printint64_t(int64_t a) { printf( "%08X%08X ", (unsigned int)(a>>32),(unsigned int)a ); }

#define TEST_TYPE(T) \
void test##T() { \
    T a, b, c, d, e, f; \
    read(0,&a,sizeof(a)); read(0,&b,sizeof(b)); read(0,&c,sizeof(c)); read(0,&d,sizeof(d)); \
    e = a * b - c * d;\
    if( b != 0 && c != 0 ) { f = a / b + 1/c;  }\
    T g = e - f / 2  + (uint32_t)b;\
    T h = b * c / d; T i = a / -d; \
    printf( "%d| ", sizeof(a) ); \
    print##T(a); print##T(b); print##T(c); print##T(d); \
    printf( ": " ); print##T(e); print##T(f); print##T(g); \
    print##T(h); print##T(i); \
    printf( "\n" ); \
}

TEST_TYPE(uint8_t);
TEST_TYPE(int8_t);
TEST_TYPE(uint16_t);
TEST_TYPE(int16_t);
TEST_TYPE(uint32_t);
TEST_TYPE(int32_t);
TEST_TYPE(uint64_t);
TEST_TYPE(int64_t);

int main(int argc, char *argv[]) {
  unsigned i;
  
  for(i=0;i<TEST_REPS;i++ ) { testuint64_t(); }
  for(i=0;i<TEST_REPS;i++ ) { testint64_t(); }
  for(i=0;i<TEST_REPS;i++ ) { testuint32_t(); }
  for(i=0;i<TEST_REPS;i++ ) { testint32_t(); }
  for(i=0;i<TEST_REPS;i++ ) { testuint16_t(); }
  for(i=0;i<TEST_REPS;i++ ) { testint16_t(); }
  for(i=0;i<TEST_REPS;i++ ) { testuint8_t(); }
  for(i=0;i<TEST_REPS;i++ ) { testint8_t(); }
  return 0;
}
