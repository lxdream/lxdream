#include "utlb.h"
#include "../lib.h"

struct utlb_test_case {
    const char *name;
    uint32_t vma;
    uint32_t pma;
    int read_exc;
    int write_exc;
};

#define OK 0

#define MAX_BATCH_ENTRIES 4
#define MAX_BATCH_TESTS 8

uint32_t dummy;

#define LOAD(ent,asid,vpn,ppn,mode) load_utlb_entry( ent, vpn, ppn, asid, mode )
#define TEST(name,asid,vma,pma,sr,sw,ur,uw) run_utlb_test_case(name,asid,vma,pma,sr,sw,ur,uw) 

int run_utlb_priv_test( struct utlb_test_case *test );
int run_utlb_user_test( struct utlb_test_case *test );

int cases_failed;
int cases_run;
int tests_failed;
int tests_run;
int tests_skipped;

int run_utlb_test_case( const char *name, int asid, unsigned int vma, unsigned int pma,
                          int sr, int sw, int ur, int uw )
{
    char tmp[64];
    struct utlb_test_case test = { tmp, vma, pma, sr, sw };
    int fails = 0;
    
    cases_run++;
    
    set_asid( asid );
    if( sr == OTLBMULTIHIT || sw == OTLBMULTIHIT ) {
        fprintf( stderr, "%s: Skipping system test (multihit)\n", name );
        tests_skipped += 2;
    } else {
        snprintf(tmp,sizeof(tmp), "%s (System)", name );
        tests_run += 2;
        fails += run_utlb_priv_test( &test );
    }
    
    if( ur == OTLBMULTIHIT || uw == OTLBMULTIHIT ) {
        fprintf( stderr, "%s: Skipping user test '%s' (multihit)\n", name );
        tests_skipped += 2;
    } else {
        snprintf(tmp,sizeof(tmp), "%s (User)", name ); 
        test.read_exc = ur;
        test.write_exc = uw;
        tests_run += 2;
        fails += run_utlb_user_test( &test );
    }
    if( fails != 0 ) {
        cases_failed++;
        tests_failed += fails;
    }
    return fails;
}

int main()
{
    /* Non-TLB behaviour tests */
    
    
    /* TLB tests */
    install_utlb_test_handler();
    invalidate_tlb();
    /* Permanently map the first and last MB of RAM into userspace - without 
     * this it's a bit hard to actually run any user-mode tests.
     */
    LOAD(62, 0, 0x0C000000, 0x0C000000, TLB_VALID|TLB_USERMODE|TLB_WRITABLE|TLB_SIZE_1M|TLB_CACHEABLE|TLB_DIRTY|TLB_SHARE);
    LOAD(63, 0, 0x0CF00000, 0x0CF00000, TLB_VALID|TLB_USERMODE|TLB_WRITABLE|TLB_SIZE_1M|TLB_CACHEABLE|TLB_DIRTY|TLB_SHARE);
    set_tlb_enabled(1);
    
    /* Test miss */
    TEST( "U0", 0, 0x12345008, 0x0c000018, RTLBMISS, WTLBMISS, RTLBMISS, WTLBMISS );
    TEST( "P1", 0, 0x8c000018, 0x0c000018, OK, OK, RADDERR, WADDERR );
    TEST( "P1", 0, 0xac000018, 0x0c000018, OK, OK, RADDERR, WADDERR );
    TEST( "P3", 0, 0xC4345008, 0x0c000018, RTLBMISS, WTLBMISS, RADDERR, WADDERR );

    /* Test flags with 1K pages */
    LOAD( 0, 0, 0x12345C00, 0x0C000000, TLB_VALID|TLB_USERMODE|TLB_WRITABLE|TLB_SIZE_1K|TLB_CACHEABLE|TLB_DIRTY );
    LOAD( 1, 1, 0x12345C00, 0x0CFFFC00, TLB_VALID|TLB_USERMODE|TLB_WRITABLE|TLB_SIZE_1K|TLB_CACHEABLE|TLB_DIRTY );
    LOAD( 2, 0, 0x12345800, 0x0C000800, TLB_VALID|TLB_WRITABLE|TLB_SIZE_1K|TLB_CACHEABLE|TLB_DIRTY );
    LOAD( 3, 0, 0x12345400, 0x0C000400, TLB_VALID|TLB_USERMODE|TLB_SIZE_1K|TLB_CACHEABLE|TLB_DIRTY );
    LOAD( 4, 0, 0x12345000, 0x0C000000, TLB_VALID|TLB_SIZE_1K|TLB_CACHEABLE|TLB_DIRTY );
    LOAD( 5, 1, 0x12345800, 0x0CF01800, TLB_VALID|TLB_USERMODE|TLB_WRITABLE|TLB_SIZE_1K|TLB_CACHEABLE );
    LOAD( 6, 1, 0x12346000, 0x0C000000, TLB_VALID|TLB_WRITABLE|TLB_SIZE_1K|TLB_CACHEABLE );
    LOAD( 7, 2, 0x12346800, 0x0C000400, TLB_VALID|TLB_USERMODE|TLB_WRITABLE|TLB_SIZE_1K|TLB_CACHEABLE|TLB_SHARE|TLB_DIRTY );
    TEST( "1K ASID 0",   0, 0x12345C18, 0x0C000018, OK, OK, OK, OK );
    TEST( "1K ASID 1",   1, 0x12345C18, 0x0CFFFC18, OK, OK, OK, OK );
    TEST( "1K ASID 2",   2, 0x12345C18, 0x0C000018, RTLBMISS, WTLBMISS, RTLBMISS, WTLBMISS );
    TEST( "1K PRIV",     0, 0x12345818, 0x0C000818, OK, OK, READPROT, WRITEPROT );
    TEST( "1K READONLY", 0, 0x12345418, 0x0C000418, OK, WRITEPROT, OK, WRITEPROT );
    TEST( "1K PRIVREAD", 0, 0x12345018, 0x0C000018, OK, WRITEPROT, READPROT, WRITEPROT );
    TEST( "1K FIRSTWR",  1, 0x12345818, 0x0CF01818, OK, FIRSTWRITE, OK, FIRSTWRITE ); 
    TEST( "1K PRIVFWR",  1, 0x12346018, 0x0C000018, OK, FIRSTWRITE, READPROT, WRITEPROT );
    TEST( "1K MISS",     1, 0x12346418, 0x0C000018, RTLBMISS, WTLBMISS, RTLBMISS, WTLBMISS );
    TEST( "1K SHARED 0", 0, 0x12346818, 0x0C000418, OK, OK, OK, OK );
    TEST( "1K SHARED 2", 2, 0x12346818, 0x0C000418, OK, OK, OK, OK );
    
    
    uninstall_utlb_test_handler();
    
    
    printf( "--> %d/%d Test cases passed (%d%%)\n", cases_run-cases_failed, cases_run, ( (cases_run-cases_failed)*100/cases_run) );
    printf( "--> %d/%d Tests passed (%d%%)\n", tests_run-tests_failed, tests_run, ( (tests_run-tests_failed)*100/tests_run) );
    
    return cases_failed == 0 ? 0 : 1;
}
