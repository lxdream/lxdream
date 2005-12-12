#include "dream.h"
#include "mem.h"
#include "aica/aica.h"
#include "asic.h"
#include "ide.h"
#include "dreamcast.h"
#include "modules.h"

/* Central switchboard for the system */

#define MAX_MODULES 32
static int num_modules = 0;
static int dreamcast_state = 0;
dreamcast_module_t modules[];

void dreamcast_configure( )
{
    dreamcast_register_module( &mem_module );
    dreamcast_register_module( &sh4_module );
    dreamcast_register_module( &asic_module );
    dreamcast_register_module( &pvr2_module );
    dreamcast_register_module( &aica_module );
    dreamcast_register_module( &maple_module );
    dreamcast_register_module( &ide_module );
}

void dreamcast_register_module( dreamcast_module_t module ) 
{
    modules[num_modules++] = module;
}


void dreamcast_init( void )
{
    int i;
    dreamcast_configure();
    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->init != NULL )
	    modules[i]->init();
    }
    mem_load_rom( "dcboot.rom", 0x00000000, 0x00200000, 0x89f2b1a1 );
    mem_load_rom( "dcflash.rom",0x00200000, 0x00020000, 0x357c3568 );
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
};

void dreamcast_load_state( FILE *f )
{
    int i;
    struct save_state_header header;

    fread( &header, sizeof(header), 1, f );
    if( strncmp( header.magic, DREAMCAST_SAVE_MAGIC, 16 ) != 0 ) {
	ERROR( "Not a DreamOn save state file" );
	return;
    }
    if( header.version != DREAMCAST_SAVE_VERSION ) {
	ERROR( "DreamOn save state version not supported" );
	return;
    }

    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->load != NULL )
	    modules[i]->load(f);
	else if( modules[i]->reset != NULL )
	    modules[i]->reset();
    }
}

void dreamcast_save_state( FILE *f )
{
    int i;
    struct save_state_header header;
    
    strcpy( header.magic, DREAMCAST_SAVE_MAGIC );
    header.version = DREAMCAST_SAVE_VERSION;
    fwrite( &header, sizeof(header), 1, f );
    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->save != NULL ) {
	    modules[i]->save(f);
	}
    }
}

