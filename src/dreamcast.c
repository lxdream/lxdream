#include "dream.h"
#include "mem.h"
#include "aica.h"
#include "asic.h"
#include "ide.h"
#include "dreamcast.h"
/* Central switchboard for the system */

void dreamcast_init( void )
{
    mem_init();
    sh4_init();
    asic_init();
    pvr2_init();
    aica_init();
    ide_reset();

    mem_create_ram_region( 0x0C000000, 16 MB, MEM_REGION_MAIN );
    mem_create_ram_region( 0x05000000, 8 MB, MEM_REGION_VIDEO );
    mem_create_ram_region( 0x00800000, 2 MB, MEM_REGION_AUDIO );
    mem_create_ram_region( 0x00703000, 8 KB, MEM_REGION_AUDIO_SCRATCH ); /*???*/
    mem_load_rom( "dcboot.rom", 0x00000000, 0x00200000, 0x89f2b1a1 );
    mem_load_rom( "dcflash.rom",0x00200000, 0x00020000, 0x357c3568 );
}

void dreamcast_reset( void )
{
    sh4_reset();
    mem_reset();
//    pvr2_reset();
    aica_reset();
}

void dreamcast_stop( void )
{
    sh4_stop();
}
