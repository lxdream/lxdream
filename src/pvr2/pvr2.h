#include "mmio.h"

MMIO_REGION_BEGIN( 0x005F8000, PVR2, "Power VR/2" )
    LONG_PORT( 0x000, PVRID, PORT_MR, 0x17FD11DB, "PVR2 Core ID" )
    LONG_PORT( 0x004, PVRVER, PORT_MR, 0x00000011, "PVR2 Core Version" )
    LONG_PORT( 0x008, PVRRST, PORT_MR, 0, "PVR2 Reset" )
    LONG_PORT( 0x014, RENDST, PORT_W, 0, "Start render" )
    LONG_PORT( 0x020, OBJBASE, PORT_MRW, 0, "Object buffer base offset" )
    LONG_PORT( 0x02C, TILEBASE, PORT_MRW, 0, "Tile buffer base offset" )
    LONG_PORT( 0x040, PVRBCOL, PORT_MRW, 0, "Border Colour (RGB)" )
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
    LONG_PORT( 0x084, TSPCLIP, PORT_MRW, 0, "Texture clip distance (float32)" )
    LONG_PORT( 0x088, BGPLANEZ, PORT_MRW, 0, "Background plane depth (float32)" )
    LONG_PORT( 0x08C, BGPLANECFG, PORT_MRW, 0, "Background plane config" )
    LONG_PORT( 0x0B0, FGTBLCOL, PORT_MRW, 0, "Fog table colour" )
    LONG_PORT( 0x0B4, FGVRTCOL, PORT_MRW, 0, "Fog vertex colour" )
    LONG_PORT( 0x0B8, FGCOEFF, PORT_MRW, 0, "Fog density coefficient (float16)" )
    LONG_PORT( 0x0BC, CLAMPHI, PORT_MRW, 0, "Clamp high colour" )
    LONG_PORT( 0x0C0, CLAMPLO, PORT_MRW, 0, "Clamp low colour" )
    LONG_PORT( 0x0C4, GUNPOS, PORT_MRW, 0, "Lightgun position" )
    LONG_PORT( 0x0CC, EVTPOS, PORT_MRW, 0, "Raster event position" )
    LONG_PORT( 0x0D0, VIDCFG, PORT_MRW, 0, "Sync configuration & enable" )
    LONG_PORT( 0x0D4, HBORDER, PORT_MRW, 0, "Horizontal border area" )
    LONG_PORT( 0x0D8, REFRESH, PORT_MRW, 0, "Refresh rates?" )
    LONG_PORT( 0x0DC, VBORDER, PORT_MRW, 0, "Vertical border area" )
    LONG_PORT( 0x0E0, SYNCPOS, PORT_MRW, 0, "Sync pulse timing" )
    LONG_PORT( 0x0E4, TSPCFG, PORT_MRW, 0, "Texture modulo width" )
    LONG_PORT( 0x0E8, VIDCFG2, PORT_MRW, 0, "Video configuration 2" )
    LONG_PORT( 0x0F0, VPOS, PORT_MRW, 0, "Vertical display position" )
    LONG_PORT( 0x0F4, SCALERCFG, PORT_MRW, 0, "Scaler configuration (?)" )
    LONG_PORT( 0x10C, BEAMPOS, PORT_R, 0, "Raster beam position" )
    LONG_PORT( 0x124, TAOPBST, PORT_MRW, 0, "TA Object Pointer Buffer start" )
    LONG_PORT( 0x128, TAOBST, PORT_MRW, 0, "TA Object Buffer start" )
    LONG_PORT( 0x12C, TAOPBEN, PORT_MRW, 0, "TA Object Pointer Buffer end" )
    LONG_PORT( 0x130, TAOBEN, PORT_MRW, 0, "TA Object Buffer end" )
    LONG_PORT( 0x134, TAOPBPOS, PORT_MRW, 0, "TA Object Pointer Buffer position" )
    LONG_PORT( 0x138, TAOBPOS, PORT_MRW, 0, "TA Object Buffer position" )
    LONG_PORT( 0x13C, TATBSZ, PORT_MRW, 0, "TA Tile Buffer size" )
    LONG_PORT( 0x140, TAOPBCFG, PORT_MRW, 0, "TA Object Pointer Buffer config" )
    LONG_PORT( 0x144, TAINIT, PORT_MRW, 0, "TA Initialize" )
    LONG_PORT( 0x164, TAOPLST, PORT_MRW, 0, "TA Object Pointer List start" )
MMIO_REGION_END


#define DISPMODE_DE  0x00000001 /* Display enable */
#define DISPMODE_SD  0x00000002 /* Scan double */
#define DISPMODE_COL 0x0000000C /* Colour mode */
#define DISPMODE_CD  0x08000000 /* Clock double */

#define MODE_RGB15 0x00000000
#define MODE_RGB16 0x00000040
#define MODE_RGB24 0x00000080
#define MODE_RGB32 0x000000C0

#define DISPSIZE_MODULO 0x3FF00000 /* line skip +1 (32-bit words)*/
#define DISPSIZE_LPF    0x000FFC00 /* lines per field */
#define DISPSIZE_PPL    0x000003FF /* pixel words (32 bit) per line */

#define VIDCFG_VP 0x00000001 /* V-sync polarity */
#define VIDCFG_HP 0x00000002 /* H-sync polarity */
#define VIDCFG_I  0x00000010 /* Interlace enable */
#define VIDCFG_BS 0x000000C0 /* Broadcast standard */
#define VIDCFG_VO 0x00000100 /* Video output enable */

#define BS_NTSC 0x00000000
#define BS_PAL  0x00000040
#define BS_PALM 0x00000080 /* ? */
#define BS_PALN 0x000000C0 /* ? */

void pvr2_next_frame( void );
