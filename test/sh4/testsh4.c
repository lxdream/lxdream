#include <stdio.h>

int test_print_result( char *testname, int failed, int total )
{
    fprintf( stderr, "%s: %d/%d tests passed\n", testname, total-failed, total );
    return failed;
}

void test_print_failure( char *testname, int number )
{
    fprintf( stderr, "%s: Test %d failed!\n", testname, number );
}

int main()
{
    
    test_add();
}
