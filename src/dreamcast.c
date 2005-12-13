#include "dream.h"
#include "mem.h"
#include "aica/aica.h"
#include "asic.h"
#include "ide.h"
#include "dreamcast.h"
#include "maple.h"
#include "modules.h"

/* Central switchboard for the system */

#define MAX_MODULES 32
static int num_modules = 0;
static int dreamcast_state = 0;
static char *dreamcast_config = "DEFAULT";
dreamcast_module_t modules[MAX_MODULES];

/**
 * This function is responsible for defining how all the pieces of the
 * dreamcast actually fit together. Among other things, this lets us
 * (reasonably) easily redefine the structure for eg various versions of the
 * Naomi.
 *
 * Note currently the locations of the various MMIO pages are hard coded in
 * the MMIO definitions - they should probably be moved here.
 */
void dreamcast_configure( )
{
    /* Register the memory framework */
    dreamcast_register_module( &mem_module );

    /* Setup standard memory map */
    mem_create_ram_region( 0x0C000000, 16 MB, MEM_REGION_MAIN );
    mem_create_ram_region( 0x00800000, 2 MB, MEM_REGION_AUDIO );
    mem_create_ram_region( 0x00703000, 8 KB, MEM_REGION_AUDIO_SCRATCH );
    mem_create_ram_region( 0x05000000, 8 MB, MEM_REGION_VIDEO );
    mem_load_rom( "dcboot.rom", 0x00000000, 0x00200000, 0x89f2b1a1 );
    mem_load_rom( "dcflash.rom",0x00200000, 0x00020000, 0x357c3568 );

    /* Load in the rest of the core modules */
    dreamcast_register_module( &sh4_module );
    dreamcast_register_module( &asic_module );
    dreamcast_register_module( &pvr2_module );
    dreamcast_register_module( &aica_module );
    dreamcast_register_module( &maple_module );
    dreamcast_register_module( &ide_module );

    /* Attach any default maple devices, ie a pair of controllers */
    maple_device_t controller1 = controller_new();
    maple_device_t controller2 = controller_new();
    maple_attach_device( controller1, 0, 0 );
    maple_attach_device( controller2, 1, 0 );
}

void dreamcast_register_module( dreamcast_module_t module ) 
{
    modules[num_modules++] = module;
    if( module->init != NULL )
	module->init();
}


void dreamcast_init( void )
{
    dreamcast_configure();
}

void dreamcast_reset( void )
{
    int i;
    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->reset != NULL )
	    modules[i]->reset();
    }
}

void dreamcast_start( void )
{
    int i;
    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->start != NULL )
	    modules[i]->start();
    }
}
void dreamcast_stop( void )
{
    int i;
    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->stop != NULL )
	    modules[i]->stop();
    }
}

struct save_state_header {
    char magic[16];
    uint32_t version;
    uint32_t module_count;
};


int dreamcast_load_state( FILE *f )
{
    int i,j;
    uint32_t count, len;
    int have_read[MAX_MODULES];
    char tmp[64];
    struct save_state_header header;

    fread( &header, sizeof(header), 1, f );
    if( strncmp( header.magic, DREAMCAST_SAVE_MAGIC, 16 ) != 0 ) {
	ERROR( "Not a DreamOn save state file" );
	return 1;
    }
    if( header.version != DREAMCAST_SAVE_VERSION ) {
	ERROR( "DreamOn save state version not supported" );
	return 1;
    }
    fread( &count, sizeof(count), 1, f );
    if( count > MAX_MODULES ) {
	ERROR( "DreamOn save state is corrupted" );
	return 1;
    }
    for( i=0; i<MAX_MODULES; i++ ) {
	have_read[i] = 0;
    }

    for( i=0; i<count; i++ ) {
	fread(tmp, 4, 1, f );
	if( strcmp(tmp, "BLCK") != 0 ) {
	    ERROR( "DreamOn save state is corrupted" );
	    return 2;
	}
	len = fread_string(tmp, sizeof(tmp), f );
	if( len > 64 || len < 1 ) {
	    ERROR( "DreamOn save state is corrupted" );
	    return 2;
	}
	
	/* Find the matching module by name */
	for( j=0; j<num_modules; j++ ) {
	    if( strcmp(modules[j]->name,tmp) == 0 ) {
		have_read[j] = 1;
		if( modules[j]->load == NULL ) {
		    ERROR( "DreamOn save state is corrupted" );
		    return 2;
		} else if( modules[j]->load(f) != 0 ) {
		    ERROR( "DreamOn save state is corrupted" );
		    return 2;
		}
		break;
	    }
	}
	if( j == num_modules ) {
	    ERROR( "DreamOn save state contains unrecognized section" );
	    return 2;
	}
    }
    
    /* Any modules that we didn't load - reset to the default state.
     * (ie it's not an error to skip a module if you don't actually
     * care about its state).
     */
    for( j=0; j<num_modules; j++ ) {
	if( have_read[j] == 0 && modules[j]->reset != NULL ) {
	    modules[j]->reset();
	}
    }
}

void dreamcast_save_state( FILE *f )
{
    int i;
    struct save_state_header header;
    
    strcpy( header.magic, DREAMCAST_SAVE_MAGIC );
    header.version = DREAMCAST_SAVE_VERSION;
    header.module_count = num_modules;
    fwrite( &header, sizeof(header), 1, f );
    fwrite_string( dreamcast_config, f );
    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->save != NULL ) {
	    fwrite( "BLCK", 4, 1, f );
	    fwrite_string( modules[i]->name, f );
	    modules[i]->save(f);
	}
    }
}

