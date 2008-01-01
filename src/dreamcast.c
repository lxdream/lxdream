/**
 * $Id$
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
#include <glib.h>
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
typedef enum { STATE_UNINIT=0, STATE_RUNNING, 
	       STATE_STOPPING, STATE_STOPPED } dreamcast_state_t;

static volatile dreamcast_state_t dreamcast_state = STATE_UNINIT;
static gboolean dreamcast_has_bios = FALSE;
static gchar *dreamcast_program_name = NULL;
static sh4addr_t dreamcast_entry_point = 0xA0000000;
static uint32_t timeslice_length = DEFAULT_TIMESLICE_LENGTH;

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
    dreamcast_has_bios = mem_load_rom( lxdream_get_config_value(CONFIG_BIOS_PATH),
				       0x00000000, 0x00200000, 0x89f2b1a1,
				       MEM_REGION_BIOS );
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
    dreamcast_has_bios = mem_load_rom( lxdream_get_config_value(CONFIG_BIOS_PATH),
				       0x00000000, 0x00200000, 0x89f2b1a1, 
				       MEM_REGION_BIOS );
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

void dreamcast_program_loaded( const gchar *name, sh4addr_t entry_point )
{
    if( dreamcast_program_name != NULL ) {
        g_free(dreamcast_program_name);
    }
    dreamcast_program_name = g_strdup(name);
    dreamcast_entry_point = entry_point;
    sh4_set_pc(entry_point);
    if( !dreamcast_has_bios ) {
        bios_install();
    }
    dcload_install();
    gui_update_state();
}

gboolean dreamcast_is_running( void )
{
    return dreamcast_state == STATE_RUNNING;
}

gboolean dreamcast_can_run(void)
{
    return dreamcast_state != STATE_UNINIT &&
      (dreamcast_has_bios || dreamcast_program_name != NULL);
}

/********************************* Save States *****************************/

struct save_state_header {
    char magic[16];
    uint32_t version;
    uint32_t module_count;
};

struct chunk_header {
    char marker[4]; /* Always BLCK */
    char name[8]; /* Block (module) name */
    uint32_t block_length;
};

/**
 * Check the save state header to ensure that it is a valid, supported
 * file. 
 * @return the number of blocks following, or 0 if the file is invalid.
 */
int dreamcast_read_save_state_header( FILE *f )
{
    struct save_state_header header;
    if( fread( &header, sizeof(header), 1, f ) != 1 ) {
	return 0;
    }
    if( strncmp( header.magic, DREAMCAST_SAVE_MAGIC, 16 ) != 0 ) {
	ERROR( "Not a %s save state file", APP_NAME );
	return 0;
    }
    if( header.version != DREAMCAST_SAVE_VERSION ) {
	ERROR( "%s save state version not supported", APP_NAME );
	return 0;
    }
    if( header.module_count > MAX_MODULES ) {
	ERROR( "%s save state is corrupted (bad module count)", APP_NAME );
	return 0;
    }
    return header.module_count;
}

int dreamcast_write_chunk_header( const gchar *name, uint32_t length, FILE *f )
{
    struct chunk_header head;

    memcpy( head.marker, "BLCK", 4 );
    memset( head.name, 0, 8 );
    memcpy( head.name, name, strlen(name) );
    head.block_length = length;
    return fwrite( &head, sizeof(head), 1, f );
}


frame_buffer_t dreamcast_load_preview( const gchar *filename )
{
    int i;
    FILE *f = fopen( filename, "r" );
    if( f == NULL ) return NULL;
    
    int module_count = dreamcast_read_save_state_header(f);
    if( module_count <= 0 ) {
	fclose(f);
	return NULL;
    }
    for( i=0; i<module_count; i++ ) {
	struct chunk_header head;
	if( fread( &head, sizeof(head), 1, f ) != 1 ) {
	    fclose(f);
	    return NULL;
	}
	if( memcmp("BLCK", head.marker, 4) != 0 ) {
	    fclose(f);
	    return NULL;
	}

	if( strcmp("PVR2", head.name) == 0 ) {
	    uint32_t buf_count;
	    int has_front;
	    fread( &buf_count, sizeof(buf_count), 1, f );
	    fread( &has_front, sizeof(has_front), 1, f );
	    if( buf_count != 0 && has_front ) {
		frame_buffer_t result = read_png_from_stream(f);
		fclose(f);
		return result;
	    }
	    break;
	} else {
	    fseek( f, head.block_length, SEEK_CUR );
	}
    }
    return NULL;
}

int dreamcast_load_state( const gchar *filename )
{
    int i,j;
    int module_count;
    int have_read[MAX_MODULES];

    FILE *f = fopen( filename, "r" );
    if( f == NULL ) return errno;

    module_count = dreamcast_read_save_state_header(f);
    if( module_count <= 0 ) {
	fclose(f);
	return 1;
    }

    for( i=0; i<MAX_MODULES; i++ ) {
	have_read[i] = 0;
    }

    for( i=0; i<module_count; i++ ) {
	struct chunk_header chunk;
	fread( &chunk, sizeof(chunk), 1, f );
	if( strncmp(chunk.marker, "BLCK", 4) != 0 ) {
	    ERROR( "%s save state is corrupted (missing block header %d)", APP_NAME, i );
	    fclose(f);
	    return 2;
	}
	
	/* Find the matching module by name */
	for( j=0; j<num_modules; j++ ) {
	    if( strcmp(modules[j]->name,chunk.name) == 0 ) {
		have_read[j] = 1;
		if( modules[j]->load == NULL ) {
		    ERROR( "%s save state is corrupted (no loader for %s)", APP_NAME, modules[j]->name );
		    fclose(f);
		    return 2;
		} else if( modules[j]->load(f) != 0 ) {
		    ERROR( "%s save state is corrupted (%s failed)", APP_NAME, modules[j]->name );
		    fclose(f);
		    return 2;
		}
		break;
	    }
	}
	if( j == num_modules ) {
	    fclose(f);
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
	    uint32_t blocklen, posn1, posn2;
	    dreamcast_write_chunk_header( modules[i]->name, 0, f );
	    posn1 = ftell(f);
	    modules[i]->save(f);
	    posn2 = ftell(f);
	    blocklen = posn2 - posn1;
	    fseek( f, posn1-4, SEEK_SET );
	    fwrite( &blocklen, sizeof(blocklen), 1, f );
	    fseek( f, posn2, SEEK_SET );
	}
    }
    fclose( f );
    INFO( "Save state written to %s", filename );
    return 0;
}

