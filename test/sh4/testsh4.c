#include <stdio.h>

int total_tests = 0;
int total_fails = 0;

int test_print_result( char *testname, int failed, int total )
{
    fprintf( stderr, "%s: %d/%d tests passed\n", testname, total-failed, total );
    total_tests += total;
    total_fails += failed;
    return failed;
}

void test_print_failure( char *testname, int number, char *message )
{
    if( message == NULL ) {
	fprintf( stderr, "%s: Test %d failed!\n", testname, number );
    } else {
	fprintf( stderr, "%s: Test %d failed: %s\n", testname, number, message );
    }
}

int main()
{
    
    test_add();
    test_addc();
    test_addv();
    test_and();
    test_andi();

    fprintf( stderr, "Total: %d/%d tests passed (%d%%)\n", total_tests-total_fails,
	     total_tests, ((total_tests-total_fails)*100)/total_tests );
}
