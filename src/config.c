/**
 * $Id$
 *
 * User configuration support
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

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gmem.h>
#include <glib/gstrfuncs.h>
#include "dream.h"
#include "config.h"
#include "maple/maple.h"


extern struct lxdream_config_entry alsa_config[];

gboolean lxdream_load_config_file( const gchar *filename );
gboolean lxdream_save_config_file( const gchar *filename );
gboolean lxdream_load_config_stream( FILE *f );
gboolean lxdream_save_config_stream( FILE *f );

static struct lxdream_config_entry global_config[] =
       {{ "bios", N_("Bios ROM"), CONFIG_TYPE_FILE, "dcboot.rom" },
        { "flash", N_("Flash ROM"), CONFIG_TYPE_FILE, "dcflash.rom" },
        { "default path", N_("Default disc path"), CONFIG_TYPE_PATH, "." },
        { "save path", N_("Save-state path"), CONFIG_TYPE_PATH, "save" },
        { "bootstrap", N_("Bootstrap IP.BIN"), CONFIG_TYPE_FILE, "IP.BIN" },
        { "gdrom", NULL, CONFIG_TYPE_FILE, NULL },
        { "recent", NULL, CONFIG_TYPE_FILE, NULL },
        { NULL, CONFIG_TYPE_NONE }};

static struct lxdream_config_entry serial_config[] =
       {{ "device", N_("Serial device"), CONFIG_TYPE_FILE, "/dev/ttyS1" },
        { NULL, CONFIG_TYPE_NONE }};

struct lxdream_config_group lxdream_config_root[] = 
       {{ "global", global_config },
        { "controllers", NULL },
        { "serial", serial_config },
#ifdef HAVE_ALSA
        { "alsa", alsa_config },
#endif
        { NULL, CONFIG_TYPE_NONE }};

static gchar *lxdream_config_load_filename = NULL;
static gchar *lxdream_config_save_filename = NULL;

gboolean lxdream_find_config()
{
    gboolean result = TRUE;
    char *home = getenv("HOME");
    if( lxdream_config_save_filename == NULL ) {
        lxdream_config_save_filename = g_strdup_printf("%s/.%s", home, DEFAULT_CONFIG_FILENAME);
    }
    if( lxdream_config_load_filename == NULL ) {
        char *sysconfig = g_strdup_printf("%s/%s", get_sysconf_path(), DEFAULT_CONFIG_FILENAME);
        if( access(lxdream_config_save_filename, R_OK) == 0 ) {
            lxdream_config_load_filename = g_strdup(lxdream_config_save_filename);
            g_free(sysconfig);
        } else if( access( sysconfig, R_OK ) == 0 ) {
            lxdream_config_load_filename = sysconfig;
        } else if( access( "./" DEFAULT_CONFIG_FILENAME, R_OK ) == 0 ) {
            lxdream_config_load_filename = g_strdup("./" DEFAULT_CONFIG_FILENAME);
            g_free(sysconfig);
        } else {
            lxdream_config_load_filename = g_strdup(lxdream_config_save_filename);
            g_free(sysconfig);
            result = FALSE;
        }	
    }
    return result;
}

void lxdream_set_config_filename( const gchar *filename )
{
    if( lxdream_config_load_filename != NULL ) {
        g_free(lxdream_config_load_filename);
    }
    lxdream_config_load_filename = g_strdup(filename);
    if( lxdream_config_save_filename != NULL ) {
        g_free(lxdream_config_save_filename);
    }
    lxdream_config_save_filename = g_strdup(filename);
}

void lxdream_set_default_config( )
{
    struct lxdream_config_group *group = lxdream_config_root;
    while( group->key != NULL ) {
        struct lxdream_config_entry *param = group->params;
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

const gchar *lxdream_get_config_value( int key )
{
    return global_config[key].value;
}

void lxdream_set_config_value( lxdream_config_entry_t param, const gchar *value )
{
    if( param->value != value ) {
        if( param->value != param->default_value && param->value != NULL ) {
            free( param->value );
        }
        param->value = g_strdup(value);
    }
}

void lxdream_set_global_config_value( int key, const gchar *value )
{
    lxdream_set_config_value(&global_config[key], value);
}

const struct lxdream_config_entry * lxdream_get_config_entry( int key )
{
    return &global_config[key];
}

gboolean lxdream_set_group_value( lxdream_config_group_t group, const gchar *key, const gchar *value )
{
    int i;
    for( i=0; group->params[i].key != NULL; i++ ) {
        if( strcasecmp( group->params[i].key, key ) == 0 ) {
            lxdream_set_config_value( &group->params[i], value );
            return TRUE;
        }
    }
    return FALSE;
}

void lxdream_copy_config_list( lxdream_config_entry_t dest, lxdream_config_entry_t src )
{
    int i;
    for( i=0; src[i].key != NULL; i++ ) {
        lxdream_set_config_value( &dest[i], src[i].value );
    }
}

gboolean lxdream_load_config( )
{
    if( lxdream_config_load_filename == NULL ) {
        lxdream_find_config();
    }
    return lxdream_load_config_file(lxdream_config_load_filename);
}

gboolean lxdream_save_config( )
{
    if( lxdream_config_save_filename == NULL ) {
        lxdream_find_config();
    }
    return lxdream_save_config_file(lxdream_config_save_filename);
}

gboolean lxdream_load_config_file( const gchar *filename )
{
    FILE *f;
    gboolean result;

    if( access(filename, F_OK) != 0 ) {
        INFO( "Configuration file '%s' does not exist, creating from defaults" );
        lxdream_set_default_config();
        lxdream_save_config();
    }

    f = fopen(filename, "ro");
    if( f == NULL ) {
        ERROR( "Unable to open configuration file '%s': %s", filename, strerror(errno) );
        lxdream_set_default_config();
        return FALSE;
    }

    result = lxdream_load_config_stream( f );
    fclose(f);
    return result;
}

gboolean lxdream_load_config_stream( FILE *f )
{

    char buf[512];
    int maple_device = -1, maple_subdevice = -1;
    struct lxdream_config_group devgroup;
    struct lxdream_config_group *group = NULL;
    maple_device_t device = NULL;
    lxdream_set_default_config();

    while( fgets( buf, sizeof(buf), f ) != NULL ) {
        g_strstrip(buf);
        if( buf[0] == '#' )
            continue;
        if( *buf == '[' ) {
            char *p = strchr(buf, ']');
            if( p != NULL ) {
                struct lxdream_config_group *tmp_group;
                maple_device = maple_subdevice = -1;
                *p = '\0';
                g_strstrip(buf+1);
                tmp_group = &lxdream_config_root[0];
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
                struct lxdream_config_entry *param = group->params;
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

gboolean lxdream_save_config_file( const gchar *filename )
{
    FILE *f = fopen(filename, "wo");
    gboolean result;
    if( f == NULL ) {
        ERROR( "Unable to open '%s': %s", filename, strerror(errno) );
        return FALSE;
    }
    result = lxdream_save_config_stream(f);
    fclose(f);
    return TRUE;
}    

gboolean lxdream_save_config_stream( FILE *f )
{
    struct lxdream_config_group *group = &lxdream_config_root[0];

    while( group->key != NULL ) {
        struct lxdream_config_entry *entry = group->params;
        fprintf( f, "[%s]\n", group->key );

        if( entry != NULL ) {
            while( entry->key != NULL ) {
                if( entry->value != NULL ) {
                    fprintf( f, "%s = %s\n", entry->key, entry->value );
                }
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
                        if( dev->get_config != NULL && ((entry = dev->get_config(dev)) != NULL) ) {
                            while( entry->key != NULL ) {
                                if( entry->value != NULL ) {
                                    fprintf( f, "%*c%s = %s\n", j==0?4:8, ' ',entry->key, entry->value );
                                }
                                entry++;
                            }
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
