/**
 * $Id: pvr2mmio.h,v 1.7 2006-08-06 02:47:08 nkeynes Exp $
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
    LONG_PORT( 0x000, PVRID, PORT_R, 0x17FD11DB, "PVR2 Core ID" )
    LONG_PORT( 0x004, PVRVER, PORT_R, 0x00000011, "PVR2 Core Version" )
    LONG_PORT( 0x008, PVRRESET, PORT_MRW, 0, "PVR2 Reset" )
    LONG_PORT( 0x014, RENDER_START, PORT_W, 0, "Start render" )
    LONG_PORT( 0x018, PVRUNK1, PORT_MRW, 0, "PVR2 unknown register 1" )
    LONG_PORT( 0x020, RENDER_POLYBASE, PORT_MRW, 0, "Object buffer base offset" )
    LONG_PORT( 0x02C, RENDER_TILEBASE, PORT_MRW, 0, "Tile buffer base offset" )
    LONG_PORT( 0x030, RENDER_TSPCFG, PORT_MRW, 0, "TSP config?" )
    LONG_PORT( 0x040, DISP_BORDER, PORT_MRW, 0, "Border Colour (RGB)" )
    LONG_PORT( 0x044, DISP_MODE, PORT_MRW, 0, "Display Mode" )
    LONG_PORT( 0x048, RENDER_MODE, PORT_MRW, 0, "Rendering Mode" )
    LONG_PORT( 0x04C, RENDER_SIZE, PORT_MRW, 0, "Rendering width (bytes/2)" )
    LONG_PORT( 0x050, DISP_ADDR1, PORT_MRW, 0, "Video memory base 1" )
    LONG_PORT( 0x054, DISP_ADDR2, PORT_MRW, 0, "Video memory base 2" )
    LONG_PORT( 0x05C, DISP_SIZE, PORT_MRW, 0, "Display size" )
    LONG_PORT( 0x060, RENDER_ADDR1, PORT_MRW, 0, "Rendering memory base 1" )
    LONG_PORT( 0x064, RENDER_ADDR2, PORT_MRW, 0, "Rendering memory base 2" )
    LONG_PORT( 0x068, RENDER_HCLIP, PORT_MRW, 0, "Horizontal clipping area" )
    LONG_PORT( 0x06C, RENDER_VCLIP, PORT_MRW, 0, "Vertical clipping area" )
    LONG_PORT( 0x074, RENDER_SHADOW, PORT_MRW, 0, "Shadowing" )
    LONG_PORT( 0x078, RENDER_NEARCLIP, PORT_MRW, 0, "Object clip distance (float32)" )
    LONG_PORT( 0x07C, RENDER_OBJCFG, PORT_MRW, 0, "Object config" )
    LONG_PORT( 0x080, PVRUNK2, PORT_MRW, 0, "PVR2 unknown register 2" )
    LONG_PORT( 0x084, RENDER_TSPCLIP, PORT_MRW, 0, "Texture clip distance (float32)" )
    LONG_PORT( 0x088, RENDER_FARCLIP, PORT_MRW, 0, "Background plane depth (float32)" )
    LONG_PORT( 0x08C, RENDER_BGPLANE, PORT_MRW, 0, "Background plane config" )
    LONG_PORT( 0x098, RENDER_ISPCFG, PORT_MRW, 0, "ISP config" )
    LONG_PORT( 0x0A0, VRAM_CFG1, PORT_MRW, 0, "VRAM config 1" )
    LONG_PORT( 0x0A4, VRAM_CFG2, PORT_MRW, 0, "VRAM config 2" )
    LONG_PORT( 0x0A8, VRAM_CFG3, PORT_MRW, 0, "VRAM config 3" )
    LONG_PORT( 0x0B0, RENDER_FOGTBLCOL, PORT_MRW, 0, "Fog table colour" )
    LONG_PORT( 0x0B4, RENDER_FOGVRTCOL, PORT_MRW, 0, "Fog vertex colour" )
    LONG_PORT( 0x0B8, RENDER_FOGCOEFF, PORT_MRW, 0, "Fog density coefficient (float16)" )
    LONG_PORT( 0x0BC, RENDER_CLAMPHI, PORT_MRW, 0, "Clamp high colour" )
    LONG_PORT( 0x0C0, RENDER_CLAMPLO, PORT_MRW, 0, "Clamp low colour" )
    LONG_PORT( 0x0C4, GUNPOS, PORT_MRW, 0, "Lightgun position" )
    LONG_PORT( 0x0C8, DISP_HPOSIRQ, PORT_MRW, 0, "Raster horizontal event position" )    
    LONG_PORT( 0x0CC, DISP_VPOSIRQ, PORT_MRW, 0, "Raster event position" )
    LONG_PORT( 0x0D0, DISP_CFG, PORT_MRW, 0, "Sync configuration & enable" )
    LONG_PORT( 0x0D4, DISP_HBORDER, PORT_MRW, 0, "Horizontal border area" )
    LONG_PORT( 0x0D8, DISP_SYNC, PORT_MRW, 0, "Sync pulse timing" )
    LONG_PORT( 0x0DC, DISP_VBORDER, PORT_MRW, 0, "Vertical border area" )
    LONG_PORT( 0x0E0, DISP_SYNC2, PORT_MRW, 0, "Sync pulse widths" )
    LONG_PORT( 0x0E4, RENDER_TEXSIZE, PORT_MRW, 0, "Texture modulo width" )
    LONG_PORT( 0x0E8, DISP_CFG2, PORT_MRW, 0, "Video configuration 2" )
    LONG_PORT( 0x0EC, DISP_HPOS, PORT_MRW, 0, "Horizontal display position" )
    LONG_PORT( 0x0F0, DISP_VPOS, PORT_MRW, 0, "Vertical display position" )
    LONG_PORT( 0x0F4, SCALERCFG, PORT_MRW, 0, "Scaler configuration (?)" )
    LONG_PORT( 0x108, RENDER_PALETTE, PORT_MRW, 0, "Palette configuration" )
    LONG_PORT( 0x10C, DISP_BEAMPOS, PORT_R, 0, "Raster beam position" )
    LONG_PORT( 0x110, PVRUNK3, PORT_MRW, 0, "PVR2 unknown register 3" )
    LONG_PORT( 0x114, PVRUNK4, PORT_MRW, 0, "PVR2 unknown register 4" )
    LONG_PORT( 0x118, PVRUNK5, PORT_MRW, 0, "PVR2 unkown register 5" )
    LONG_PORT( 0x11C, PVRUNK6, PORT_MRW, 0, "PVR2 unkown register 6" )
    LONG_PORT( 0x124, TA_TILEBASE, PORT_MRW, 0, "TA Tile matrix start" )
    LONG_PORT( 0x128, TA_POLYBASE, PORT_MRW, 0, "TA Polygon buffer start" )
    LONG_PORT( 0x12C, TA_LISTEND, PORT_MRW, 0, "TA Tile matrix end" )
    LONG_PORT( 0x130, TA_POLYEND, PORT_MRW, 0, "TA Polygon buffer end" )
    LONG_PORT( 0x134, TA_LISTPOS, PORT_R, 0, "TA Tile list position" )
    LONG_PORT( 0x138, TA_POLYPOS, PORT_R, 0, "TA Polygon buffer position" )
    LONG_PORT( 0x13C, TA_TILESIZE, PORT_MRW, 0, "TA Tile matrix size" )
    LONG_PORT( 0x140, TA_TILECFG, PORT_MRW, 0, "TA Tile matrix config" )
    LONG_PORT( 0x144, TA_INIT, PORT_W, 0, "TA Initialize" )
    LONG_PORT( 0x148, YUV_ADDR, PORT_MRW, 0, "YUV conversion address" )
    LONG_PORT( 0x14C, YUV_CFG, PORT_MRW, 0, "YUV configuration" )
    LONG_PORT( 0x150, YUV_COUNT, PORT_MR, 0, "YUV conversion count" )
    LONG_PORT( 0x160, TA_REINIT, PORT_W, 0, "TA re-initialize" )
    LONG_PORT( 0x164, TA_LISTBASE, PORT_MRW, 0, "TA Tile list start" )
    LONG_PORT( 0x1A8, PVRUNK7, PORT_MRW, 0, "PVR2 unknown register 7" )
MMIO_REGION_END

MMIO_REGION_BEGIN( 0x005F9000, PVR2PAL, "Power VR/2 CLUT Palettes" )
    LONG_PORT( 0x000, PAL0_0, PORT_MRW, 0, "Pal0 colour 0" )
MMIO_REGION_END

MMIO_REGION_BEGIN( 0x10000000, PVR2TA, "Power VR/2 TA Command port" )
    LONG_PORT( 0x000, TACMD, PORT_MRW, 0, "TA Command port" )
MMIO_REGION_END
