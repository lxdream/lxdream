#include <assert.h>
#include "sh4/xltcache.h"
#include "dreamcast.h"

extern xlat_cache_block_t xlat_new_cache;
extern xlat_cache_block_t xlat_new_cache_ptr;
extern xlat_cache_block_t xlat_temp_cache;
extern xlat_cache_block_t xlat_temp_cache_ptr;

/**
 * Test initial allocations from the new cache
 */
void test_initial()
{
    int i;
    xlat_cache_block_t block = xlat_start_block( 0x0C008000 );
    assert( block->active == 1 );
    assert( block->size == XLAT_NEW_CACHE_SIZE - (2*sizeof(struct xlat_cache_block)) );
    memset( block->code, 0xB5, 8192 );
    xlat_commit_block( 8192, 100 );
    assert( block->active == 1 );
    assert( block->size == 8192 );
    
    int size = XLAT_NEW_CACHE_SIZE - (4*sizeof(struct xlat_cache_block)) - 8192 - 4096;
    xlat_cache_block_t block2 = xlat_start_block( 0x0C009000 );
    assert( block2->active == 1 );
    assert( block2->size == XLAT_NEW_CACHE_SIZE - (3*sizeof(struct xlat_cache_block)) - 8192 );
    memset( block2->code, 0x6D, size );
    xlat_commit_block( size, 200 );
    assert( block2->active == 1 );
    assert( block2->size == size );
    
    void *addr = xlat_get_code( 0x0C008000 );
    assert( addr == &block->code );
    addr = xlat_get_code( 0x0C009000 );
    assert( addr == &block2->code );
    addr = xlat_get_code( 0x0C008002 );
    assert( addr == NULL );
    
    xlat_cache_block_t block3 = xlat_start_block( 0x0D009800 );
    assert( block3->active == 1 );
    assert( block3->size == 4096 );
    memset( block3->code, 0x9C, 4096 );
    xlat_cache_block_t block3a = xlat_extend_block(8192);
    assert( block3a != block3 );
    assert( block3a == block );
    assert( block3a->active == 1 );
    assert( block3a->size = 8192 );
    assert( block3->active == 0 );
    assert( block3->size == 4096 );
    for( i=0; i<4096; i++ ) {
	assert( block3a->code[i] == 0x9C );
    }
    for( i=4096; i<8192; i++ ) {
	assert( block3a->code[i] == 0xB5 );
    }
    xlat_commit_block(6142, 432);
    addr = xlat_get_code( 0x0D009800 );
    assert( addr == &block3a->code );
    /* check promoted block in temp cache */
    addr = xlat_get_code( 0x0C008000 );
    assert( addr == &xlat_temp_cache->code );
    assert( xlat_temp_cache->active == 1 );
    assert( xlat_temp_cache->size == 8192 );
    for( i=0; i<8192; i++ ) {
	assert( xlat_temp_cache->code[i] == 0xB5 );
    }
}

int main()
{
    xlat_cache_init();
    xlat_check_integrity();
    
    test_initial();
    return 0;
}
