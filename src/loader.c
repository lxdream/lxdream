/*
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include "gui.h"
#include "ipbin.h"
#include "sh4core.h"
#include "mem.h"

char ip_bin_magic[32] = "SEGA SEGAKATANA SEGA ENTERPRISES";
char iso_magic[6] = "\001CD001";

#define CDI_V2 0x80000004
#define CDI_V3 0x80000005

void open_file( char *filename )
{
    char buf[32];
    uint32_t tmpa[2];
    struct stat st;
    
    int fd = open( filename, O_RDONLY );
    if( fd == -1 ) {
        ERROR( "Unable to open file: '%s' (%s)", filename,
               strerror(errno) );
        return;
    }

    fstat( fd, &st );
    if( st.st_size < 32768 ) {
        ERROR( "File '%s' too small to be a dreamcast image", filename );
        close(fd);
        return;
    }
    
    /* begin magic */
    if( read( fd, buf, 32 ) != 32 ) {
        ERROR( "Unable to read from file '%s'", filename );
        close(fd);
        return;
    }
    if( memcmp( buf, ip_bin_magic, 32 ) == 0 ) {
        /* we have a DC bootstrap */
        if( st.st_size == BOOTSTRAP_SIZE ) {
            char *load = mem_get_region( BOOTSTRAP_LOAD_ADDR );
            /* Just the bootstrap, no image... */
            WARN( "File '%s' contains bootstrap only, loading anyway",
                  filename );
            lseek( fd, 0, SEEK_SET );
            read( fd, load, BOOTSTRAP_SIZE );
            parse_ipbin( load );
            sh4_reset();
            sh4_set_pc( BOOTSTRAP_LOAD_ADDR + 0x300 );
            set_disassembly_region( BOOTSTRAP_LOAD_ADDR );
            set_disassembly_pc( sh4r.pc, TRUE );
            update_registers();
        } else {
            /* look for a valid ISO9660 header */
            lseek( fd, 32768, SEEK_SET );
            read( fd, buf, 8 );
            if( memcmp( buf, iso_magic, 6 ) == 0 ) {
                /* Alright, got it */
                INFO( "Loading ISO9660 filesystem from '%s'",
                      filename );
            }
        }
    } else {
        /* check if it's a CDI file: */
        lseek( fd, -8, SEEK_END );
        read( fd, &tmpa, 8 );
        if( tmpa[0] == CDI_V2 || tmpa[0] == CDI_V3 ) {
            /* Yup, it is */
            INFO( "Loading CDI file '%s'", filename );
        } else {
            ERROR( "Don't know what to do with '%s'", filename );
        }
    }
    close(fd);
}
