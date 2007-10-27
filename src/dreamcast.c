/**
 * $Id: dreamcast.c,v 1.26 2007-10-27 05:47:55 nkeynes Exp $
 * Central switchboard for the system. This pulls all the individual modules
 * together into some kind of coherent structure. This is also where you'd
 * add Naomi support, if I ever get a board to play with...
 *
 * Copyright (c) 2005 Nathan Keynes.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <errno.h>
#include "dream.h"
#include "config.h"
#include "mem.h"
#include "aica/aica.h"
#include "asic.h"
#include "dreamcast.h"
#include "gdrom/ide.h"
#include "maple/maple.h"

/**
 * Current state of the DC virtual machine
 */
#define STATE_UNINIT 0
#define STATE_RUNNING 1
#define STATE_STOPPING 2
#define STATE_STOPPED 3 
static volatile int dreamcast_state = STATE_UNINIT;
static uint32_t timeslice_length = DEFAULT_TIMESLICE_LENGTH;
const char *dreamcast_config = "DEFAULT";

#define MAX_MODULES 32
static int num_modules = 0;
dreamcast_module_t modules[MAX_MODULES];

/**
 * The unknown module is used for logging files without an actual module
 * declaration
 */
struct dreamcast_module unknown_module = { "****", NULL, NULL, NULL, NULL, 
					   NULL, NULL, NULL };

/**
 * This function is responsible for defining how all the pieces of the
 * dreamcast actually fit together. 
 *
 * Note currently the locations of the various MMIO pages are hard coded in
 * the MMIO definitions - they should probably be moved here.
 */
void dreamcast_configure( )
{
    dreamcast_register_module( &eventq_module );
    /* Register the memory framework */
    dreamcast_register_module( &mem_module );

    /* Setup standard memory map */
    mem_create_repeating_ram_region( 0x0C000000, 16 MB, MEM_REGION_MAIN, 0x01000000, 0x0F000000 );
    mem_create_ram_region( 0x00800000, 2 MB, MEM_REGION_AUDIO );
    mem_create_ram_region( 0x00703000, 8 KB, MEM_REGION_AUDIO_SCRATCH );
    mem_create_ram_region( 0x05000000, 8 MB, MEM_REGION_VIDEO );
    mem_load_rom( lxdream_get_config_value(CONFIG_BIOS_PATH),
		  0x00000000, 0x00200000, 0x89f2b1a1, MEM_REGION_BIOS );
    mem_create_ram_region( 0x00200000, 0x00020000, MEM_REGION_FLASH );
    mem_load_block( lxdream_get_config_value(CONFIG_FLASH_PATH),
		    0x00200000, 0x00020000 );

    /* Load in the rest of the core modules */
    dreamcast_register_module( &sh4_module );
    dreamcast_register_module( &asic_module );
    dreamcast_register_module( &pvr2_module );
    dreamcast_register_module( &aica_module );
    dreamcast_register_module( &maple_module );
    dreamcast_register_module( &ide_module );
}

void dreamcast_config_changed(void)
{
    mem_load_rom( lxdream_get_config_value(CONFIG_BIOS_PATH),
		  0x00000000, 0x00200000, 0x89f2b1a1, MEM_REGION_BIOS );
    mem_load_block( lxdream_get_config_value(CONFIG_FLASH_PATH),
		    0x00200000, 0x00020000 );
}

void dreamcast_save_flash()
{
    const char *file = lxdream_get_config_value(CONFIG_FLASH_PATH);
    mem_save_block( file, 0x00200000, 0x00020000 );
}

/**
 * Constructs a system configuration for the AICA in standalone mode,
 * ie sound chip only.
 */
void dreamcast_configure_aica_only( )
{
    dreamcast_register_module( &mem_module );
    mem_create_ram_region( 0x00800000, 2 MB, MEM_REGION_AUDIO );
    mem_create_ram_region( 0x00703000, 8 KB, MEM_REGION_AUDIO_SCRATCH );
    dreamcast_register_module( &aica_module );
    aica_enable();
    dreamcast_state = STATE_STOPPED;
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
    dreamcast_state = STATE_STOPPED;
}

void dreamcast_reset( void )
{
    int i;
    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->reset != NULL )
	    modules[i]->reset();
    }
}

void dreamcast_run( void )
{
    int i;
    if( dreamcast_state != STATE_RUNNING ) {
	for( i=0; i<num_modules; i++ ) {
	    if( modules[i]->start != NULL )
		modules[i]->start();
	}
    }
    dreamcast_state = STATE_RUNNING;
    while( dreamcast_state == STATE_RUNNING ) {
	int time_to_run = timeslice_length;
	for( i=0; i<num_modules; i++ ) {
	    if( modules[i]->run_time_slice != NULL )
		time_to_run = modules[i]->run_time_slice( time_to_run );
	}

    }

    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->stop != NULL )
	    modules[i]->stop();
    }
    dreamcast_state = STATE_STOPPED;
}

void dreamcast_run_for( unsigned int seconds, unsigned int nanosecs )
{
    
    int i;
    if( dreamcast_state != STATE_RUNNING ) {
	for( i=0; i<num_modules; i++ ) {
	    if( modules[i]->start != NULL )
		modules[i]->start();
	}
    }
    dreamcast_state = STATE_RUNNING;
    uint32_t nanos = 0;
    if( nanosecs != 0 ) {
        nanos = 1000000000 - nanosecs;
	seconds++;
    }
    while( dreamcast_state == STATE_RUNNING && seconds != 0 ) {
	int time_to_run = timeslice_length;
	for( i=0; i<num_modules; i++ ) {
	    if( modules[i]->run_time_slice != NULL )
		time_to_run = modules[i]->run_time_slice( time_to_run );
	}
	nanos += time_to_run;
	if( nanos >= 1000000000 ) {
	    nanos -= 1000000000;
	    seconds--;
	}
    }

    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->stop != NULL )
	    modules[i]->stop();
    }
    dreamcast_state = STATE_STOPPED;
}

void dreamcast_stop( void )
{
    if( dreamcast_state == STATE_RUNNING )
	dreamcast_state = STATE_STOPPING;
}

void dreamcast_shutdown()
{
    dreamcast_stop();
    dreamcast_save_flash();
}

gboolean dreamcast_is_running( void )
{
    return dreamcast_state == STATE_RUNNING;
}

/********************************* Save States *****************************/

struct save_state_header {
    char magic[16];
    uint32_t version;
    uint32_t module_count;
};

int dreamcast_load_state( const gchar *filename )
{
    int i,j;
    uint32_t len;
    int have_read[MAX_MODULES];
    char tmp[64];
    struct save_state_header header;
    FILE *f;

    f = fopen( filename, "r" );
    if( f == NULL ) return errno;

    fread( &header, sizeof(header), 1, f );
    if( strncmp( header.magic, DREAMCAST_SAVE_MAGIC, 16 ) != 0 ) {
	ERROR( "Not a %s save state file", APP_NAME );
	return 1;
    }
    if( header.version != DREAMCAST_SAVE_VERSION ) {
	ERROR( "%s save state version not supported", APP_NAME );
	return 1;
    }
    if( header.module_count > MAX_MODULES ) {
	ERROR( "%s save state is corrupted (bad module count)", APP_NAME );
	return 1;
    }
    for( i=0; i<MAX_MODULES; i++ ) {
	have_read[i] = 0;
    }

    for( i=0; i<header.module_count; i++ ) {
	fread(tmp, 4, 1, f );
	if( strncmp(tmp, "BLCK", 4) != 0 ) {
	    ERROR( "%s save state is corrupted (missing block header %d)", APP_NAME, i );
	    return 2;
	}
	len = fread_string(tmp, sizeof(tmp), f );
	if( len > 64 || len < 1 ) {
	    ERROR( "%s save state is corrupted (bad string)", APP_NAME );
	    return 2;
	}
	
	/* Find the matching module by name */
	for( j=0; j<num_modules; j++ ) {
	    if( strcmp(modules[j]->name,tmp) == 0 ) {
		have_read[j] = 1;
		if( modules[j]->load == NULL ) {
		    ERROR( "%s save state is corrupted (no loader for %s)", APP_NAME, modules[j]->name );
		    return 2;
		} else if( modules[j]->load(f) != 0 ) {
		    ERROR( "%s save state is corrupted (%s failed)", APP_NAME, modules[j]->name );
		    return 2;
		}
		break;
	    }
	}
	if( j == num_modules ) {
	    ERROR( "%s save state contains unrecognized section", APP_NAME );
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
    fclose(f);
    INFO( "Save state read from %s", filename );
    return 0;
}

int dreamcast_save_state( const gchar *filename )
{
    int i;
    FILE *f;
    struct save_state_header header;
    
    f = fopen( filename, "w" );
    if( f == NULL )
	return errno;
    strcpy( header.magic, DREAMCAST_SAVE_MAGIC );
    header.version = DREAMCAST_SAVE_VERSION;
    header.module_count = 0;

    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->save != NULL )
	    header.module_count++;
    }
    fwrite( &header, sizeof(header), 1, f );
    for( i=0; i<num_modules; i++ ) {
	if( modules[i]->save != NULL ) {
	    fwrite( "BLCK", 4, 1, f );
	    fwrite_string( modules[i]->name, f );
	    modules[i]->save(f);
	}
    }
    fclose( f );
    INFO( "Save state written to %s", filename );
    return 0;
}

