/**
 * $Id: dreamcast.c,v 1.20 2007-01-12 10:16:02 nkeynes Exp $
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
#include <glib/gstrfuncs.h>
#include "dream.h"
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
static char *dreamcast_config = "DEFAULT";

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
    if( mem_load_rom( dreamcast_get_config_value(CONFIG_BIOS_PATH),
		      0x00000000, 0x00200000, 0x89f2b1a1 ) == NULL ) {
	/* Bios wasn't found. Dump an empty ram region in there for something to do */
	mem_create_ram_region( 0x00000000, 0x00200000, MEM_REGION_BIOS );
    }
    mem_create_ram_region( 0x00200000, 0x00020000, MEM_REGION_FLASH );
    mem_load_block( dreamcast_get_config_value(CONFIG_FLASH_PATH),
		    0x00200000, 0x00020000 );

    /* Load in the rest of the core modules */
    dreamcast_register_module( &sh4_module );
    dreamcast_register_module( &asic_module );
    dreamcast_register_module( &pvr2_module );
    dreamcast_register_module( &aica_module );
    dreamcast_register_module( &maple_module );
    dreamcast_register_module( &ide_module );
}

void dreamcast_save_flash()
{
    char *file = dreamcast_get_config_value(CONFIG_FLASH_PATH);
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

void dreamcast_stop( void )
{
    if( dreamcast_state == STATE_RUNNING )
	dreamcast_state = STATE_STOPPING;
}

void dreamcast_shutdown()
{
    dreamcast_stop();
    sh4_stop();
    dreamcast_save_flash();
}

gboolean dreamcast_is_running( void )
{
    return dreamcast_state == STATE_RUNNING;
}

/***************************** User Configuration **************************/


static struct dreamcast_config_entry global_config[] =
    {{ "bios", CONFIG_TYPE_FILE, "dcboot.rom" },
     { "flash", CONFIG_TYPE_FILE, "dcflash.rom" },
     { "default path", CONFIG_TYPE_PATH, "." },
     { "save path", CONFIG_TYPE_PATH, "save" },
     { "bootstrap", CONFIG_TYPE_FILE, "IP.BIN" },
     { NULL, CONFIG_TYPE_NONE }};

static struct dreamcast_config_entry serial_config[] =
    {{ "device", CONFIG_TYPE_FILE, "/dev/ttyS1" },
     { NULL, CONFIG_TYPE_NONE }};

struct dreamcast_config_group dreamcast_config_root[] = 
    {{ "global", global_config },
     { "controllers", NULL },
     { "serial", serial_config },
     { NULL, CONFIG_TYPE_NONE }};

void dreamcast_set_default_config( )
{
    struct dreamcast_config_group *group = dreamcast_config_root;
    while( group->key != NULL ) {
	struct dreamcast_config_entry *param = group->params;
	if( param != NULL ) {
	    while( param->key != NULL ) {
		if( param->value != param->default_value ) {
		    if( param->value != NULL )
			free( param->value );
		    param->value = (gchar *)param->default_value;
		}
		param++;
	    }
	}
	group++;
    }
    maple_detach_all();
}

const gchar *dreamcast_get_config_value( int key )
{
    return global_config[key].value;
}

gboolean dreamcast_load_config( const gchar *filename )
{
    FILE *f = fopen(filename, "ro");
    gboolean result;

    if( f == NULL ) {
	ERROR( "Unable to open '%s': %s", filename, strerror(errno) );
	return FALSE;
    }

    result = dreamcast_load_config_stream( f );
    fclose(f);
    return result;
}

gboolean dreamcast_load_config_stream( FILE *f )
{

    char buf[512], *p;
    int maple_device = -1, maple_subdevice = -1;
    struct dreamcast_config_group devgroup;
    struct dreamcast_config_group *group = NULL;
    maple_device_t device = NULL;
    dreamcast_set_default_config();

    while( fgets( buf, sizeof(buf), f ) != NULL ) {
	g_strstrip(buf);
	if( buf[0] == '#' )
	    continue;
	if( *buf == '[' ) {
	    char *p = strchr(buf, ']');
	    if( p != NULL ) {
		struct dreamcast_config_group *tmp_group;
		maple_device = maple_subdevice = -1;
		*p = '\0';
		g_strstrip(buf+1);
		tmp_group = &dreamcast_config_root[0];
		while( tmp_group->key != NULL ) {
		    if( strcasecmp(tmp_group->key, buf+1) == 0 ) {
			group = tmp_group;
			break;
		    }
		    tmp_group++;
		}
	    }
	} else if( group != NULL ) {
	    char *value = strchr( buf, '=' );
	    if( value != NULL ) {
		struct dreamcast_config_entry *param = group->params;
		*value = '\0';
		value++;
		g_strstrip(buf);
		g_strstrip(value);
		if( strcmp(group->key,"controllers") == 0  ) {
		    if( g_strncasecmp( buf, "device ", 7 ) == 0 ) {
			maple_device = strtoul( buf+7, NULL, 0 );
			if( maple_device < 0 || maple_device > 3 ) {
			    ERROR( "Device number must be between 0..3 (not '%s')", buf+7);
			    continue;
			}
			maple_subdevice = 0;
			device = maple_new_device( value );
			if( device == NULL ) {
			    ERROR( "Unrecognized device '%s'", value );
			} else {
			    devgroup.key = "controllers";
			    devgroup.params = maple_get_device_config(device);
			    maple_attach_device( device, maple_device, maple_subdevice );
			    group = &devgroup;
			}
			continue;
		    } else if( g_strncasecmp( buf, "subdevice ", 10 ) == 0 ) {
			maple_subdevice = strtoul( buf+10, NULL, 0 );
			if( maple_device == -1 ) {
			    ERROR( "Subdevice not allowed without primary device" );
			} else if( maple_subdevice < 1 || maple_subdevice > 5 ) {
			    ERROR( "Subdevice must be between 1..5 (not '%s')", buf+10 );
			} else if( (device = maple_new_device(value)) == NULL ) {
			    ERROR( "Unrecognized subdevice '%s'", value );
			} else {
			    devgroup.key = "controllers";
			    devgroup.params = maple_get_device_config(device);
			    maple_attach_device( device, maple_device, maple_subdevice );
			    group = &devgroup;
			}
			continue;
		    }
		}
		while( param->key != NULL ) {
		    if( strcasecmp( param->key, buf ) == 0 ) {
			param->value = g_strdup(value);
			break;
		    }
		    param++;
		}
	    }
	}
    }
    return TRUE;
}

gboolean dreamcast_save_config( const gchar *filename )
{
    FILE *f = fopen(filename, "wo");
    gboolean result;
    if( f == NULL ) {
	ERROR( "Unable to open '%s': %s", filename, strerror(errno) );
	return FALSE;
    }
    result = dreamcast_save_config_stream(f);
    fclose(f);
}    

gboolean dreamcast_save_config_stream( FILE *f )
{
    struct dreamcast_config_group *group = &dreamcast_config_root[0];
    
    while( group->key != NULL ) {
	struct dreamcast_config_entry *entry = group->params;
	fprintf( f, "[%s]\n", group->key );
	
	if( entry != NULL ) {
	    while( entry->key != NULL ) {
		fprintf( f, "%s = %s\n", entry->key, entry->value );
		entry++;
	    }
	} else if( strcmp(group->key, "controllers") == 0 ) {
	    int i,j;
	    for( i=0; i<4; i++ ) {
		for( j=0; j<6; j++ ) {
		    maple_device_t dev = maple_get_device( i, j );
		    if( dev != NULL ) {
			if( j == 0 )
			    fprintf( f, "Device %d = %s\n", i, dev->device_class->name );
			else 
			    fprintf( f, "Subdevice %d = %s\n", j, dev->device_class->name );
			entry = dev->get_config(dev);
			while( entry->key != NULL ) {
			    fprintf( f, "%*c%s = %s\n", j==0?4:8, ' ',entry->key, entry->value );
			    entry++;
			}
		    }
		}
	    }
	}
	fprintf( f, "\n" );
	group++;
    }
    return TRUE;
}

/********************************* Save States *****************************/

#define DREAMCAST_SAVE_MAGIC "%!-lxDream!Save\0"
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

