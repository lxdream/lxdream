#include "dream.h"
#include "video.h"
#include "mem.h"
#include "asic.h"
#include "pvr2.h"
#define MMIO_IMPL
#include "pvr2.h"

char *video_base;

void pvr2_init( void )
{
    register_io_region( &mmio_region_PVR2 );
    video_base = mem_get_region_by_name( MEM_REGION_VIDEO );
}

uint32_t vid_stride, vid_lpf, vid_ppl, vid_hres, vid_vres, vid_col;
int interlaced, bChanged = 1, bEnabled = 0, vid_size = 0;
char *frame_start; /* current video start address (in real memory) */

/*
 * Display the next frame, copying the current contents of video ram to
 * the window. If the video configuration has changed, first recompute the
 * new frame size/depth.
 */
void pvr2_next_frame( void )
{
    if( bChanged ) {
        int dispsize = MMIO_READ( PVR2, DISPSIZE );
        int dispmode = MMIO_READ( PVR2, DISPMODE );
        int vidcfg = MMIO_READ( PVR2, VIDCFG );
        vid_stride = ((dispsize & DISPSIZE_MODULO) >> 20) - 1;
        vid_lpf = ((dispsize & DISPSIZE_LPF) >> 10) + 1;
        vid_ppl = ((dispsize & DISPSIZE_PPL)) + 1;
        vid_col = (dispmode & DISPMODE_COL);
        frame_start = video_base + MMIO_READ( PVR2, DISPADDR1 );
        interlaced = (vidcfg & VIDCFG_I ? 1 : 0);
        bEnabled = (dispmode & DISPMODE_DE) && (vidcfg & VIDCFG_VO ) ? 1 : 0;
        vid_size = (vid_ppl * vid_lpf) << (interlaced ? 3 : 2);
        vid_hres = vid_ppl;
        vid_vres = vid_lpf;
        if( interlaced ) vid_vres <<= 1;
        switch( vid_col ) {
            case MODE_RGB15:
            case MODE_RGB16: vid_hres <<= 1; break;
            case MODE_RGB24: vid_hres *= 3; break;
            case MODE_RGB32: vid_hres <<= 2; break;
        }
        vid_hres >>= 2;
        video_update_size( vid_hres, vid_vres, vid_col );
        bChanged = 0;
    }
    if( bEnabled ) {
        /* Assume bit depths match for now... */
        memcpy( video_data, frame_start, vid_size );
    } else {
        memset( video_data, 0, vid_size );
    }
    video_update_frame();
    asic_event( EVENT_SCANLINE1 );
    asic_event( EVENT_SCANLINE2 );
    asic_event( EVENT_RETRACE );
}

void mmio_region_PVR2_write( uint32_t reg, uint32_t val )
{
    if( reg >= 0x200 && reg < 0x600 ) { /* Fog table */
        MMIO_WRITE( PVR2, reg, val );
        /* I don't want to hear about these */
        return;
    }
    
    INFO( "PVR2 write to %08X <= %08X [%s: %s]", reg, val, 
          MMIO_REGID(PVR2,reg), MMIO_REGDESC(PVR2,reg) );
   
    switch(reg) {
        case DISPSIZE: bChanged = 1;
        case DISPMODE: bChanged = 1;
        case DISPADDR1: bChanged = 1;
        case DISPADDR2: bChanged = 1;
        case VIDCFG: bChanged = 1;
            break;
            
    }
    MMIO_WRITE( PVR2, reg, val );
}

MMIO_REGION_READ_FN( PVR2, reg )
{
    switch( reg ) {
        case BEAMPOS:
            return sh4r.icount&0x20 ? 0x2000 : 1;
        default:
            return MMIO_READ( PVR2, reg );
    }
}
