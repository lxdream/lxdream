/**
 * $Id: pvr2mmio.h,v 1.3 2006-03-23 13:19:55 nkeynes Exp $
 *
 * PVR2 (video chip) MMIO register definitions.
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

#include "mmio.h"

MMIO_REGION_BEGIN( 0x005F8000, PVR2, "Power VR/2" )
    LONG_PORT( 0x000, PVRID, PORT_MR, 0x17FD11DB, "PVR2 Core ID" )
    LONG_PORT( 0x004, PVRVER, PORT_MR, 0x00000011, "PVR2 Core Version" )
    LONG_PORT( 0x008, PVRRST, PORT_MR, 0, "PVR2 Reset" )
    LONG_PORT( 0x014, RENDSTART, PORT_W, 0, "Start render" )
    LONG_PORT( 0x020, OBJBASE, PORT_MRW, 0, "Object buffer base offset" )
    LONG_PORT( 0x02C, TILEBASE, PORT_MRW, 0, "Tile buffer base offset" )
    LONG_PORT( 0x040, DISPBORDER, PORT_MRW, 0, "Border Colour (RGB)" )
    LONG_PORT( 0x044, DISPMODE, PORT_MRW, 0, "Display Mode" )
    LONG_PORT( 0x048, RENDMODE, PORT_MRW, 0, "Rendering Mode" )
    LONG_PORT( 0x04C, RENDSIZE, PORT_MRW, 0, "Rendering width (bytes/2)" )
    LONG_PORT( 0x050, DISPADDR1, PORT_MRW, 0, "Video memory base 1" )
    LONG_PORT( 0x054, DISPADDR2, PORT_MRW, 0, "Video memory base 2" )
    LONG_PORT( 0x05C, DISPSIZE, PORT_MRW, 0, "Display size" )
    LONG_PORT( 0x060, RENDADDR1, PORT_MRW, 0, "Rendering memory base 1" )
    LONG_PORT( 0x064, RENDADDR2, PORT_MRW, 0, "Rendering memory base 2" )
    LONG_PORT( 0x068, HCLIP, PORT_MRW, 0, "Horizontal clipping area" )
    LONG_PORT( 0x06C, VCLIP, PORT_MRW, 0, "Vertical clipping area" )
    LONG_PORT( 0x074, SHADOW, PORT_MRW, 0, "Shadowing" )
    LONG_PORT( 0x078, OBJCLIP, PORT_MRW, 0, "Object clip distance (float32)" )
    LONG_PORT( 0x07C, OBJCFG, PORT_MRW, 0, "Object config" )
    LONG_PORT( 0x084, TSPCLIP, PORT_MRW, 0, "Texture clip distance (float32)" )
    LONG_PORT( 0x088, BGPLANEZ, PORT_MRW, 0, "Background plane depth (float32)" )
    LONG_PORT( 0x08C, BGPLANE, PORT_MRW, 0, "Background plane config" )
    LONG_PORT( 0x0B0, FOGTBLCOL, PORT_MRW, 0, "Fog table colour" )
    LONG_PORT( 0x0B4, FOGVRTCOL, PORT_MRW, 0, "Fog vertex colour" )
    LONG_PORT( 0x0B8, FOGCOEFF, PORT_MRW, 0, "Fog density coefficient (float16)" )
    LONG_PORT( 0x0BC, CLAMPHI, PORT_MRW, 0, "Clamp high colour" )
    LONG_PORT( 0x0C0, CLAMPLO, PORT_MRW, 0, "Clamp low colour" )
    LONG_PORT( 0x0C4, GUNPOS, PORT_MRW, 0, "Lightgun position" )
    LONG_PORT( 0x0CC, VPOS_IRQ, PORT_MRW, 0, "Raster event position" )
    LONG_PORT( 0x0D0, DISPCFG, PORT_MRW, 0, "Sync configuration & enable" )
    LONG_PORT( 0x0D4, HBORDER, PORT_MRW, 0, "Horizontal border area" )
    LONG_PORT( 0x0D8, REFRESH, PORT_MRW, 0, "Refresh rates?" )
    LONG_PORT( 0x0DC, VBORDER, PORT_MRW, 0, "Vertical border area" )
    LONG_PORT( 0x0E0, SYNCPOS, PORT_MRW, 0, "Sync pulse timing" )
    LONG_PORT( 0x0E4, TSPCFG, PORT_MRW, 0, "Texture modulo width" )
    LONG_PORT( 0x0E8, DISPCFG2, PORT_MRW, 0, "Video configuration 2" )
    LONG_PORT( 0x0F0, VPOS, PORT_MRW, 0, "Vertical display position" )
    LONG_PORT( 0x0F4, SCALERCFG, PORT_MRW, 0, "Scaler configuration (?)" )
    LONG_PORT( 0x108, PALETTECFG, PORT_MRW, 0, "Palette configuration" )
    LONG_PORT( 0x10C, BEAMPOS, PORT_R, 0, "Raster beam position" )
    LONG_PORT( 0x124, TAOBJPBASE, PORT_MRW, 0, "TA Object Pointer Buffer start" )
    LONG_PORT( 0x128, TAOBJBASE, PORT_MRW, 0, "TA Object Buffer start" )
    LONG_PORT( 0x12C, TAOBJPEND, PORT_MRW, 0, "TA Object Pointer Buffer end" )
    LONG_PORT( 0x130, TAOBJEND, PORT_MRW, 0, "TA Object Buffer end" )
    LONG_PORT( 0x134, TAOBJPPOS, PORT_MRW, 0, "TA Object Pointer Buffer position" )
    LONG_PORT( 0x138, TAOBJPOS, PORT_MRW, 0, "TA Object Buffer position" )
    LONG_PORT( 0x13C, TATBSZ, PORT_MRW, 0, "TA Tile Buffer size" )
    LONG_PORT( 0x140, TAOPBCFG, PORT_MRW, 0, "TA Object Pointer Buffer config" )
    LONG_PORT( 0x144, TAINIT, PORT_MRW, 0, "TA Initialize" )
    LONG_PORT( 0x164, TAOPLST, PORT_MRW, 0, "TA Object Pointer List start" )
MMIO_REGION_END

MMIO_REGION_BEGIN( 0x005F9000, PVR2PAL, "Power VR/2 CLUT Palettes" )
    LONG_PORT( 0x000, PAL0_0, PORT_MRW, 0, "Pal0 colour 0" )
MMIO_REGION_END

MMIO_REGION_BEGIN( 0x10000000, PVR2TA, "Power VR/2 TA Command port" )
    LONG_PORT( 0x000, TACMD, PORT_MRW, 0, "TA Command port" )
MMIO_REGION_END
