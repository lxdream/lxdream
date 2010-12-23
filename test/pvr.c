/**
 * $Id$
 * 
 * PVR support code
 *
 * Copyright (c) 2006 Nathan Keynes.
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

#include "lib.h"
#include "pvr.h"

#define PVR_BASE 0xA05F8000
#define PVR_RESET    (PVR_BASE+0x008)
#define TA_INIT      (PVR_BASE+0x144)
#define TA_TILESTART (PVR_BASE+0x124)
#define TA_OBJSTART  (PVR_BASE+0x128)
#define TA_TILEEND   (PVR_BASE+0x12C)
#define TA_OBJEND    (PVR_BASE+0x130)
#define TA_TILEPOSN  (PVR_BASE+0x134)
#define TA_OBJPOSN   (PVR_BASE+0x138)
#define TA_SIZE      (PVR_BASE+0x13C)
#define TA_TILECFG   (PVR_BASE+0x140)
#define TA_REINIT    (PVR_BASE+0x160)
#define TA_PLISTSTART (PVR_BASE+0x164)

#define RENDER_START    (PVR_BASE+0x014)
#define RENDER_POLYBASE (PVR_BASE+0x020)
#define RENDER_TILEBASE (PVR_BASE+0x02C)
#define RENDER_MODE     (PVR_BASE+0x048)
#define RENDER_SIZE     (PVR_BASE+0x04C)
#define RENDER_ADDR1    (PVR_BASE+0x060)
#define RENDER_ADDR2    (PVR_BASE+0x064)
#define RENDER_HCLIP    (PVR_BASE+0x068)
#define RENDER_VCLIP    (PVR_BASE+0x06C)
#define RENDER_NEARCLIP (PVR_BASE+0x078)
#define RENDER_FARCLIP  (PVR_BASE+0x088)
#define RENDER_BGPLANE  (PVR_BASE+0x08C)

#define DISPLAY_MODE    (PVR_BASE+0x044)
#define DISPLAY_ADDR1   (PVR_BASE+0x050)
#define DISPLAY_ADDR2   (PVR_BASE+0x054)
#define DISPLAY_SIZE    (PVR_BASE+0x05C

void ta_dump_regs( FILE *f )
{
    fprintf( stderr, "TA Object start[128]: %08X posn[138]: %08X end[130]: %08X\n",
	     long_read(TA_OBJSTART), long_read(TA_OBJPOSN), long_read(TA_OBJEND) );
    fprintf( stderr, "TA OPB start[124]: %08X posn[134]: %08X end[12c]: %08X init: %08X\n",
	     long_read(TA_TILESTART), long_read(TA_TILEPOSN), long_read(TA_TILEEND),
	     long_read(TA_PLISTSTART) );
    fprintf( stderr, "TA Tilesize: %08X  config: %08X\n",  long_read(TA_SIZE), long_read(TA_TILECFG) );
}


void ta_init( struct ta_config *config )
{
    long_write( PVR_RESET, 1 );
    long_write( PVR_RESET, 0 );

    long_write( TA_SIZE, config->grid_size );
    long_write( TA_OBJSTART, config->obj_start & 0x00FFFFFF );
    long_write( TA_OBJEND, config->obj_end & 0x00FFFFFF );
    long_write( TA_TILESTART, config->tile_start & 0x00FFFFFF );
    long_write( TA_TILEEND, config->tile_end & 0x00FFFFFF );
    long_write( TA_PLISTSTART, config->plist_start & 0x00FFFFFF );
    long_write( TA_TILECFG, config->ta_cfg );
    long_write( TA_INIT, 0x80000000 );
}

void ta_reinit( )
{
    long_write( TA_REINIT, 0x80000000 );
}

int pvr_get_objbuf_size( )
{
    return long_read( TA_OBJPOSN ) - long_read( TA_OBJSTART );
}

int pvr_get_objbuf_posn( )
{
    return long_read( TA_OBJPOSN );
}

int pvr_get_plist_posn( )
{
    unsigned int addr = long_read( TA_TILEPOSN ) << 2;
    return addr;
}

void pvr_dump_objbuf( FILE *f )
{
    unsigned int start = long_read( TA_OBJSTART );
    unsigned int posn = long_read( TA_OBJPOSN );
    unsigned int end = long_read( TA_OBJEND );
    char *buf;
    unsigned int length;
    if( start < posn ) {
	buf = (char *)(0xA5000000+start);
	length = posn-start;
    } else {
	buf = (char *)(0xA5000000+end);
	length = start-posn;
    }

    fprintf( f, "Obj buffer: %08X - %08X - %08X\n", start, posn, end );
    fwrite_dump( f, buf, length );
}
	
void pvr_dump_tilebuf( FILE *f )
{
    unsigned int start = long_read( TA_TILESTART );
    unsigned int posn = long_read( TA_TILEPOSN );
    unsigned int end = long_read( TA_TILEEND );
    char *buf;
    unsigned int length;
    if( start < posn ) {
	buf = (char *)(0xA5000000+start);
	length = posn-start;
    } else {
	buf = (char *)(0xA5000000+end);
	length = start-posn;
    }

    fprintf( f, "Tile buffer: %08X - %08X - %08X\n", start, posn, end );
    fwrite_dump( f, buf, length );
}

static int ta_tile_sizes[4] = { 0, 32, 64, 128 };
#define TILE_SIZE(cfg, tile) ta_tile_sizes[((((cfg->ta_cfg) >> (4*tile))&0x03))]
#define TILE_ENABLED(cfg, tile) ((((cfg->ta_cfg) >> (4*tile))&0x03) != 0)
void pvr_compute_tilematrix_addr( int *tile_ptrs, struct ta_config *config ) {
    int tile_sizes[5], i;
    int hsegs = (config->grid_size & 0xFFFF)+1;
    int vsegs = (config->grid_size >> 16) + 1;
    for( i=0; i<5; i++ ) {
	tile_sizes[i] = TILE_SIZE(config,i);
    }
    tile_ptrs[0] = config->tile_start;
    tile_ptrs[1] = tile_ptrs[0] + (hsegs*vsegs*tile_sizes[0]);
    tile_ptrs[2] = tile_ptrs[1] + (hsegs*vsegs*tile_sizes[1]);
    tile_ptrs[3] = tile_ptrs[2] + (hsegs*vsegs*tile_sizes[2]);
    tile_ptrs[4] = tile_ptrs[3] + (hsegs*vsegs*tile_sizes[3]);
}

static uint32_t *pvr_compute_tile_ptrs( uint32_t *target, struct ta_config *config, int x, int y )
{
    int i;
    int cfg = config->ta_cfg;
    int hsegs = (config->grid_size & 0xFFFF)+1;
    int vsegs = (config->grid_size >> 16) + 1;
    int tilematrix = config->tile_start;
    for( i=0; i<5; i++ ) {
	if( cfg & 0x03 ) {
	    int tile_size = ta_tile_sizes[cfg&0x03];
	    *target++ = tilematrix + (((y*hsegs)+x)*tile_size);
	    tilematrix += hsegs*vsegs*tile_size;
	} else {
	    *target++ = 0x80000000;
	}
	cfg = cfg >> 4;
    }
    return target;
}

void pvr_build_tilemap1( uint32_t addr, struct ta_config *config, uint32_t control_word )
{
    uint32_t *dest = (uint32_t *)(PVR_VRAM_BASE+addr);
    int w = (config->grid_size & 0x0000FFFF) + 1;
    int h = (config->grid_size >> 16) + 1;

    int x,y;
    memset( (char *)(dest-18), 0, 18*4 );
    *dest++ = 0x10000000;
    *dest++ = 0x80000000;
    *dest++ = 0x80000000;
    *dest++ = 0x80000000;
    *dest++ = 0x80000000;
    *dest++ = 0x80000000;
    for( x=0; x<w; x++ ) {
	for( y=0; y<h; y++ ) {
	    *dest++ = control_word | (y << 8) | (x << 2);
	    dest = pvr_compute_tile_ptrs(dest, config, x, y);
	}
    }
    dest[-6] |= 0x80000000; /* End-of-render */ 
}

void pvr_build_tilemap2( uint32_t addr, struct ta_config *config, uint32_t control_word )
{
    uint32_t *dest = (uint32_t *)(PVR_VRAM_BASE+addr);
    int w = (config->grid_size & 0x0000FFFF) + 1;
    int h = (config->grid_size >> 16) + 1;

    int x,y;
    *dest++ = 0x10000000;
    *dest++ = 0x80000000;
    *dest++ = 0x80000000;
    *dest++ = 0x80000000;
    *dest++ = 0x80000000;
    *dest++ = 0x80000000;
    for( x=0; x<w; x++ ) {
	for( y=0; y<h; y++ ) {
	    *dest++ = 0x40000000;
	    *dest++ = 0x80000000;
	    *dest++ = 0x80000000;
	    *dest++ = 0x80000000;
	    *dest++ = 0x80000000;
	    *dest++ = 0x80000000;
	    *dest++ = control_word | (y << 8) | (x << 2);
	    dest = pvr_compute_tile_ptrs(dest, config, x, y);
	}
    }
    dest[-6] |= 0x80000000; /* End-of-render */ 
}

void render_set_backplane( uint32_t mode )
{
    long_write( RENDER_BGPLANE, mode );
}

int get_line_size( struct render_config *config )
{
    int modulo = config->width;
    switch( config->mode & 0x07 ) {
    case 4:
	modulo *= 3; /* ??? */
	break;
    case 5:
    case 6:
	modulo *= 4;
	break;
    default:
	modulo *= 2;
    } 
    return modulo;
}

void render_start( struct render_config *config )
{
    int modulo = get_line_size( config );
    long_write( RENDER_POLYBASE, config->polybuf );
    long_write( RENDER_TILEBASE, config->tilemap );
    long_write( RENDER_ADDR1, config->render_addr );
    long_write( RENDER_SIZE, modulo >> 3 ); 
    long_write( RENDER_ADDR2, config->render_addr + modulo ); /* Not used? */
    long_write( RENDER_HCLIP, (config->width - 1) << 16 );
    long_write( RENDER_VCLIP, (config->height - 1) << 16 );
    long_write( RENDER_MODE, config->mode );
    float_write( RENDER_FARCLIP, config->farclip );
    float_write( RENDER_NEARCLIP, config->nearclip );
    long_write( RENDER_START, 0xFFFFFFFF );
}

void display_render( struct render_config *config )
{
    long_write( DISPLAY_ADDR1, config->render_addr );
    long_write( DISPLAY_ADDR2, config->render_addr + get_line_size(config) );
}

/************** Stolen from TATEST *************/

static unsigned int three_d_params[] = {
        0x80a8, 0x15d1c951,     /* M (Unknown magic value) */
        0x80a0, 0x00000020,     /* M */
        0x8008, 0x00000000,     /* TA out of reset */
        0x8048, 0x00000009,     /* alpha config */
        0x8068, 0x02800000,     /* pixel clipping x */
        0x806c, 0x01e00000,     /* pixel clipping y */
        0x8110, 0x00093f39,     /* M */
        0x8098, 0x00800408,     /* M */
        0x804c, 0x000000a0,     /* display align (640*2)/8 */
        0x8078, 0x3f800000,     /* polygon culling (1.0f) */
        0x8084, 0x00000000,     /* M */
        0x8030, 0x00000101,     /* M */
        0x80b0, 0x007f7f7f,     /* Fog table color */
        0x80b4, 0x007f7f7f,     /* Fog vertex color */
        0x80c0, 0x00000000,     /* color clamp min */
        0x80bc, 0xffffffff,     /* color clamp max */
        0x8080, 0x00000007,     /* M */
        0x8074, 0x00000001,     /* cheap shadow */
        0x807c, 0x0027df77,     /* M */
        0x8008, 0x00000001,     /* TA reset */
        0x8008, 0x00000000,     /* TA out of reset */
        0x80e4, 0x00000000,     /* stride width */
        0x6884, 0x00000000,     /* Disable all interrupt events */
        0x6930, 0x00000000,
        0x6938, 0x00000000,
        0x6900, 0xffffffff,     /* Clear all pending int events */
        0x6908, 0xffffffff,
        0x6930, 0x002807ec,     /* Re-enable some events */
        0x6938, 0x0000000e,
        0x80b8, 0x0000ff07,     /* fog density */
        0x80b4, 0x007f7f7f,     /* fog vertex color */
        0x80b0, 0x007f7f7f,     /* fog table color */
        0x8108, 0x00000003      /* 32bit palette  */
};

static unsigned int scrn_params[] = {
        0x80e8, 0x00160000,     /* screen control */
        0x8044, 0x00800000,     /* pixel mode (vb+0x11) */
        0x805c, 0x00000000,     /* Size modulo and display lines (vb+0x17) */
        0x80d0, 0x00000100,     /* interlace flags */
        0x80d8, 0x020c0359,     /* M */
        0x80cc, 0x001501fe,     /* M */
        0x80d4, 0x007e0345,     /* horizontal border */
        0x80dc, 0x00240204,     /* vertical position */
        0x80e0, 0x07d6c63f,     /* sync control */
        0x80ec, 0x000000a4,     /* horizontal position */
        0x80f0, 0x00120012,     /* vertical border */
        0x80c8, 0x03450000,     /* set to same as border H in 80d4 */
        0x8068, 0x027f0000,     /* (X resolution - 1) << 16 */
        0x806c, 0x01df0000,     /* (Y resolution - 1) << 16 */
        0x804c, 0x000000a0,     /* display align */
        0x8118, 0x00008040,     /* M */
        0x80f4, 0x00000401,     /* anti-aliasing */
        0x8048, 0x00000009,     /* alpha config */
        0x7814, 0x00000000,     /* More interrupt control stuff (so it seems)*/
        0x7834, 0x00000000,
        0x7854, 0x00000000,
        0x7874, 0x00000000,
        0x78bc, 0x4659404f,
        0x8040, 0x00000000      /* border color */
};

static void set_regs(unsigned int *values, int cnt)
{
  volatile unsigned char *regs = (volatile unsigned char *)(void *)0xa05f0000;
  unsigned int r, v;

  while(cnt--) {
    r = *values++;
    v = *values++;
    *(volatile unsigned int *)(regs+r) = v;
  }
}

int pvr_check_cable()
{
  volatile unsigned int *porta = (unsigned int *)0xff80002c;

  /* PORT8 and PORT9 is input */
  *porta = (*porta & ~0xf0000) | 0xa0000;

  /* Return PORT8 and PORT9 */
  return ((*(volatile unsigned short *)(porta+1))>>8)&3;
}
void pvr_init_video(int cabletype, int mode, int res)
{
  volatile unsigned int *videobase=(volatile unsigned int *)(void*)0xa05f8000;
  static int bppshifttab[]= { 1,1,0,2 };
  int shift, lines, modulo, words_per_line, vpos;
  int laceoffset=0, voffset=0;
  unsigned int videoflags, attribs;
  unsigned int hvcounter = (res<2? 0x01060359 : 0x020c0359);

  mode &= 3;
  shift = bppshifttab[mode];

  videobase[8/4]=0;
  videobase[0x40/4]=0;

  /* Select pixel clock and colour mode */
  mode = (mode<<2)|1;
  lines = 240;
  if(!(cabletype&2)) {

    /* VGA mode */

    if(res < 2)
      mode |= 2; /* doublescan */

    hvcounter = 0x020c0359;

    lines <<= 1;
    mode |= 0x800000; /* fast pixel clock */
  }
  videobase[0x44/4]=mode;

  /* Set video base address.  Short fields will be offset by
     640 pixels, regardless of horizontal resolution.       */
  videobase[0x50/4]=0;
  videobase[0x54/4]=640<<shift;

  /* Set screen size, modulo, and interlace flag */
  videoflags = 1<<8;
  if(res==0)
    words_per_line=(320/4)<<shift;
  else
    words_per_line=(640/4)<<shift;
  modulo = 1;

  if(!(cabletype&2))
  {
    if(res==0)
      /* VGA lores -> skip 320 pixels to keep modulo at 640 pixels */
      modulo += words_per_line;
  } else {
    if(res!=1)
      /* NTSC lores -> skip 320 pixels to keep modulo at 640 pixels */
      /* _or_ NTSC hires -> skip every other line due to interlace  */
      modulo += words_per_line;

    if(res==2)
      /* interlace mode */
      videoflags |= 1<<4;

    /* enable NTSC */
    videoflags |= 1<<6;
  }

  /* Write screen size and modulo */
  videobase[0x5c/4]=(((modulo<<10)+lines-1)<<10)+words_per_line-1;

  /* Enable video (lace, NTSC) */
  videobase[0xd0/4]=videoflags;

  /* Screen and border position */

  if(!(cabletype&2))
    /* VGA */
    voffset += 36;
  else
    voffset += 18;

  vpos=(voffset<<16)|(voffset+laceoffset);

  videobase[0xf0/4]=vpos;       /* V start              */
  videobase[0xdc/4]=vpos+lines; /* start and end border */
  videobase[0xec/4]=0xa4;       /* Horizontal pos       */
  videobase[0xd8/4]=hvcounter;  /* HV counter           */
  videobase[0xd4/4]=0x007e0345; /* Horizontal border    */

  /* Select horizontal pixel doubling for lowres */
  if(res==0)
    attribs=((22<<8)+1)<<8;
  else
    attribs=22<<16;
  videobase[0xe8/4]=attribs;

  /* Set up vertical blank event */
  vpos = 260;
  if(!(cabletype&2))
    vpos = 510;
  videobase[0xcc/4]=(0x21<<16)|vpos;

  /* Select RGB/CVBS */
  if(cabletype&1)
    mode = 3;
  else
    mode = 0;
  *(volatile unsigned int *)(void*)0xa0702c00 = mode << 8;

  return;
}

void pvr_init()
{
  volatile unsigned int *vbl = (volatile unsigned int *)(void *)0xa05f810c;

  set_regs(three_d_params, sizeof(three_d_params)/sizeof(three_d_params[0])/2);
  while (!(*vbl & 0x01ff));
  while (*vbl & 0x01ff);
  set_regs(scrn_params, sizeof(scrn_params)/sizeof(scrn_params[0])/2);
  pvr_init_video(pvr_check_cable(), 1, 2);
}

void draw_grid( unsigned short *addr, unsigned short colour )
{
    int x,y;
    unsigned int linesize = 640;
    for( x=0; x<640; x+=32 ) {
        for( y=0; y<480; y++ ) {
            addr[(linesize*y) + x] = colour;
        }
    }
    for( y=0; y<480; y+=32 ) {
        for( x=0; x<640; x++ ) {
            addr[(linesize*y) + x] = colour;
        }
    }
}

void draw_grid_24( unsigned char *addr, unsigned int colour )
{
    int x,y;
    char r = (colour >> 16) & 0xFF;
    char g = (colour >> 8) & 0xFF;
    char b = (colour & 0xFF);
    unsigned int linesize = 640*3;
    for( x=0; x<640; x+=32 ) {
        for( y=0; y<480; y++ ) {
            int a = (linesize*y)+x * 3;
            addr[a++] = r;
            addr[a++] = g;
            addr[a++] = b;
        }
    }
    for( y=0; y<480; y+=32 ) {
        for( x=0; x<640; x++ ) {
            int a = (linesize*y)+x * 3;
            addr[a++] = r;
            addr[a++] = g;
            addr[a++] = b;
        }
    }
}
