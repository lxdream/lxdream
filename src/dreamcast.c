/**
 * $Id: dreamcast.c,v 1.10 2005-12-25 01:28:36 nkeynes Exp $
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
#include "mem.h"
#include "aica/aica.h"
#include "asic.h"
#include "dreamcast.h"
#include "gdrom/ide.h"
#include "maple/maple.h"
#include "modules.h"

/**
 * Current state of the DC virtual machine
 */
#define STATE_UNINIT 0
#define STATE_RUNNING 1
#define STATE_STOPPING 2
#define STATE_STOPPED 3 
static volatile int dreamcast_state = STATE_UNINIT;
static uint32_t timeslice_length = DEFAULT_TIMESLICE_LENGTH;
static char *dreamcast_config = "DEFAULT";

#define MAX_MODULES 32
static int num_modules = 0;
dreamcast_module_t modules[MAX_MODULES];

/**
 * This function is responsible for defining how all the pieces of the
 * dreamcast actually fit together. 
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

void dreamcast_stop( void )
{
    if( dreamcast_state == STATE_RUNNING )
	dreamcast_state = STATE_STOPPING;
}

gboolean dreamcast_is_running( void )
{
    return dreamcast_state == STATE_RUNNING;
}


/********************************* Save States *****************************/

#define DREAMCAST_SAVE_MAGIC "%!-DreamOn!Save\0"
#define DREAMCAST_SAVE_VERSION 0x00010000

struct save_state_header {
    char magic[16];
    uint32_t version;
    uint32_t module_count;
};

int dreamcast_load_state( const gchar *filename )
{
    int i,j;
    uint32_t count, len;
    int have_read[MAX_MODULES];
    char tmp[64];
    struct save_state_header header;
    FILE *f;

    f = fopen( filename, "r" );
    if( f == NULL ) return errno;

    fread( &header, sizeof(header), 1, f );
    if( strncmp( header.magic, DREAMCAST_SAVE_MAGIC, 16 ) != 0 ) {
	ERROR( "Not a DreamOn save state file" );
	return 1;
    }
    if( header.version != DREAMCAST_SAVE_VERSION ) {
	ERROR( "DreamOn save state version not supported" );
	return 1;
    }
    if( header.module_count > MAX_MODULES ) {
	ERROR( "DreamOn save state is corrupted (bad module count)" );
	return 1;
    }
    for( i=0; i<MAX_MODULES; i++ ) {
	have_read[i] = 0;
    }

    for( i=0; i<header.module_count; i++ ) {
	fread(tmp, 4, 1, f );
	if( strncmp(tmp, "BLCK", 4) != 0 ) {
	    ERROR( "DreamOn save state is corrupted (missing block header %d)", i );
	    return 2;
	}
	len = fread_string(tmp, sizeof(tmp), f );
	if( len > 64 || len < 1 ) {
	    ERROR( "DreamOn save state is corrupted (bad string)" );
	    return 2;
	}
	
	/* Find the matching module by name */
	for( j=0; j<num_modules; j++ ) {
	    if( strcmp(modules[j]->name,tmp) == 0 ) {
		have_read[j] = 1;
		if( modules[j]->load == NULL ) {
		    ERROR( "DreamOn save state is corrupted (no loader for %s)", modules[j]->name );
		    return 2;
		} else if( modules[j]->load(f) != 0 ) {
		    ERROR( "DreamOn save state is corrupted (%s failed)", modules[j]->name );
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
    fclose(f);
    INFO( "Save state read from %s", filename );
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
}

