/**
 * $Id: pvr.h,v 1.2 2006-08-02 04:13:15 nkeynes Exp $
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

#define PVR_VRAM_BASE 0xA5000000
#define PVR_VRAM64_BASE 0xA4000000


#define TA_PIXFMT_RGB555   0
#define TA_PIXFMT_RGB565   1
#define TA_PIXFMT_ARGB4444 2
#define TA_PIXFMT_ARGB1555 3
#define TA_PIXFMT_RGB888   5
#define TA_PIXFMT_ARGB8888 6
#define TA_PIXFMT_DITHER   8

#define TA_CMD_POLYGON                    0x80000000
#define TA_CMD_MODIFIER                   0x80000000
#define TA_CMD_POLYGON_TYPE_OPAQUE        (0<<24)
#define TA_CMD_MODIFIER_TYPE_OPAQUE       (1<<24)
#define TA_CMD_POLYGON_TYPE_TRANSPARENT   (2<<24)
#define TA_CMD_MODIFIER_TYPE_TRANSPARENT  (3<<24)
#define TA_CMD_POLYGON_TYPE_PUNCHTHRU     (4<<24)
#define TA_CMD_POLYGON_SUBLIST            0x00800000
#define TA_CMD_POLYGON_STRIPLENGTH_1      (0<<18)
#define TA_CMD_POLYGON_STRIPLENGTH_2      (1<<18)
#define TA_CMD_POLYGON_STRIPLENGTH_4      (2<<18)
#define TA_CMD_POLYGON_STRIPLENGTH_6      (3<<18)
#define TA_CMD_POLYGON_USER_CLIP_INSIDE   0x00020000
#define TA_CMD_POLYGON_USER_CLIP_OUTSIDE  0x00030000
#define TA_CMD_POLYGON_AFFECTED_BY_MODIFIER  0x00000080
#define TA_CMD_POLYGON_CHEAP_SHADOW_MODIFIER 0x00000000
#define TA_CMD_POLYGON_NORMAL_MODIFIER       0x00000040
#define TA_CMD_POLYGON_PACKED_COLOUR      (0<<4)
#define TA_CMD_POLYGON_FLOAT_COLOUR       (1<<4)
#define TA_CMD_POLYGON_INTENSITY          (2<<4)
#define TA_CMD_POLYGON_PREVFACE_INTENSITY (3<<4)
#define TA_CMD_POLYGON_TEXTURED           0x00000008
#define TA_CMD_POLYGON_SPECULAR_HIGHLIGHT 0x00000004
#define TA_CMD_POLYGON_GOURAUD_SHADING    0x00000002
#define TA_CMD_POLYGON_16BIT_UV           0x00000001

#define TA_POLYMODE1_Z_NEVER        (0<<29)
#define TA_POLYMODE1_Z_LESS         (1<<29)
#define TA_POLYMODE1_Z_EQUAL        (2<<29)
#define TA_POLYMODE1_Z_LESSEQUAL    (3<<29)
#define TA_POLYMODE1_Z_GREATER      (4<<29)
#define TA_POLYMODE1_Z_NOTEQUAL     (5<<29)
#define TA_POLYMODE1_Z_GREATEREQUAL (6<<29)
#define TA_POLYMODE1_Z_ALWAYS       (7<<29)
#define TA_POLYMODE1_CULL_SMALL     (1<<27)
#define TA_POLYMODE1_CULL_CCW       (2<<27)
#define TA_POLYMODE1_CULL_CW        (3<<27)
#define TA_POLYMODE1_NO_Z_UPDATE    0x04000000

#define TA_POLYMODE2_BLEND_DEFAULT  (0x20<<24)
#define TA_POLYMODE2_FOG_TABLE      (0<<22)
#define TA_POLYMODE2_FOG_VERTEX     (1<<22)
#define TA_POLYMODE2_FOG_DISABLED   (2<<22)
#define TA_POLYMODE2_FOG_TABLE2     (3<<22)
#define TA_POLYMODE2_CLAMP_COLOURS  0x00200000
#define TA_POLYMODE2_ENABLE_ALPHA   0x00100000
#define TA_POLYMODE2_DISABLE_TEXTURE_TRANSPARENCY 0x00080000
#define TA_POLYMODE2_TEXTURE_FLIP_U   0x00080000
#define TA_POLYMODE2_TEXTURE_FLIP_V   0x00040000
#define TA_POLYMODE2_TEXTURE_CLAMP_U  0x00020000
#define TA_POLYMODE2_TEXTURE_CLAMP_V  0x00010000
#define TA_POLYMODE2_TRILINEAR_FILTER 0x00004000
#define TA_POLYMODE2_BILINEAR_FILTER  0x00002000

#define TA_POLYMODE2_MIPMAP_D_0_25    (1<<8)
#define TA_POLYMODE2_MIPMAP_D_0_50    (2<<8)
#define TA_POLYMODE2_MIPMAP_D_0_75    (3<<8)
#define TA_POLYMODE2_MIPMAP_D_1_00    (4<<8)
#define TA_POLYMODE2_MIPMAP_D_1_25    (5<<8)
#define TA_POLYMODE2_MIPMAP_D_1_50    (6<<8)
#define TA_POLYMODE2_MIPMAP_D_1_75    (7<<8)
#define TA_POLYMODE2_MIPMAP_D_2_00    (8<<8)
#define TA_POLYMODE2_MIPMAP_D_2_25    (9<<8)
#define TA_POLYMODE2_MIPMAP_D_2_50    (10<<8)
#define TA_POLYMODE2_MIPMAP_D_2_75    (11<<8)
#define TA_POLYMODE2_MIPMAP_D_3_00    (12<<8)
#define TA_POLYMODE2_MIPMAP_D_3_25    (13<<8)
#define TA_POLYMODE2_MIPMAP_D_3_50    (14<<8)
#define TA_POLYMODE2_MIPMAP_D_3_75    (15<<8)
#define TA_POLYMODE2_TEXTURE_REPLACE  (0<<6)
#define TA_POLYMODE2_TEXTURE_MODULATE (1<<6)
#define TA_POLYMODE2_TEXTURE_DECAL    (2<<6)
#define TA_POLYMODE2_U_SIZE_8         (0<<3)
#define TA_POLYMODE2_U_SIZE_16        (1<<3)
#define TA_POLYMODE2_U_SIZE_32        (2<<3)
#define TA_POLYMODE2_U_SIZE_64        (3<<3)
#define TA_POLYMODE2_U_SIZE_128       (4<<3)
#define TA_POLYMODE2_U_SIZE_256       (5<<3)
#define TA_POLYMODE2_U_SIZE_512       (6<<3)
#define TA_POLYMODE2_U_SIZE_1024      (7<<3)
#define TA_POLYMODE2_V_SIZE_8         (0<<0)
#define TA_POLYMODE2_V_SIZE_16        (1<<0)
#define TA_POLYMODE2_V_SIZE_32        (2<<0)
#define TA_POLYMODE2_V_SIZE_64        (3<<0)
#define TA_POLYMODE2_V_SIZE_128       (4<<0)
#define TA_POLYMODE2_V_SIZE_256       (5<<0)
#define TA_POLYMODE2_V_SIZE_512       (6<<0)
#define TA_POLYMODE2_V_SIZE_1024      (7<<0)
#define TA_TEXTUREMODE_MIPMAP       0x80000000
#define TA_TEXTUREMODE_VQ_COMPRESSION 0x40000000
#define TA_TEXTUREMODE_ARGB1555     (0<<27)
#define TA_TEXTUREMODE_RGB565       (1<<27)
#define TA_TEXTUREMODE_ARGB4444     (2<<27)
#define TA_TEXTUREMODE_YUV422       (3<<27)
#define TA_TEXTUREMODE_BUMPMAP      (4<<27)
#define TA_TEXTUREMODE_CLUT4        (5<<27)
#define TA_TEXTUREMODE_CLUT8        (6<<27)
#define TA_TEXTUREMODE_CLUTBANK8(n) ((n)<<25) /* 0-3  */
#define TA_TEXTUREMODE_CLUTBANK4(n) ((n)<<21) /* 0-63 */
#define TA_TEXTUREMODE_TWIDDLED     0x00000000
#define TA_TEXTUREMODE_NON_TWIDDLED 0x04000000
#define TA_TEXTUREMODE_ADDRESS(a)   ((((unsigned long)(void*)(a))&0x7fffff)>>3)
#define TA_CMD_VERTEX     0xe0000000
#define TA_CMD_VERTEX_LAST 0xF0000000  /* end of strip */

#define GRID_SIZE( hres, vres ) (((((vres+31) / 32)-1)<<16)|((((hres+31) / 32)-1)))

struct ta_config {
    unsigned int ta_cfg;
    unsigned int grid_size;
    unsigned int obj_start;
    unsigned int obj_end;
    unsigned int tile_start;
    unsigned int tile_end;
    unsigned int plist_start;
};

void ta_init( struct ta_config *config );
void pvr_dump_objbuf( FILE *f );
void pvr_dump_tilebuf( FILE *f );
int pvr_get_objbuf_size();
int pvr_get_plist_posn();
