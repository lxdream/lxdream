
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "cdi.h"

#define CDI_V2 0x80000004
#define CDI_V3 0x80000005

static char track_start_marker[20] = { 0,0,1,0,0,0,255,255,255,255,
                                       0,0,1,0,0,0,255,255,255,255 };

struct cdi_trailer {
    uint32_t cdi_version;
    uint32_t header_offset;
};

struct cdi_track {
    int file_posn;
    int length; /* bytes */
    int pregap; /* sectors */
    int mode;
    int sector_size;
    int session;
    struct cdi_track *next;
};

struct cdi_handle {
    int fd;
    uint16_t num_sessions;
    struct cdi_track *tracks;
};



struct cdi_track_data {
    char unknown[0x19];
    uint32_t pregap_length;
    uint32_t length;
    char unknown2[6];
    uint32_t mode;
    char unknown3[0x0c];
    uint32_t start_lba;
    uint32_t total_length;
    char unknown4[0x10];
    uint32_t sector_size;
    char unknown5[0x1D];
} __attribute__((packed));

cdi_t cdi_open( char *filename )
{
#define BAIL( x, ... ) { fprintf( stderr, x, __VA_ARGS__ ); if( fd != -1 ) close(fd); return NULL; }

    struct stat st;
    int fd = -1, i,j, tmp;
    int posn = 0, hdr;
    long len;
    struct cdi_trailer trail;
    struct cdi_handle cdi;
    uint16_t tracks;
    uint32_t new_fmt;
    char tmpc;
    char marker[20];

    fd = open( filename, O_RDONLY );
    if( fd == -1 )
        BAIL( "Unable to open file: %s (%s)\n", filename, strerror(errno) );
    fstat( fd, &st );
    if( st.st_size < 8 )
        BAIL( "File is too small to be a valid CDI image: %s\n", filename );
    len = lseek( fd, -8, SEEK_END );
    read( fd, &trail, sizeof(trail) );
    if( trail.header_offset > st.st_size ||
        trail.header_offset == 0 )
        BAIL( "Not a valid CDI image: %s\n", filename );

    if( trail.cdi_version == CDI_V2 ) trail.cdi_version = 2;
    else if( trail.cdi_version == CDI_V3 ) trail.cdi_version = 3;
    else BAIL( "Unknown CDI version code %08x in %s\n", trail.cdi_version,
               filename );

    lseek( fd, trail.header_offset, SEEK_SET );
    read( fd, &cdi.num_sessions, 2 );
    

    printf( "CDI version: %d\n", trail.cdi_version );
    printf( "Sessions: %d\n\n", cdi.num_sessions );
    for( i=0; i< cdi.num_sessions; i++ ) {        
        read( fd, &tracks, 2 );
        printf( "Session %d - %d tracks:\n", i+1, tracks );
        for( j=0; j<tracks; j++ ) {
            struct cdi_track_data trk;
            
            read( fd, &new_fmt, 4 );
            if( new_fmt != 0 ) { /* Additional data 3.00.780+ ?? */
                printf( "Note: CDI image has 3.00.780+ flag set\n" );
                lseek( fd, 8, SEEK_CUR );
            }
            read( fd, marker, 20 );
            if( memcmp( marker, track_start_marker, 20) != 0 )
                BAIL( "Track start marker not found, error reading cdi\n",NULL );
            read( fd, &tmp, 4 );
            printf( "Unknown 4 bytes: %08x\n", tmp );
            read( fd, &tmpc, 1 );
            lseek( fd, (int)tmpc, SEEK_CUR ); /* skip over the filename */
            read( fd, &trk, sizeof(trk) );
            switch( trk.sector_size ) {
                case 0: trk.sector_size = 2048; hdr=0; break;
                case 1: trk.sector_size = 2336; hdr=8; break;
                case 2:
                    trk.sector_size = 2352;
                    if( trk.mode == 2 ) hdr=24;
                    else hdr=16;
                    break;
                default: BAIL( "Unknown sector size: %d\n", trk.sector_size );
            }
            posn += hdr;
            len = trk.length*trk.sector_size;
            printf( "  Track %d\n", j+1 );
            printf( "    Pregap: %08x\n", trk.pregap_length );
            printf( "    Length: %08x\n", trk.length );
            printf( "    Mode: %d\n", trk.mode );
            printf( "    Sector size: %d\n", trk.sector_size );
            printf( "    Start LBA: %08x\n", trk.start_lba );
            printf( "    Total length: %08x\n", trk.total_length );
            printf( "   ---\n" );
            printf( "    File position: %08x\n", posn+trk.pregap_length*trk.sector_size );
            printf( "    File length: %d\n", len );
            posn += trk.total_length*trk.sector_size;
            lseek( fd, 12, SEEK_CUR );
            if( new_fmt )
                lseek( fd, 78, SEEK_CUR );
            tmp = lseek( fd, 0, SEEK_CUR );
            printf( "(Header offset: %x)\n", tmp - trail.header_offset );
        }
    }
    
    return NULL;
#undef BAIL(x, ...)
}

int main(int argc, char *argv[] )
{
    int i;

    for( i=1; i<argc; i++ ) {
        cdi_open(argv[i]);
    }
    return 0;
}
    
