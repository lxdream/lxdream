#define D #define
#define tst(a) D a 1

tst(forte)

int main()
{
	signed int a = 0xFFFFFFFF;
	unsigned int b = 0xFFFFFFFF;
	
	a >>= 16;
	b >>= 16;
	
	printf( "%08x %08x\n", a, b );
}
