/**
 * $Id$
 * 
 * GDB RDP server stub - SH4 + ARM
 *
 * Copyright (c) 2009 Nathan Keynes.
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <arpa/inet.h>
#include "lxdream.h"
#include "dream.h"
#include "dreamcast.h"
#include "ioutil.h"
#include "cpu.h"
#include "gui.h"

#define DEFAULT_BUFFER_SIZE 1024
#define BUFFER_SIZE_MARGIN 32
#define MAX_BUFFER_SIZE 65536

/* These are just local interpretations - they're not interpreted by GDB
 * in any way shape or form.
 */
#define GDB_ERROR_FORMAT 1 /* Badly formatted command */
#define GDB_ERROR_INVAL  2 /* Invalid data */
#define GDB_ERROR_FAIL   3 /* Command failed */
struct gdb_server {
    cpu_desc_t cpu;
    gboolean mmu;
    int fd;
    const gchar *peer_name;
    char *buf;
    int buf_size;
    int buf_posn;
};

static GList *gdb_server_conn_list = NULL;

void gdb_server_free( gpointer data )
{
    struct gdb_server *server = (struct gdb_server *)data;
    gdb_server_conn_list = g_list_remove(gdb_server_conn_list, server);
    free((char *)server->peer_name);
    free(server->buf);
    free(data);
}

int gdb_checksum( char *data, int length )
{
    int i;
    int result = 0;
    for( i=0; i<length; i++ ) 
        result += data[i];
    result &= 0xFF;
    return result;
}

void gdb_send_frame( struct gdb_server *server, char *data, int length )
{
    char out[length+5];
    snprintf( out, length+5, "$%.*s#%02x", length, data, gdb_checksum(data,length) ); 
    write( server->fd, out, length+4 );
}

/**
 * Send bulk data (ie memory dump) as hex, with optional string prefix.
 * Saves double copying when going through gdb_send_frame.
 */ 
void gdb_send_hex_data( struct gdb_server *server, char *prefix, unsigned char *data, int datalen )
{
   int prefixlen = 0;
   if( prefix != NULL )
       prefixlen = strlen(prefix);
   int totallen = datalen*2 + prefixlen + 4;
   char out[totallen+1];
   char *p = &out[1];
   int i;
   
   out[0] = '$';
   if( prefix != NULL ) {
       p += sprintf( p, "%s", prefix );
   }
   for( i=0; i<datalen; i++ ) {
       p += sprintf( p, "%02x", data[i] );
   }
   *p++ = '#';
   sprintf( p, "%02x", gdb_checksum(out+1, datalen*2 + prefixlen) );
   write( server->fd, out, totallen );
}

/**
 * Parse bulk hex data - buffer should be at least datalen/2 bytes long
 */
size_t gdb_read_hex_data( struct gdb_server *server, unsigned char *buf, char *data, int datalen )
{
    char *p = data;
    for( int i=0; i<datalen/2; i++ ) {
        int v;
        sscanf( p, "%02x", &v );
        buf[i] = v;
        p += 2;
    }    
    return datalen/2;
}

/**
 * Parse bulk binary-encoded data - $, #, 0x7D are encoded as 0x7d, char ^ 0x20. 
 * Buffer should be at least datalen bytes longs.
 */
size_t gdb_read_binary_data( struct gdb_server *server, unsigned char *buf, char *data, int datalen )
{
    unsigned char *q = buf;
    for( int i=0, j=0; i<datalen; i++ ) {
        if( data[i] == 0x7D ) {
            if( i == datalen-1 ) {
                return -1;
            } else {
                *q++ = data[++i] ^ 0x20;
            }
        } else {
            *q++ = data[i];
        }
    }
    return q - buf;
}

void gdb_printf_frame( struct gdb_server *server, char *msg, ... )
{
    va_list va;
    
    va_start(va,msg);
    int len = vsnprintf( NULL, 0, msg, va );
    char buf[len+1];
    vsnprintf( buf, len+1, msg, va);
    va_end(va);
    gdb_send_frame( server, buf, len );
}

int gdb_print_registers( struct gdb_server *server, char *buf, int buflen, int firstreg, int regcount )
{
    int i;
    char *p = buf;
    char *endp = buf + (buflen-8);
    for( i=firstreg; i < firstreg + regcount && p < endp; i++ ) {
        uint8_t *val = server->cpu->get_register(i);
        if( val == NULL ) {
            sprintf( p, "00000000" );
        } else {
            sprintf( p, "%02x%02x%02x%02x", val[0], val[1], val[2], val[3] );
        }
        p += 8;
    }
    
    return i - firstreg;
}

void gdb_set_registers( struct gdb_server *server, char *buf, int firstreg, int regcount )
{
    int i;
    char *p = buf;
    for( i=firstreg; i < firstreg + regcount; i++ ) {
        uint8_t *val = server->cpu->get_register(i);
        unsigned int a,b,c,d;
        if( val != NULL ) {
            sscanf( p, "%02x%02x%02x%02x", &a, &b, &c, &d );
            val[0] = (uint8_t)a;
            val[1] = (uint8_t)b;
            val[2] = (uint8_t)c;
            val[3] = (uint8_t)d;
        }
        p += 8;
    }
}

/**
 * Send a 2-digit error code. There's no actual definition for any of the codes
 * so they're more for our own amusement really.
 */
void gdb_send_error( struct gdb_server *server, int error )
{
    char out[4];
    snprintf( out, 4, "E%02X", (error&0xFF) );
    gdb_send_frame( server, out, 3 );
}

void gdb_server_handle_frame( struct gdb_server *server, int command, char *data, int length )
{
    unsigned int tmp, tmp2, tmp3;
    char buf[512];
    
    switch( command ) {
    case '!': /* Enable extended mode */
        gdb_send_frame( server, "OK", 2 );
        break;
    case '?': /* Get stop reason - always return 5 (TRAP) */
        gdb_send_frame( server, "S05", 3 );
        break;
    case 'c': /* Continue */
        gui_do_later(dreamcast_run);
        break;
    case 'g': /* Read all general registers */
        gdb_print_registers( server, buf, sizeof(buf), 0, server->cpu->num_gpr_regs );
        gdb_send_frame( server, buf, strlen(buf) );
        break;
    case 'G': /* Write all general registers */
        if( length != server->cpu->num_gpr_regs*8 ) {
            gdb_send_error( server, GDB_ERROR_FORMAT );
        } else {
            gdb_set_registers( server, data, 0, server->cpu->num_gpr_regs );
            gdb_send_frame( server, "OK", 2 );
        }
        break;
    case 'H': /* Set thread - only thread 1 is supported here */
        if( length < 2 ) {
            gdb_send_error( server, GDB_ERROR_FORMAT );
        } else {
            int thread;
            sscanf( data+1, "%d", &thread );
            if( thread >= -1 && thread <= 1 ) {
                gdb_send_frame( server, "OK", 2 );
            } else {
                gdb_send_error( server, GDB_ERROR_INVAL );
            }
        }
        break;
    case 'k': /* kill - do nothing */
        gdb_send_frame( server, "", 0 );
        break;
    case 'm': /* Read memory */
        if( sscanf( data, "%x,%x", &tmp, &tmp2 ) != 2 ) {
            gdb_send_error( server, GDB_ERROR_FORMAT );
        } else {
            size_t datalen;
            unsigned char mem[tmp2];
            if( server->mmu ) {
                datalen = server->cpu->read_mem_vma(mem, tmp, tmp2);
            } else {
                datalen = server->cpu->read_mem_phys(mem, tmp, tmp2);
            }
            if( datalen == 0 ) {
                gdb_send_error( server, GDB_ERROR_INVAL );
            } else {
                gdb_send_hex_data( server, NULL, mem, datalen );
            }
        }   
        break;
    case 'M': /* Write memory */
        if( sscanf( data, "%x,%x:%n", &tmp, &tmp2, &tmp3 ) != 2 ||
                length-tmp3 != tmp2*2 ) {
            gdb_send_error( server, GDB_ERROR_FORMAT );
        } else {
            size_t len;
            unsigned char mem[tmp2];
            len = gdb_read_hex_data( server, mem, data+tmp3, length-tmp3 );
            if( len != tmp2 ) {
                gdb_send_error( server, GDB_ERROR_FORMAT );
            } else {
                if( server->mmu ) {
                    len = server->cpu->write_mem_vma(tmp, mem, tmp2);
                } else {
                    len = server->cpu->write_mem_phys(tmp, mem, tmp2);
                }
                if( len != tmp2 ) {
                    gdb_send_error( server, GDB_ERROR_INVAL );
                } else {
                    gdb_send_frame( server, "OK", 2 );
                }
            }
        }
        break;   
    case 'p': /* Read single register */
        if( sscanf( data, "%x", &tmp ) != 1 ) {
            gdb_send_error( server, GDB_ERROR_FORMAT );
        } else if( tmp >= server->cpu->num_gdb_regs ) {
            gdb_send_error( server, GDB_ERROR_INVAL );
        } else {
            gdb_print_registers( server, buf, sizeof(buf), tmp, 1 );
            gdb_send_frame( server, buf, 8 );
        }
        break;
    case 'P': /* Write single register. */
        if( sscanf( data, "%x=%n", &tmp, &tmp2 ) != 1 || 
                length-tmp2 != 8) {
            gdb_send_error( server, GDB_ERROR_FORMAT );
        } else if( tmp >= server->cpu->num_gdb_regs ) {
            gdb_send_error( server, GDB_ERROR_INVAL );
        } else {
            gdb_set_registers( server, data+tmp2, tmp, 1 ); 
            gdb_send_frame( server, "OK", 2 );
        }
        break;
    case 'q': /* Query data */
        if( strcmp( data, "C" ) == 0 ) {
            gdb_send_frame( server, "QC1", 3 );
        } else if( strcmp( data, "fThreadInfo" ) == 0 ) {
            gdb_send_frame( server, "m1", 2 );
        } else if( strcmp( data, "sThreadInfo" ) == 0 ) {
            gdb_send_frame( server, "l", 1 );
        } else if( strncmp( data, "Supported", 9 ) == 0 ) {
            gdb_send_frame( server, "PacketSize=4000", 15 );
        } else if( strcmp( data, "Symbol::" ) == 0 ) {
            gdb_send_frame( server, "OK", 2 );
        } else {
            gdb_send_frame( server, "", 0 );
        }
        break;
    case 's': /* Single-step */
        if( length != 0 ) {
            if( sscanf( data, "%x", &tmp ) != 1 ) {
                gdb_send_error( server, GDB_ERROR_FORMAT );
            } else {
                *server->cpu->pc = tmp;
            }
        }
        server->cpu->step_func();
        gdb_send_frame( server, "S05", 3 );
        break;
    case 'T': /* Thread alive */
        if( sscanf( data, "%x", &tmp ) != 1 ) {
            gdb_send_error( server, GDB_ERROR_FORMAT );
        } else if( tmp != 1 ) {
            gdb_send_error( server, GDB_ERROR_INVAL );
        } else {
            gdb_send_frame( server, "OK", 2 );
        }
        break;
    case 'v': /* Verbose */
        /* Only current one is vCont, which we don't bother supporting
         * at the moment, but don't warn about it either */
        gdb_send_frame( server, "", 0 );
        break;
    case 'X': /* Write memory binary */
        if( sscanf( data, "%x,%x:%n", &tmp, &tmp2, &tmp3 ) != 2 ) {
            gdb_send_error( server, GDB_ERROR_FORMAT );
        } else {
            unsigned char mem[length - tmp3];
            size_t len = gdb_read_binary_data( server, mem, data + tmp3, length-tmp3 );
            if( len != tmp2 ) {
                gdb_send_error( server, GDB_ERROR_FORMAT );
            } else {
                if( server->mmu ) {
                    len = server->cpu->write_mem_vma(tmp, mem, tmp2);
                } else {
                    len = server->cpu->write_mem_phys(tmp, mem, tmp2);
                }
                if( len != tmp2 ) {
                    gdb_send_error( server, GDB_ERROR_INVAL );
                } else {
                    gdb_send_frame( server, "OK", 2 );
                }
            }
        }
        break;
    case 'z': /* Remove Break/watchpoint */
        if( sscanf( data, "%d,%x,%x", &tmp, &tmp2, &tmp3 ) != 3 ) {
            gdb_send_error( server, GDB_ERROR_FORMAT );
        } else {
            if( tmp == 0 || tmp == 1 ) { /* soft break or hard break */
                server->cpu->clear_breakpoint( tmp2, BREAK_KEEP );
                gdb_send_frame( server, "OK", 2 );
            } else {
                gdb_send_frame( server, "", 0 );
            }
        }
        break;
    case 'Z': /* Insert Break/watchpoint */
        if( sscanf( data, "%d,%x,%x", &tmp, &tmp2, &tmp3 ) != 3 ) {
            gdb_send_error( server, GDB_ERROR_FORMAT );
        } else {
            if( tmp == 0 || tmp == 1 ) { /* soft break or hard break */
                server->cpu->set_breakpoint( tmp2, BREAK_KEEP );
                gdb_send_frame( server, "OK", 2 );
            } else {
                gdb_send_frame( server, "", 0 );
            }
        }
        break;
    default:
        /* Command unsupported */
        WARN( "Received unknown GDB command '%c%s'", command, data ); 
        gdb_send_frame( server, "", 0 );
        break;
    }

}

/**
 * Decode out frames from the raw data stream. A frame takes the form of
 *   $<data>#<checksum>
 * where data may not contain a '#' character, and checksum is a simple 
 * 8-bit modulo sum of all bytes in data, encoded as a 2-char hex number.
 * 
 * The only other legal wire forms are 
 *   + 
 * indicating successful reception of the last message (ignored here), and
 *   -
 * indicating failed reception of the last message - need to resend.
 */
void gdb_server_process_buffer( struct gdb_server *server )
{
    int i, frame_start = -1, frame_len = -1;
    for( i=0; i<server->buf_posn; i++ ) {
        if( frame_start == -1 ) {
            if( server->buf[i] == '$' ) {
                frame_start = i;
            } else if( server->buf[i] == '+' ) {
                /* Success */
                continue;
            } else if( server->buf[i] == '-' ) {
                /* Request retransmit */
            } else if( server->buf[i] == '\003' ) { /* Control-C */
                if( dreamcast_is_running() ) {
                    dreamcast_stop();
                }
            } /* Anything else is noise */
        } else if( server->buf[i] == '#' ) {
            frame_len = i - frame_start - 1;
            if( i+2 < server->buf_posn ) {
                int calc_checksum = gdb_checksum( &server->buf[frame_start+1], frame_len );
                int frame_checksum = 0;
                sscanf( &server->buf[i+1], "%02x", &frame_checksum );    

                if( calc_checksum != frame_checksum ) {
                    WARN( "GDB frame checksum failure (expected %02X but was %02X)", 
                            calc_checksum, frame_checksum ); 
                    write( server->fd, "-", 1 );
                } else if( frame_len == 0 ) {
                    /* Empty frame - should never occur as a request */
                    WARN( "Empty GDB frame received" );
                    write( server->fd, "-", 1 );
                } else {
                    /* We have a good frame */
                    write( server->fd, "+", 1 );
                    server->buf[i] = '\0';
                    gdb_server_handle_frame( server, server->buf[frame_start+1], &server->buf[frame_start+2], frame_len-1 );
                }
                i+=2;
                frame_start = -1;
            }
        }
    }
    if( frame_start == -1 ) {
        server->buf_posn = 0; /* Consumed whole buffer */
    } else if( frame_start > 0 ) {
        memmove(&server->buf[0], &server->buf[frame_start], server->buf_posn - frame_start);
        server->buf_posn -= frame_start;
    }
}

gboolean gdb_server_data_callback( int fd, void *data )
{
    struct gdb_server *server = (struct gdb_server *)data;

    size_t len = read( fd, &server->buf[server->buf_posn], server->buf_size - server->buf_posn );
    if( len > 0 ) {
        server->buf_posn += len;
        gdb_server_process_buffer( server );

        /* If we have an oversized packet, extend the buffer */
        if( server->buf_posn > server->buf_size - BUFFER_SIZE_MARGIN &&
                server->buf_size < MAX_BUFFER_SIZE ) {
            server->buf_size <<= 1;
            server->buf = realloc( server->buf, server->buf_size );
            assert( server->buf != NULL );
        }
        return TRUE;
    } else {
        INFO( "GDB disconnected" );
        return FALSE;
    }
}

gboolean gdb_server_connect_callback( int fd, gpointer data )
{
    struct sockaddr_in sin;
    socklen_t sinlen = sizeof(sin);
    struct gdb_server *server = (struct gdb_server *)data;
    int conn_fd = accept( fd, (struct sockaddr *)&sin, &sinlen);
    if( conn_fd != -1 ) {
        struct gdb_server *chan_serv = calloc( sizeof(struct gdb_server), 1 );
        chan_serv->cpu = server->cpu;
        chan_serv->mmu = server->mmu;
        chan_serv->fd = conn_fd;
        chan_serv->peer_name = g_strdup_printf("%s:%d", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port)); 
        chan_serv->buf = malloc(1024);
        chan_serv->buf_size = 1024;
        chan_serv->buf_posn = 0;
        io_register_tcp_listener( conn_fd, gdb_server_data_callback, chan_serv, gdb_server_free );
        gdb_server_conn_list = g_list_append(gdb_server_conn_list, chan_serv);
        INFO( "GDB connected from %s", chan_serv->peer_name );
    }
    return TRUE;
}

void gdb_server_notify_stopped( struct gdb_server *server )
{
    gdb_send_frame( server, "S05", 3 );
}

/**
 * stop handler to generate notifications to all connected clients
 */
void gdb_server_on_stop()
{
    GList *ptr;
    for( ptr = gdb_server_conn_list; ptr != NULL; ptr = ptr->next ) {
        gdb_server_notify_stopped( (struct gdb_server *)ptr->data );
    }
}

static gboolean module_registered = FALSE;
static struct dreamcast_module gdb_server_module = {
        "gdbserver", NULL, NULL, NULL, NULL, gdb_server_on_stop, NULL, NULL };


/**
 * Bind a network port for a GDB remote server for the specified cpu. The
 * port is registered for the system network callback.
 * 
 * @param interface network interface to bind to, or null for the default (all) interface  
 * @param port 
 * @param cpu CPU to make available over the network port.. 
 * @param mmu if TRUE, virtual memory is made available to GDB, otherwise GDB
 *     accesses physical memory.
 * @return TRUE if the server was bound successfully.
 */
gboolean gdb_init_server( const char *interface, int port, cpu_desc_t cpu, gboolean mmu )
{
    if( !module_registered ) {
        dreamcast_register_module( &gdb_server_module );
        module_registered = TRUE;
    }

    int fd = io_create_server_socket( interface, port );
    if( fd == -1 ) {
        return FALSE;
    }

    struct gdb_server *server = calloc( sizeof(struct gdb_server), 1 );
    server->cpu = cpu;
    server->mmu = mmu;
    server->fd = fd;
    gboolean result = io_register_tcp_listener( fd, gdb_server_connect_callback, server, gdb_server_free ) != NULL;
    INFO( "%s GDB server running on port %d", cpu->name, port );
    return result;
}
