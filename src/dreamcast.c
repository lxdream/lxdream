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
#include <unistd.h>
#include "lxdream.h"
#include "lxpaths.h"
#include "dream.h"
#include "mem.h"
#include "dreamcast.h"
#include "asic.h"
#include "syscall.h"
#include "gui.h"
#include "aica/aica.h"
#include "gdrom/ide.h"
#include "maple/maple.h"
#include "pvr2/pvr2.h"
#include "sh4/sh4.h"
#include "sh4/sh4core.h"
#include "vmu/vmulist.h"

/**
 * Current state of the DC virtual machine
 */
typedef enum { STATE_UNINIT=0, STATE_RUNNING, 
               STATE_STOPPING, STATE_STOPPED } dreamcast_state_t;
    
static volatile dreamcast_state_t dreamcast_state = STATE_UNINIT;
static gboolean dreamcast_use_bios = TRUE;
static gboolean dreamcast_has_bios = FALSE;
static gboolean dreamcast_has_flash = FALSE;
static gboolean dreamcast_exit_on_stop = FALSE;
static gchar *dreamcast_program_name = NULL;
static sh4addr_t dreamcast_entry_point = 0xA0000000;
static uint32_t timeslice_length = DEFAULT_TIMESLICE_LENGTH;
static uint64_t run_time_nanosecs = 0;
static unsigned int quick_save_state = -1;

#define MAX_MODULES 32
static int num_modules = 0;
dreamcast_module_t modules[MAX_MODULES];

/**
 * The unknown module is used for logging files without an actual module
 * declaration
 */
struct dreamcast_module unknown_module = { "****", NULL, NULL, NULL, NULL, 
        NULL, NULL, NULL };

extern struct mem_region_fn mem_region_sdram;
extern struct mem_region_fn mem_region_vram32;
extern struct mem_region_fn mem_region_vram64;
extern struct mem_region_fn mem_region_audioram;
extern struct mem_region_fn mem_region_audioscratch;
extern struct mem_region_fn mem_region_flashram;
extern struct mem_region_fn mem_region_bootrom;
extern struct mem_region_fn mem_region_pvr2ta;
extern struct mem_region_fn mem_region_pvr2yuv;
extern struct mem_region_fn mem_region_pvr2vdma1;
extern struct mem_region_fn mem_region_pvr2vdma2;

unsigned char dc_main_ram[16 MB];
unsigned char dc_boot_rom[2 MB];
unsigned char dc_flash_ram[128 KB];

/**
 * This function is responsible for defining how all the pieces of the
 * dreamcast actually fit together. 
 *
 * Note currently the locations of the various MMIO pages are hard coded in
 * the MMIO definitions - they should probably be moved here.
 */
void dreamcast_configure( gboolean use_bootrom )
{
    char *bios_path = lxdream_get_global_config_path_value(CONFIG_BIOS_PATH);
    char *flash_path = lxdream_get_global_config_path_value(CONFIG_FLASH_PATH);

    /* Initialize the event queue first */
    event_init();
    
    /* Register the memory framework */
    dreamcast_register_module( &mem_module );

    /* Setup standard memory map */
    mem_map_region( dc_boot_rom,     0x00000000, 2 MB,   MEM_REGION_BIOS,         &mem_region_bootrom, MEM_FLAG_ROM, 2 MB, 0 );
    mem_map_region( dc_flash_ram,    0x00200000, 128 KB, MEM_REGION_FLASH,        &mem_region_flashram, MEM_FLAG_RAM, 128 KB, 0 );
    mem_map_region( aica_main_ram,   0x00800000, 2 MB,   MEM_REGION_AUDIO,        &mem_region_audioram, MEM_FLAG_RAM, 2 MB, 0 );
    mem_map_region( aica_scratch_ram,0x00703000, 8 KB,   MEM_REGION_AUDIO_SCRATCH,&mem_region_audioscratch, MEM_FLAG_RAM, 8 KB, 0 );
    mem_map_region( NULL,            0x04000000, 8 MB,   MEM_REGION_VIDEO64,      &mem_region_vram64, 0, 8 MB, 0 );
    mem_map_region( pvr2_main_ram,   0x05000000, 8 MB,   MEM_REGION_VIDEO,        &mem_region_vram32, MEM_FLAG_RAM, 8 MB, 0 ); 
    mem_map_region( dc_main_ram,     0x0C000000, 16 MB,  MEM_REGION_MAIN,         &mem_region_sdram, MEM_FLAG_RAM, 0x01000000, 0x0F000000 );
    mem_map_region( NULL,            0x10000000, 8 MB,   MEM_REGION_PVR2TA,       &mem_region_pvr2ta, 0, 0x02000000, 0x12000000 );
    mem_map_region( NULL,            0x10800000, 8 MB,   MEM_REGION_PVR2YUV,      &mem_region_pvr2yuv, 0, 0x02000000, 0x12800000 );
    mem_map_region( NULL,            0x11000000, 16 MB,  MEM_REGION_PVR2VDMA1,    &mem_region_pvr2vdma1, 0, 16 MB, 0 );
    mem_map_region( NULL,            0x13000000, 16 MB,  MEM_REGION_PVR2VDMA2,    &mem_region_pvr2vdma2, 0, 16 MB, 0 );
    
    dreamcast_use_bios = use_bootrom;
    dreamcast_has_bios = dreamcast_load_bios( bios_path );
    if( !dreamcast_has_bios ) {
        dreamcast_load_fakebios();
    }

    if( flash_path != NULL && flash_path[0] != '\0' ) {
        mem_load_block( flash_path, 0x00200000, 0x00020000 );
    }
    dreamcast_has_flash = TRUE;

    /* Load in the rest of the core modules */
    dreamcast_register_module( &sh4_module );
    dreamcast_register_module( &asic_module );
    dreamcast_register_module( &pvr2_module );
    dreamcast_register_module( &aica_module );
    dreamcast_register_module( &maple_module );
    dreamcast_register_module( &ide_module );
    dreamcast_register_module( &eventq_module );

    g_free(bios_path);
    g_free(flash_path);
}

gboolean dreamcast_load_bios( const gchar *filename )
{
    if( dreamcast_use_bios ) {
        dreamcast_has_bios = mem_load_rom( dc_boot_rom, filename, 2 MB, 0x89f2b1a1 );
    } else {
        dreamcast_has_bios = FALSE;
    }
    return dreamcast_has_bios;
}

static const char fakebios[] = {
        0x00, 0xd1, /* movl $+4, r1 */
        0x2b, 0x41, /* jmp @r1 */
        0xa0, 0xff, /* .long 0xffffffa0 */
        0xff, 0xff };
gboolean dreamcast_load_fakebios( )
{
    memset( dc_boot_rom, 0, 2 MB );
    memcpy( dc_boot_rom, fakebios, sizeof(fakebios) );
    syscall_add_hook( 0xA0, bios_boot );
    dreamcast_has_bios = TRUE;
    return TRUE;
}

gboolean dreamcast_load_flash( const gchar *filename )
{
    if( filename != NULL && filename[0] != '\0' ) {
        return mem_load_block( filename, 0x00200000, 0x00020000 ) == 0;
    }
    return FALSE;
}


gboolean dreamcast_config_changed(void *data, struct lxdream_config_group *group, unsigned item,
                                       const gchar *oldval, const gchar *newval)
{
    gchar *tmp;
    switch(item) {
    case CONFIG_BIOS_PATH:
        tmp = get_expanded_path(newval);
        dreamcast_load_bios(tmp);
        g_free(tmp);
        break;
    case CONFIG_FLASH_PATH:
        tmp = get_expanded_path(newval);
        dreamcast_load_flash(tmp);
        g_free(tmp);
        break;
    }
    reset_gui_paths();
    return TRUE;
}

void dreamcast_save_flash()
{
    if( dreamcast_has_flash ) {
        char *file = lxdream_get_global_config_path_value(CONFIG_FLASH_PATH);
        mem_save_block( file, 0x00200000, 0x00020000 );
        g_free(file);
    }
}

/**
 * Constructs a system configuration for the AICA in standalone mode,
 * ie sound chip only.
 */
void dreamcast_configure_aica_only( )
{
    dreamcast_register_module( &mem_module );
    mem_map_region( aica_main_ram, 0x00800000, 2 MB, MEM_REGION_AUDIO, &mem_region_audioram, MEM_FLAG_RAM, 2 MB, 0 );
    mem_map_region( aica_scratch_ram, 0x00703000, 8 KB, MEM_REGION_AUDIO_SCRATCH, &mem_region_audioscratch, MEM_FLAG_RAM, 8 KB, 0 );
    dreamcast_register_module( &aica_module );
    aica_enable();
    dreamcast_state = STATE_STOPPED;
}

void dreamcast_register_module( dreamcast_module_t module ) 
{
    assert( num_modules < MAX_MODULES );
    modules[num_modules++] = module;
    if( module->init != NULL )
        module->init();
}

void dreamcast_set_run_time( uint32_t secs, uint32_t nanosecs )
{
    run_time_nanosecs = (((uint64_t)secs) * 1000000000) + nanosecs;
}

void dreamcast_set_exit_on_stop( gboolean flag )
{
    dreamcast_exit_on_stop = flag;
}

void dreamcast_init( gboolean use_bootrom )
{
    dreamcast_configure( use_bootrom );
    dreamcast_state = STATE_STOPPED;
}

void dreamcast_reset( void )
{
    sh4_core_exit(CORE_EXIT_SYSRESET);
    int i;
    for( i=0; i<num_modules; i++ ) {
        if( modules[i]->reset != NULL )
            modules[i]->reset();
    }
}

void dreamcast_run( void )
{
    int i;
    
    if( !dreamcast_can_run() ) {
        ERROR(_("No program is loaded, and no BIOS is configured (required to boot a CD image). To continue, either load a binary program, or set the path to your BIOS file in the Path Preferences"));
        return;
    }
    
    if( dreamcast_state != STATE_RUNNING ) {
        for( i=0; i<num_modules; i++ ) {
            if( modules[i]->start != NULL )
                modules[i]->start();
        }
    }
    
    if( maple_should_grab() ) {
        gui_set_use_grab(TRUE);
    }

    dreamcast_state = STATE_RUNNING;

    if( run_time_nanosecs != 0 ) {
        while( dreamcast_state == STATE_RUNNING ) {
            uint32_t time_to_run = timeslice_length;
            if( run_time_nanosecs < time_to_run ) {
                time_to_run = (uint32_t)run_time_nanosecs;
            }

            for( i=0; i<num_modules; i++ ) {
                if( modules[i]->run_time_slice != NULL )
                    time_to_run = modules[i]->run_time_slice( time_to_run );
            }

            if( run_time_nanosecs > time_to_run ) {
                run_time_nanosecs -= time_to_run;
            } else {
                run_time_nanosecs = 0; // Finished
                break;
            }
        }
    } else {
        while( dreamcast_state == STATE_RUNNING ) {
            int time_to_run = timeslice_length;
            for( i=0; i<num_modules; i++ ) {
                if( modules[i]->run_time_slice != NULL )
                    time_to_run = modules[i]->run_time_slice( time_to_run );
            }

        }
    }

    gui_set_use_grab(FALSE);
    
    for( i=0; i<num_modules; i++ ) {
        if( modules[i]->stop != NULL )
            modules[i]->stop();
    }
    
    vmulist_save_all();
    dreamcast_state = STATE_STOPPED;

    if( dreamcast_exit_on_stop ) {
        dreamcast_shutdown();
        exit(0);
    }
}

void dreamcast_stop( void )
{
    sh4_core_exit(CORE_EXIT_HALT); // returns only if not inside SH4 core
    if( dreamcast_state == STATE_RUNNING )
        dreamcast_state = STATE_STOPPING;
}

void dreamcast_shutdown()
{
    // Don't do a dreamcast_stop - if we're calling this out of SH4 code,
    // it's a shutdown-and-quit event
    if( dreamcast_state == STATE_RUNNING )
        dreamcast_state = STATE_STOPPING;
    dreamcast_save_flash();
    vmulist_save_all();
#ifdef ENABLE_SH4STATS
    sh4_stats_print(stdout);
#endif
}

void dreamcast_program_loaded( const gchar *name, sh4addr_t entry_point )
{
    if( dreamcast_program_name != NULL ) {
        g_free(dreamcast_program_name);
    }
    dreamcast_program_name = g_strdup(name);
    dreamcast_entry_point = entry_point;
    sh4_set_pc(entry_point);
    sh4_write_sr( sh4_read_sr() & (~SR_BL) ); /* Unmask interrupts */
    bios_install();
    dcload_install();
    gui_update_state();
}

gboolean dreamcast_is_running( void )
{
    return dreamcast_state == STATE_RUNNING;
}

gboolean dreamcast_can_run(void)
{
    return dreamcast_state != STATE_UNINIT;
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
int dreamcast_read_save_state_header( FILE *f, char *error, int errorlen )
{
    struct save_state_header header;
    if( fread( &header, sizeof(header), 1, f ) != 1 ) {
        return 0;
    }
    if( strncmp( header.magic, DREAMCAST_SAVE_MAGIC, 16 ) != 0 ) {
    	if( error != NULL )
    		snprintf( error, errorlen, _("File is not a %s save state"), APP_NAME );
        return 0;
    }
    if( header.version != DREAMCAST_SAVE_VERSION ) {
    	if( error != NULL )
    		snprintf( error, errorlen, _("Unsupported %s save state version"), APP_NAME );
        return 0;
    }
    if( header.module_count > MAX_MODULES ) {
    	if( error != NULL )
    		snprintf( error, errorlen, _("%s save state is corrupted (bad module count)"), APP_NAME );
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

    int module_count = dreamcast_read_save_state_header(f, NULL, 0);
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

gboolean dreamcast_load_state( const gchar *filename )
{
    int i,j;
    int module_count;
    char error[128];
    int have_read[MAX_MODULES];

    FILE *f = fopen( filename, "r" );
    if( f == NULL ) return FALSE;

    module_count = dreamcast_read_save_state_header(f, error, sizeof(error));
    if( module_count <= 0 ) {
    	ERROR( error );
        fclose(f);
        return FALSE;
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
            return FALSE;
        }

        /* Find the matching module by name */
        for( j=0; j<num_modules; j++ ) {
            if( strcmp(modules[j]->name,chunk.name) == 0 ) {
                have_read[j] = 1;
                if( modules[j]->load == NULL ) {
                    ERROR( "%s save state is corrupted (no loader for %s)", APP_NAME, modules[j]->name );
                    fclose(f);
                    return FALSE;
                } else if( modules[j]->load(f) != 0 ) {
                    ERROR( "%s save state is corrupted (%s failed)", APP_NAME, modules[j]->name );
                    fclose(f);
                    return FALSE;
                }
                break;
            }
        }
        if( j == num_modules ) {
            fclose(f);
            ERROR( "%s save state contains unrecognized section", APP_NAME );
            return FALSE;
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
    return TRUE;
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

/********************** Quick save state support ***********************/
/* This section doesn't necessarily belong here, but it probably makes the
 * most sense here next to the regular save/load functions
 */

static gchar *get_quick_state_filename( int state )
{
    gchar *path = lxdream_get_global_config_path_value(CONFIG_SAVE_PATH);
    gchar *str = g_strdup_printf( QUICK_STATE_FILENAME, path, state );
    g_free( path );
    return str;
}


static void dreamcast_quick_state_init()
{
    const char *state = lxdream_get_global_config_value(CONFIG_QUICK_STATE);
    if( state != NULL ) {
        quick_save_state = atoi(state);
        if( quick_save_state > MAX_QUICK_STATE ) {
            quick_save_state = 0;
        }
    } else {
        quick_save_state = 0;
    }
}

void dreamcast_quick_save()
{
    if( quick_save_state == -1 ) 
        dreamcast_quick_state_init();
    gchar *str = get_quick_state_filename(quick_save_state);
    dreamcast_save_state(str);
    g_free(str);
}

void dreamcast_quick_load()
{
    if( quick_save_state == -1 ) 
        dreamcast_quick_state_init();
    gchar *str = get_quick_state_filename(quick_save_state);
    dreamcast_load_state(str);
    g_free(str);
}

unsigned int dreamcast_get_quick_state( )
{
    if( quick_save_state == -1 ) 
        dreamcast_quick_state_init();
    return quick_save_state;
}

void dreamcast_set_quick_state( unsigned int state )
{
    if( state <= MAX_QUICK_STATE && state != quick_save_state ) {
        quick_save_state = state;
        char buf[3];
        sprintf( buf, "%d", quick_save_state );
        lxdream_set_global_config_value(CONFIG_QUICK_STATE, buf);
        lxdream_save_config();
    }
}

gboolean dreamcast_has_quick_state( unsigned int state )
{
    gchar *str = get_quick_state_filename(state);
    int result = access(str, R_OK);
    g_free(str);
    return result == 0 ? TRUE : FALSE;
}

/********************* The Boot ROM address space **********************/
static int32_t FASTCALL ext_bootrom_read_long( sh4addr_t addr )
{
    return *((int32_t *)(dc_boot_rom + (addr&0x001FFFFF)));
}
static int32_t FASTCALL ext_bootrom_read_word( sh4addr_t addr )
{
    return SIGNEXT16(*((int16_t *)(dc_boot_rom + (addr&0x001FFFFF))));
}
static int32_t FASTCALL ext_bootrom_read_byte( sh4addr_t addr )
{
    return SIGNEXT8(*((int16_t *)(dc_boot_rom + (addr&0x001FFFFF))));
}
static void FASTCALL ext_bootrom_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memcpy( dest, dc_boot_rom +(addr&0x001FFFFF), 32 );
}

struct mem_region_fn mem_region_bootrom = { 
        ext_bootrom_read_long, unmapped_write_long, 
        ext_bootrom_read_word, unmapped_write_long, 
        ext_bootrom_read_byte, unmapped_write_long, 
        ext_bootrom_read_burst, unmapped_write_burst }; 

/********************* The Flash RAM address space **********************/
static int32_t FASTCALL ext_flashram_read_long( sh4addr_t addr )
{
    return *((int32_t *)(dc_flash_ram + (addr&0x0001FFFF)));
}
static int32_t FASTCALL ext_flashram_read_word( sh4addr_t addr )
{
    return SIGNEXT16(*((int16_t *)(dc_flash_ram + (addr&0x0001FFFF))));
}
static int32_t FASTCALL ext_flashram_read_byte( sh4addr_t addr )
{
    return SIGNEXT8(*((int16_t *)(dc_flash_ram + (addr&0x0001FFFF))));
}
static void FASTCALL ext_flashram_write_long( sh4addr_t addr, uint32_t val )
{
    *(uint32_t *)(dc_flash_ram + (addr&0x0001FFFF)) = val;
    asic_g2_write_word();
}
static void FASTCALL ext_flashram_write_word( sh4addr_t addr, uint32_t val )
{
    *(uint16_t *)(dc_flash_ram + (addr&0x0001FFFF)) = (uint16_t)val;
    asic_g2_write_word();
}
static void FASTCALL ext_flashram_write_byte( sh4addr_t addr, uint32_t val )
{
    *(uint8_t *)(dc_flash_ram + (addr&0x0001FFFF)) = (uint8_t)val;
    asic_g2_write_word();
}
static void FASTCALL ext_flashram_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memcpy( dest, dc_flash_ram+(addr&0x0001FFFF), 32 );
}
static void FASTCALL ext_flashram_write_burst( sh4addr_t addr, unsigned char *src )
{
    memcpy( dc_flash_ram+(addr&0x0001FFFF), src, 32 );
}

struct mem_region_fn mem_region_flashram = { ext_flashram_read_long, ext_flashram_write_long, 
        ext_flashram_read_word, ext_flashram_write_word, 
        ext_flashram_read_byte, ext_flashram_write_byte, 
        ext_flashram_read_burst, ext_flashram_write_burst }; 

