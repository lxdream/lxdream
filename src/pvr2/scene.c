/**
 * $Id$
 *
 * Manage the internal vertex/polygon buffers and scene data structure.
 * Where possible this uses VBOs for the vertex + index data.
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

#include <assert.h>
#include <string.h>
#include <math.h>
#include "lxdream.h"
#include "display.h"
#include "pvr2/pvr2.h"
#include "pvr2/pvr2mmio.h"
#include "pvr2/glutil.h"
#include "pvr2/scene.h"

#define U8TOFLOAT(n)  (((float)((n)+1))/256.0)

static void unpack_bgra(uint32_t bgra, float *rgba)
{
    rgba[0] = ((float)(((bgra&0x00FF0000)>>16) + 1)) / 256.0;
    rgba[1] = ((float)(((bgra&0x0000FF00)>>8) + 1)) / 256.0;
    rgba[2] = ((float)((bgra&0x000000FF) + 1)) / 256.0;
    rgba[3] = ((float)(((bgra&0xFF000000)>>24) + 1)) / 256.0;
}

static inline uint32_t bgra_to_rgba(uint32_t bgra)
{
    return (bgra&0xFF00FF00) | ((bgra&0x00FF0000)>>16) | ((bgra&0x000000FF)<<16);
}

/**
 * Convert a half-float (16-bit) FP number to a regular 32-bit float.
 * Source is 1-bit sign, 5-bit exponent, 10-bit mantissa.
 * TODO: Check the correctness of this.
 */
static float halftofloat( uint16_t half )
{
    union {
        float f;
        uint32_t i;
    } temp;
    temp.i = ((uint32_t)half)<<16;
    return temp.f;
}

static float parse_fog_density( uint32_t value )
{
    union {
        uint32_t i;
        float f;
    } u;
    u.i = (((value+127)&0xFF)<<23)|((value & 0xFF00)<<7);
    return u.f;
}

struct pvr2_scene_struct pvr2_scene;

static gboolean vbo_init = FALSE;
static float scene_shadow_intensity = 0.0;

#ifdef ENABLE_VERTEX_BUFFER
static gboolean vbo_supported = FALSE;
#endif

/**
 * Test for VBO support, and allocate all the system memory needed for the
 * temporary structures. GL context must have been initialized before this
 * point.
 */
void pvr2_scene_init()
{
    if( !vbo_init ) {
#ifdef ENABLE_VERTEX_BUFFER
        if( isGLVertexBufferSupported() ) {
            vbo_supported = TRUE;
            pvr2_scene.vbo_id = 1;
        }
#endif
        pvr2_scene.vertex_array = NULL;
        pvr2_scene.vertex_array_size = 0;
        pvr2_scene.poly_array = g_malloc( MAX_POLY_BUFFER_SIZE );
        pvr2_scene.buf_to_poly_map = g_malloc0( BUF_POLY_MAP_SIZE );
        vbo_init = TRUE;
    }
}

/**
 * Clear the scene data structures in preparation for fresh data
 */
void pvr2_scene_reset()
{
    pvr2_scene.poly_count = 0;
    pvr2_scene.vertex_count = 0;
    memset( pvr2_scene.buf_to_poly_map, 0, BUF_POLY_MAP_SIZE );
}

void pvr2_scene_shutdown()
{
#ifdef ENABLE_VERTEX_BUFFER
    if( vbo_supported ) {
        glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
        glDeleteBuffersARB( 1, &pvr2_scene.vbo_id );
        pvr2_scene.vbo_id = 0;
    } else {
#endif
        g_free( pvr2_scene.vertex_array );
        pvr2_scene.vertex_array = NULL;
#ifdef ENABLE_VERTEX_BUFFER
    }
#endif

    g_free( pvr2_scene.poly_array );
    pvr2_scene.poly_array = NULL;
    g_free( pvr2_scene.buf_to_poly_map );
    pvr2_scene.buf_to_poly_map = NULL;
    vbo_init = FALSE;
}

void *vertex_buffer_map()
{
    // Allow 8 vertexes for the background (4+4)
    uint32_t size = (pvr2_scene.vertex_count + 8) * sizeof(struct vertex_struct);
#ifdef ENABLE_VERTEX_BUFFER
    if( vbo_supported ) {
        glGetError();
        glBindBufferARB( GL_ARRAY_BUFFER_ARB, pvr2_scene.vbo_id );
        if( size > pvr2_scene.vertex_array_size ) {
            glBufferDataARB( GL_ARRAY_BUFFER_ARB, size, NULL, GL_DYNAMIC_DRAW_ARB );
            int status = glGetError();
            if( status != 0 ) {
                fprintf( stderr, "Error %08X allocating vertex buffer\n", status );
                abort();
            }
            pvr2_scene.vertex_array_size = size;
        }
        pvr2_scene.vertex_array = glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
        assert(pvr2_scene.vertex_array != NULL );
    } else {
#endif
        if( size > pvr2_scene.vertex_array_size ) {
            pvr2_scene.vertex_array = g_realloc( pvr2_scene.vertex_array, size );
        }
#ifdef ENABLE_VERTEX_BUFFER
    }
#endif
    return pvr2_scene.vertex_array;
}

gboolean vertex_buffer_unmap()
{
#ifdef ENABLE_VERTEX_BUFFER
    if( vbo_supported ) {
        pvr2_scene.vertex_array = NULL;
        return glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
    } else {
        return TRUE;
    }
#else
    return TRUE;
#endif
}

static struct polygon_struct *scene_add_polygon( pvraddr_t poly_idx, int vertex_count,
                                                 shadow_mode_t is_modified )
{
    int vert_mul = is_modified != SHADOW_NONE ? 2 : 1;

    if( pvr2_scene.buf_to_poly_map[poly_idx] != NULL ) {
        if( vertex_count > pvr2_scene.buf_to_poly_map[poly_idx]->vertex_count ) {
            pvr2_scene.vertex_count += (vertex_count - pvr2_scene.buf_to_poly_map[poly_idx]->vertex_count) * vert_mul;
            pvr2_scene.buf_to_poly_map[poly_idx]->vertex_count = vertex_count;
        }
        return pvr2_scene.buf_to_poly_map[poly_idx];
    } else {
        struct polygon_struct *poly = &pvr2_scene.poly_array[pvr2_scene.poly_count++];
        poly->context = &pvr2_scene.pvr2_pbuf[poly_idx];
        poly->vertex_count = vertex_count;
        poly->vertex_index = -1;
        poly->mod_vertex_index = -1;
        poly->next = NULL;
        poly->sub_next = NULL;
        pvr2_scene.buf_to_poly_map[poly_idx] = poly;
        pvr2_scene.vertex_count += (vertex_count * vert_mul);
        return poly;
    }
}

/**
 * Given a starting polygon, break it at the specified triangle so that the
 * preceding triangles are retained, and the remainder are contained in a
 * new sub-polygon. Does not preserve winding.
 */
static struct polygon_struct *scene_split_subpolygon( struct polygon_struct *parent, int split_offset )
{
    assert( split_offset > 0 && split_offset < (parent->vertex_count-2) );
    assert( pvr2_scene.poly_count < MAX_POLYGONS );
    struct polygon_struct *poly = &pvr2_scene.poly_array[pvr2_scene.poly_count++];
    poly->vertex_count = parent->vertex_count - split_offset;
    poly->vertex_index = parent->vertex_index + split_offset;
    if( parent->mod_vertex_index == -1 ) {
        poly->mod_vertex_index = -1;
    } else {
        poly->mod_vertex_index = parent->mod_vertex_index + split_offset;
    }
    poly->context = parent->context;
    poly->next = NULL;
    poly->sub_next = parent->sub_next;

    parent->sub_next = poly;
    parent->vertex_count = split_offset + 2;

    return poly;
}

static float scene_get_palette_offset( uint32_t tex )
{
    uint32_t fmt = (tex & PVR2_TEX_FORMAT_MASK);
    if( fmt == PVR2_TEX_FORMAT_IDX4 ) {
        return ((float)((tex & 0x07E00000) >> 17))/1024.0 + 0.0002;
    } else if( fmt == PVR2_TEX_FORMAT_IDX8 ) {
        return ((float)((tex & 0x06000000) >> 17))/1024.0 + 0.0002;
    } else {
        return -1.0;
    }
}

/**
 * Decode a single PVR2 renderable vertex (opaque/trans/punch-out, but not shadow
 * volume)
 * @param vert Pointer to output vertex structure
 * @param poly1 First word of polygon context (needed to understand vertex)
 * @param poly2 Second word of polygon context
 * @param pvr2_data Pointer to raw pvr2 vertex data (in VRAM)
 * @param modify_offset Offset in 32-bit words to the tex/color data. 0 for
 *        the normal vertex, half the vertex length for the modified vertex.
 */
static void pvr2_decode_render_vertex( struct vertex_struct *vert, uint32_t poly1,
                                       uint32_t poly2, uint32_t tex, uint32_t *pvr2_data,
                                       int modify_offset )
{
    gboolean force_alpha = !POLY2_ALPHA_ENABLE(poly2);
    union pvr2_data_type {
        uint32_t *ival;
        float *fval;
    } data;

    data.ival = pvr2_data;

    vert->x = *data.fval++;
    vert->y = *data.fval++;

    float z = *data.fval++;
    if( !isfinite(z) ) {
        z = 0;
    } else if( z != 0 ) {
        z = 1/z;
    }
    if( z > pvr2_scene.bounds[5] ) {
        pvr2_scene.bounds[5] = z;
    } else if( z < pvr2_scene.bounds[4] && z != 0 ) {
        pvr2_scene.bounds[4] = z;
    }
    vert->z = z;
    data.ival += modify_offset;


    if( POLY1_TEXTURED(poly1) ) {
        if( POLY1_UV16(poly1) ) {
            vert->u = halftofloat( *data.ival>>16 );
            vert->v = halftofloat( *data.ival );
            data.ival++;
        } else {
            vert->u = *data.fval++;
            vert->v = *data.fval++;
        }

        switch( POLY2_TEX_BLEND(poly2) ) {
        case 0:/* Convert replace => modulate by setting colour values to 1.0 */
            vert->rgba[0] = vert->rgba[1] = vert->rgba[2] = vert->rgba[3] = 1.0;
            vert->tex_mode = 0.0;
            data.ival++; /* Skip the colour word */
            break;
        case 2: /* Decal */
            vert->tex_mode = 1.0;
            unpack_bgra(*data.ival++, vert->rgba);
            break;
        case 1:
            force_alpha = TRUE;
            /* fall-through */
        default:
            vert->tex_mode = 0.0;
            unpack_bgra(*data.ival++, vert->rgba);
            break;
        }
        vert->r = scene_get_palette_offset(tex);
    } else {
        vert->tex_mode = 2.0;
        vert->r = -1.0;
        unpack_bgra(*data.ival++, vert->rgba);
    }

    if( POLY1_SPECULAR(poly1) ) {
        unpack_bgra(*data.ival++, vert->offset_rgba);
    } else {
        vert->offset_rgba[0] = 0.0;
        vert->offset_rgba[1] = 0.0;
        vert->offset_rgba[2] = 0.0;
        vert->offset_rgba[3] = 0.0;
    }

    if( force_alpha ) {
        vert->rgba[3] = 1.0;
    }
}

/**
 * Compute texture, colour, and z values for 1 or more result points by interpolating from
 * a set of 3 input points. The result point(s) must define their x,y.
 */
static void scene_compute_vertexes( struct vertex_struct *result,
                                    int result_count,
                                    struct vertex_struct *input,
                                    gboolean is_solid_shaded )
{
    int i,j;
    float sx = input[2].x - input[1].x;
    float sy = input[2].y - input[1].y;
    float tx = input[0].x - input[1].x;
    float ty = input[0].y - input[1].y;

    float detxy = ((sy) * (tx)) - ((ty) * (sx));
    if( detxy == 0 ) {
        // If the input points fall on a line, they don't define a usable
        // polygon - the PVR2 takes the last input point as the result in
        // this case.
        for( i=0; i<result_count; i++ ) {
            float x = result[i].x;
            float y = result[i].y;
            memcpy( &result[i], &input[2], sizeof(struct vertex_struct) );
            result[i].x = x;
            result[i].y = y;
        }
        return;
    }
    float sz = input[2].z - input[1].z;
    float tz = input[0].z - input[1].z;
    float su = input[2].u - input[1].u;
    float tu = input[0].u - input[1].u;
    float sv = input[2].v - input[1].v;
    float tv = input[0].v - input[1].v;

    for( i=0; i<result_count; i++ ) {
        float t = ((result[i].x - input[1].x) * sy -
                (result[i].y - input[1].y) * sx) / detxy;
        float s = ((result[i].y - input[1].y) * tx -
                (result[i].x - input[1].x) * ty) / detxy;

        float rz = input[1].z + (t*tz) + (s*sz);
        if( rz > pvr2_scene.bounds[5] ) {
            pvr2_scene.bounds[5] = rz;
        } else if( rz < pvr2_scene.bounds[4] ) {
            pvr2_scene.bounds[4] = rz;
        }
        result[i].z = rz;
        result[i].u = input[1].u + (t*tu) + (s*su);
        result[i].v = input[1].v + (t*tv) + (s*sv);
        result[i].r = input[1].r; /* Last two components are flat */
        result[i].tex_mode = input[1].tex_mode;

        if( is_solid_shaded ) {
            memcpy( result->rgba, input[2].rgba, sizeof(result->rgba) );
            memcpy( result->offset_rgba, input[2].offset_rgba, sizeof(result->offset_rgba) );
        } else {
            float *rgba0 = input[0].rgba;
            float *rgba1 = input[1].rgba;
            float *rgba2 = input[2].rgba;
            float *rgba3 = result[i].rgba;
            for( j=0; j<8; j++ ) {
                float tc = *rgba0++ - *rgba1;
                float sc = *rgba2++ - *rgba1;
                float rc = *rgba1++ + (t*tc) + (s*sc);
                *rgba3++ = rc;
            }
        }
    }
}

static float scene_compute_lut_fog_vertex( float z, float fog_density, float fog_table[][2] )
{
    union {
        uint32_t i;
        float f;
    } v;
    v.f = z * fog_density;
    if( v.f < 1.0 ) v.f = 1.0;
    else if( v.f > 255.9999 ) v.f = 255.9999;
    
    uint32_t index = ((v.i >> 18) & 0x0F)|((v.i>>19)&0x70);
    return fog_table[index][0];
}

/**
 * Compute the fog coefficients for all polygons using lookup-table fog. It's 
 * a little more convenient to do this as a separate pass, since we don't have
 * to worry about computed vertexes.
 */
static void scene_compute_lut_fog( )
{
    int i,j;

    float fog_density = parse_fog_density(MMIO_READ( PVR2, RENDER_FOGCOEFF ));
    float fog_table[128][2];
    
    /* Parse fog table out into floating-point format */
    for( i=0; i<128; i++ ) {
        uint32_t ent = MMIO_READ( PVR2, RENDER_FOGTABLE + (i<<2) );
        fog_table[i][0] = ((float)(((ent&0x0000FF00)>>8) + 1)) / 256.0;
        fog_table[i][1] = ((float)((ent&0x000000FF) + 1)) / 256.0;
    }
    
    
    for( i=0; i<pvr2_scene.poly_count; i++ ) {
        int mode = POLY2_FOG_MODE(pvr2_scene.poly_array[i].context[1]);
        uint32_t index = pvr2_scene.poly_array[i].vertex_index;
        if( mode == PVR2_POLY_FOG_LOOKUP ) {
            for( j=0; j<pvr2_scene.poly_array[i].vertex_count; j++ ) {
                pvr2_scene.vertex_array[index+j].offset_rgba[3] = 
                    scene_compute_lut_fog_vertex( pvr2_scene.vertex_array[index+j].z, fog_density, fog_table );
            }
        } else if( mode == PVR2_POLY_FOG_LOOKUP2 ) {
            for( j=0; j<pvr2_scene.poly_array[i].vertex_count; j++ ) {
                pvr2_scene.vertex_array[index+j].rgba[0] = pvr2_scene.fog_lut_colour[0];
                pvr2_scene.vertex_array[index+j].rgba[1] = pvr2_scene.fog_lut_colour[1];
                pvr2_scene.vertex_array[index+j].rgba[2] = pvr2_scene.fog_lut_colour[2];
                pvr2_scene.vertex_array[index+j].rgba[3] = 
                    scene_compute_lut_fog_vertex( pvr2_scene.vertex_array[index+j].z, fog_density, fog_table );
                pvr2_scene.vertex_array[index+j].offset_rgba[3] = 0;
            }
        } else if( mode == PVR2_POLY_FOG_DISABLED ) {
            for( j=0; j<pvr2_scene.poly_array[i].vertex_count; j++ ) {
                pvr2_scene.vertex_array[index+j].offset_rgba[3] = 0;
            }
        }
    }    
}

/**
 * Manually cull back-facing polygons where we can - this actually saves
 * us a lot of time vs passing everything to GL to do it.
 */
static void scene_backface_cull()
{
    unsigned poly_idx;
    unsigned poly_count = pvr2_scene.poly_count; /* Note: we don't want to process any sub-polygons created here */
    for( poly_idx = 0; poly_idx<poly_count; poly_idx++ ) {
        uint32_t poly1 = pvr2_scene.poly_array[poly_idx].context[0];
        if( POLY1_CULL_ENABLE(poly1) ) {
            struct polygon_struct *poly = &pvr2_scene.poly_array[poly_idx];
            unsigned vert_idx = poly->vertex_index;
            unsigned tri_count = poly->vertex_count-2;
            struct vertex_struct *vert = &pvr2_scene.vertex_array[vert_idx];
            unsigned i;
            gboolean ccw = (POLY1_CULL_MODE(poly1) == CULL_CCW);
            int first_visible = -1, last_visible = -1;
            for( i=0; i<tri_count; i++ ) {
                float ux = vert[i+1].x - vert[i].x;
                float uy = vert[i+1].y - vert[i].y;
                float vx = vert[i+2].x - vert[i].x;
                float vy = vert[i+2].y - vert[i].y;
                float nz = (ux*vy) - (uy*vx);
                if( ccw ? nz > 0 : nz < 0 ) {
                    /* Surface is visible */
                    if( first_visible == -1 ) {
                        first_visible = i;
                        /* Elide the initial hidden triangles (note we don't
                         * need to care about winding anymore here) */
                        poly->vertex_index += i;
                        poly->vertex_count -= i;
                        if( poly->mod_vertex_index != -1 )
                            poly->mod_vertex_index += i;
                    } else if( last_visible != i-1 ) {
                        /* And... here we have to split the polygon. Allocate a new
                         * sub-polygon to hold the vertex references */
                        struct polygon_struct *sub = scene_split_subpolygon(poly, (i-first_visible));
                        poly->vertex_count -= (i-first_visible-1) - last_visible;
                        first_visible = i;
                        poly = sub;
                    }
                    last_visible = i;
                } /* Else culled */
                /* Invert ccw flag for triangle strip processing */
                ccw = !ccw;
            }
            if( last_visible == -1 ) {
                /* No visible surfaces, so we can mark the whole polygon as being vertex-less */
                poly->vertex_count = 0;
            } else if( last_visible != tri_count-1 ) {
                /* Remove final hidden tris */
                poly->vertex_count -= (tri_count - 1 - last_visible);
            }
        }
    }
}

static void scene_add_cheap_shadow_vertexes( struct vertex_struct *src, struct vertex_struct *dest, int count )
{
    unsigned int i, j;
    
    for( i=0; i<count; i++ ) {
        dest->x = src->x;
        dest->y = src->y;
        dest->z = src->z;
        dest->u = src->u;
        dest->v = src->v;
        dest->r = src->r;
        dest->tex_mode = src->tex_mode;
        dest->rgba[0] = src->rgba[0] * scene_shadow_intensity;
        dest->rgba[1] = src->rgba[1] * scene_shadow_intensity;
        dest->rgba[2] = src->rgba[2] * scene_shadow_intensity;
        dest->rgba[3] = src->rgba[3] * scene_shadow_intensity;
        dest->offset_rgba[0] = src->offset_rgba[0] * scene_shadow_intensity;
        dest->offset_rgba[1] = src->offset_rgba[1] * scene_shadow_intensity;
        dest->offset_rgba[2] = src->offset_rgba[2] * scene_shadow_intensity;
        dest->offset_rgba[3] = src->offset_rgba[3];
        dest++;
        src++;
    }
}

static void scene_add_vertexes( pvraddr_t poly_idx, int vertex_length,
                                shadow_mode_t is_modified )
{
    struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[poly_idx];
    uint32_t *ptr = &pvr2_scene.pvr2_pbuf[poly_idx];
    uint32_t *context = ptr;
    unsigned int i;

    if( poly->vertex_index == -1 ) {
        ptr += (is_modified == SHADOW_FULL ? 5 : 3 );
        poly->vertex_index = pvr2_scene.vertex_index;

        assert( poly != NULL );
        assert( pvr2_scene.vertex_index + poly->vertex_count <= pvr2_scene.vertex_count );
        for( i=0; i<poly->vertex_count; i++ ) {
            pvr2_decode_render_vertex( &pvr2_scene.vertex_array[pvr2_scene.vertex_index++], context[0], context[1], context[2], ptr, 0 );
            ptr += vertex_length;
        }
        if( is_modified ) {
            assert( pvr2_scene.vertex_index + poly->vertex_count <= pvr2_scene.vertex_count );
            poly->mod_vertex_index = pvr2_scene.vertex_index;
            if( is_modified == SHADOW_FULL ) {
                int mod_offset = (vertex_length - 3)>>1;
                ptr = &pvr2_scene.pvr2_pbuf[poly_idx] + 5;
                for( i=0; i<poly->vertex_count; i++ ) {
                    pvr2_decode_render_vertex( &pvr2_scene.vertex_array[pvr2_scene.vertex_index++], context[0], context[3], context[4], ptr, mod_offset );
                    ptr += vertex_length;
                }
            } else {
                scene_add_cheap_shadow_vertexes( &pvr2_scene.vertex_array[poly->vertex_index], 
                        &pvr2_scene.vertex_array[poly->mod_vertex_index], poly->vertex_count );
                pvr2_scene.vertex_index += poly->vertex_count;
            }
        }
    }
}

static void scene_add_quad_vertexes( pvraddr_t poly_idx, int vertex_length,
                                     shadow_mode_t is_modified )
{
    struct polygon_struct *poly = pvr2_scene.buf_to_poly_map[poly_idx];
    uint32_t *ptr = &pvr2_scene.pvr2_pbuf[poly_idx];
    uint32_t *context = ptr;
    unsigned int i;

    if( poly->vertex_index == -1 ) {
        // Construct it locally and copy to the vertex buffer, as the VBO is
        // allowed to be horribly slow for reads (ie it could be direct-mapped
        // vram).
        struct vertex_struct quad[4];

        assert( poly != NULL );
        assert( pvr2_scene.vertex_index + poly->vertex_count <= pvr2_scene.vertex_count );
        ptr += (is_modified == SHADOW_FULL ? 5 : 3 );
        poly->vertex_index = pvr2_scene.vertex_index;
        for( i=0; i<4; i++ ) {
            pvr2_decode_render_vertex( &quad[i], context[0], context[1], context[2], ptr, 0 );
            ptr += vertex_length;
        }
        scene_compute_vertexes( &quad[3], 1, &quad[0], !POLY1_GOURAUD_SHADED(context[0]) );
        // Swap last two vertexes (quad arrangement => tri strip arrangement)
        memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index], quad, sizeof(struct vertex_struct)*2 );
        memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index+2], &quad[3], sizeof(struct vertex_struct) );
        memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index+3], &quad[2], sizeof(struct vertex_struct) );
        pvr2_scene.vertex_index += 4;

        if( is_modified ) {
            assert( pvr2_scene.vertex_index + poly->vertex_count <= pvr2_scene.vertex_count );
            poly->mod_vertex_index = pvr2_scene.vertex_index;
            if( is_modified == SHADOW_FULL ) {
                int mod_offset = (vertex_length - 3)>>1;
                ptr = &pvr2_scene.pvr2_pbuf[poly_idx] + 5;
                for( i=0; i<4; i++ ) {
                    pvr2_decode_render_vertex( &quad[4], context[0], context[3], context[4], ptr, mod_offset );
                    ptr += vertex_length;
                }
                scene_compute_vertexes( &quad[3], 1, &quad[0], !POLY1_GOURAUD_SHADED(context[0]) );
                memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index], quad, sizeof(struct vertex_struct)*2 );
                memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index+2], &quad[3], sizeof(struct vertex_struct) );
                memcpy( &pvr2_scene.vertex_array[pvr2_scene.vertex_index+3], &quad[2], sizeof(struct vertex_struct) );
            } else {
                scene_add_cheap_shadow_vertexes( &pvr2_scene.vertex_array[poly->vertex_index], 
                        &pvr2_scene.vertex_array[poly->mod_vertex_index], poly->vertex_count );
                pvr2_scene.vertex_index += poly->vertex_count;
            }
            pvr2_scene.vertex_index += 4;
        }
    }
}

static void scene_extract_polygons( pvraddr_t tile_entry )
{
    uint32_t *tile_list = (uint32_t *)(pvr2_main_ram+tile_entry);
    do {
        uint32_t entry = *tile_list++;
        if( entry >> 28 == 0x0F ) {
            break;
        } else if( entry >> 28 == 0x0E ) {
            tile_list = (uint32_t *)(pvr2_main_ram + (entry&0x007FFFFF));
        } else {
            pvraddr_t polyaddr = entry&0x000FFFFF;
            shadow_mode_t is_modified = (entry & 0x01000000) ? pvr2_scene.shadow_mode : SHADOW_NONE;
            int vertex_length = (entry >> 21) & 0x07;
            int context_length = 3;
            if( is_modified == SHADOW_FULL ) {
                context_length = 5;
                vertex_length <<= 1 ;
            }
            vertex_length += 3;

            if( (entry & 0xE0000000) == 0x80000000 ) {
                /* Triangle(s) */
                int strip_count = ((entry >> 25) & 0x0F)+1;
                int polygon_length = 3 * vertex_length + context_length;
                int i;
                struct polygon_struct *last_poly = NULL;
                for( i=0; i<strip_count; i++ ) {
                    struct polygon_struct *poly = scene_add_polygon( polyaddr, 3, is_modified );
                    polyaddr += polygon_length;
                    if( last_poly != NULL && last_poly->next == NULL ) {
                        last_poly->next = poly;
                    }
                    last_poly = poly;
                }
            } else if( (entry & 0xE0000000) == 0xA0000000 ) {
                /* Sprite(s) */
                int strip_count = ((entry >> 25) & 0x0F)+1;
                int polygon_length = 4 * vertex_length + context_length;
                int i;
                struct polygon_struct *last_poly = NULL;
                for( i=0; i<strip_count; i++ ) {
                    struct polygon_struct *poly = scene_add_polygon( polyaddr, 4, is_modified );
                    polyaddr += polygon_length;
                    if( last_poly != NULL && last_poly->next == NULL ) {
                        last_poly->next = poly;
                    }
                    last_poly = poly;
                }
            } else {
                /* Polygon */
                int i, last = -1;
                for( i=5; i>=0; i-- ) {
                    if( entry & (0x40000000>>i) ) {
                        last = i;
                        break;
                    }
                }
                if( last != -1 ) {
                    scene_add_polygon( polyaddr, last+3, is_modified );
                }
            }
        }
    } while( 1 );
}

static void scene_extract_vertexes( pvraddr_t tile_entry )
{
    uint32_t *tile_list = (uint32_t *)(pvr2_main_ram+tile_entry);
    do {
        uint32_t entry = *tile_list++;
        if( entry >> 28 == 0x0F ) {
            break;
        } else if( entry >> 28 == 0x0E ) {
            tile_list = (uint32_t *)(pvr2_main_ram + (entry&0x007FFFFF));
        } else {
            pvraddr_t polyaddr = entry&0x000FFFFF;
            shadow_mode_t is_modified = (entry & 0x01000000) ? pvr2_scene.shadow_mode : SHADOW_NONE;
            int vertex_length = (entry >> 21) & 0x07;
            int context_length = 3;
            if( is_modified == SHADOW_FULL ) {
                context_length = 5;
                vertex_length <<=1 ;
            }
            vertex_length += 3;

            if( (entry & 0xE0000000) == 0x80000000 ) {
                /* Triangle(s) */
                int strip_count = ((entry >> 25) & 0x0F)+1;
                int polygon_length = 3 * vertex_length + context_length;
                int i;
                for( i=0; i<strip_count; i++ ) {
                    scene_add_vertexes( polyaddr, vertex_length, is_modified );
                    polyaddr += polygon_length;
                }
            } else if( (entry & 0xE0000000) == 0xA0000000 ) {
                /* Sprite(s) */
                int strip_count = ((entry >> 25) & 0x0F)+1;
                int polygon_length = 4 * vertex_length + context_length;
                int i;
                for( i=0; i<strip_count; i++ ) {
                    scene_add_quad_vertexes( polyaddr, vertex_length, is_modified );
                    polyaddr += polygon_length;
                }
            } else {
                /* Polygon */
                int i, last = -1;
                for( i=5; i>=0; i-- ) {
                    if( entry & (0x40000000>>i) ) {
                        last = i;
                        break;
                    }
                }
                if( last != -1 ) {
                    scene_add_vertexes( polyaddr, vertex_length, is_modified );
                }
            }
        }
    } while( 1 );
}

static void scene_extract_background( void )
{
    uint32_t bgplane = MMIO_READ(PVR2, RENDER_BGPLANE);
    int vertex_length = (bgplane >> 24) & 0x07;
    int context_length = 3, i;
    shadow_mode_t is_modified = (bgplane & 0x08000000) ? pvr2_scene.shadow_mode : SHADOW_NONE;

    struct polygon_struct *poly = &pvr2_scene.poly_array[pvr2_scene.poly_count++];
    uint32_t *context = &pvr2_scene.pvr2_pbuf[(bgplane & 0x00FFFFFF)>>3];
    poly->context = context;
    poly->vertex_count = 4;
    poly->vertex_index = pvr2_scene.vertex_count;
    if( is_modified == SHADOW_FULL ) {
        context_length = 5;
        vertex_length <<= 1;
    }
    if( is_modified != SHADOW_NONE ) {
        poly->mod_vertex_index = pvr2_scene.vertex_count + 4;
        pvr2_scene.vertex_count += 8;
    } else {
        poly->mod_vertex_index = -1;
        pvr2_scene.vertex_count += 4;
    }
    vertex_length += 3;
    context_length += (bgplane & 0x07) * vertex_length;

    poly->next = NULL;
    poly->sub_next = NULL;
    pvr2_scene.bkgnd_poly = poly;

    struct vertex_struct base_vertexes[3];
    uint32_t *ptr = context + context_length;
    for( i=0; i<3; i++ ) {
        pvr2_decode_render_vertex( &base_vertexes[i], context[0], context[1], context[2],
                ptr, 0 );
        ptr += vertex_length;
    }
    struct vertex_struct *result_vertexes = &pvr2_scene.vertex_array[poly->vertex_index];
    result_vertexes[0].x = result_vertexes[0].y = 0;
    result_vertexes[1].x = result_vertexes[3].x = pvr2_scene.buffer_width;
    result_vertexes[1].y = result_vertexes[2].x = 0;
    result_vertexes[2].y = result_vertexes[3].y  = pvr2_scene.buffer_height;
    scene_compute_vertexes( result_vertexes, 4, base_vertexes, !POLY1_GOURAUD_SHADED(context[0]) );

    if( is_modified == SHADOW_FULL ) {
        int mod_offset = (vertex_length - 3)>>1;
        ptr = context + context_length;
        for( i=0; i<3; i++ ) {
            pvr2_decode_render_vertex( &base_vertexes[i], context[0], context[3], context[4],
                    ptr, mod_offset );
            ptr += vertex_length;
        }
        result_vertexes = &pvr2_scene.vertex_array[poly->mod_vertex_index];
        result_vertexes[0].x = result_vertexes[0].y = 0;
        result_vertexes[1].x = result_vertexes[3].x = pvr2_scene.buffer_width;
        result_vertexes[1].y = result_vertexes[2].x = 0;
        result_vertexes[2].y = result_vertexes[3].y  = pvr2_scene.buffer_height;
        scene_compute_vertexes( result_vertexes, 4, base_vertexes, !POLY1_GOURAUD_SHADED(context[0]) );
    } else if( is_modified == SHADOW_CHEAP ) {
        scene_add_cheap_shadow_vertexes( &pvr2_scene.vertex_array[poly->vertex_index], 
                &pvr2_scene.vertex_array[poly->mod_vertex_index], poly->vertex_count );
        pvr2_scene.vertex_index += poly->vertex_count;
    }

}


uint32_t pvr2_scene_buffer_width()
{
    return pvr2_scene.buffer_width;
}

uint32_t pvr2_scene_buffer_height()
{
    return pvr2_scene.buffer_height;
}

/**
 * Extract the current scene into the rendering structures. We run two passes
 * - first pass extracts the polygons into pvr2_scene.poly_array (finding vertex counts),
 * second pass extracts the vertex data into the VBO/vertex array.
 *
 * Difficult to do in single pass as we don't generally know the size of a
 * polygon for certain until we've seen all tiles containing it. It also means we
 * can count the vertexes and allocate the appropriate size VBO.
 *
 * FIXME: accesses into VRAM need to be bounds-checked properly
 */
void pvr2_scene_read( void )
{
    pvr2_scene_init();
    pvr2_scene_reset();

    pvr2_scene.bounds[0] = MMIO_READ( PVR2, RENDER_HCLIP ) & 0x03FF;
    pvr2_scene.bounds[1] = ((MMIO_READ( PVR2, RENDER_HCLIP ) >> 16) & 0x03FF) + 1;
    pvr2_scene.bounds[2] = MMIO_READ( PVR2, RENDER_VCLIP ) & 0x03FF;
    pvr2_scene.bounds[3] = ((MMIO_READ( PVR2, RENDER_VCLIP ) >> 16) & 0x03FF) + 1;
    pvr2_scene.bounds[4] = pvr2_scene.bounds[5] = MMIO_READF( PVR2, RENDER_FARCLIP );

    uint32_t scaler = MMIO_READ( PVR2, RENDER_SCALER );
    if( scaler & SCALER_HSCALE ) {
    	/* If the horizontal scaler is in use, we're (in principle) supposed to
    	 * divide everything by 2. However in the interests of display quality,
    	 * instead we want to render to the unscaled resolution and downsample
    	 * only if/when required.
    	 */
    	pvr2_scene.bounds[1] *= 2;
    }
    
    uint32_t fog_col = MMIO_READ( PVR2, RENDER_FOGTBLCOL );
    unpack_bgra( fog_col, pvr2_scene.fog_lut_colour );
    fog_col = MMIO_READ( PVR2, RENDER_FOGVRTCOL );
    unpack_bgra( fog_col, pvr2_scene.fog_vert_colour );
    
    uint32_t *tilebuffer = (uint32_t *)(pvr2_main_ram + MMIO_READ( PVR2, RENDER_TILEBASE ));
    uint32_t *segment = tilebuffer;
    uint32_t shadow = MMIO_READ(PVR2,RENDER_SHADOW);
    pvr2_scene.segment_list = (struct tile_segment *)tilebuffer;
    pvr2_scene.pvr2_pbuf = (uint32_t *)(pvr2_main_ram + MMIO_READ(PVR2,RENDER_POLYBASE));
    pvr2_scene.shadow_mode = shadow & 0x100 ? SHADOW_CHEAP : SHADOW_FULL;
    scene_shadow_intensity = U8TOFLOAT(shadow&0xFF);

    int max_tile_x = 0;
    int max_tile_y = 0;
    int obj_config = MMIO_READ( PVR2, RENDER_OBJCFG );
    int isp_config = MMIO_READ( PVR2, RENDER_ISPCFG );

    if( (obj_config & 0x00200000) == 0 ) {
        if( isp_config & 1 ) {
            pvr2_scene.sort_mode = SORT_NEVER;
        } else {
            pvr2_scene.sort_mode = SORT_ALWAYS;
        }
    } else {
        pvr2_scene.sort_mode = SORT_TILEFLAG;
    }

    // Pass 1: Extract polygon list
    uint32_t control;
    int i;
    do {
        control = *segment++;
        int tile_x = SEGMENT_X(control);
        int tile_y = SEGMENT_Y(control);
        if( tile_x > max_tile_x ) {
            max_tile_x = tile_x;
        }
        if( tile_y > max_tile_y ) {
            max_tile_y = tile_y;
        }
        for( i=0; i<5; i++ ) {
            if( (*segment & NO_POINTER) == 0 ) {
                scene_extract_polygons( *segment );
            }
            segment++;
        }
    } while( (control & SEGMENT_END) == 0 );

    pvr2_scene.buffer_width = (max_tile_x+1)<<5;
    pvr2_scene.buffer_height = (max_tile_y+1)<<5;

    // Pass 2: Extract vertex data
    vertex_buffer_map();
    pvr2_scene.vertex_index = 0;
    segment = tilebuffer;
    do {
        control = *segment++;
        for( i=0; i<5; i++ ) {
            if( (*segment & NO_POINTER) == 0 ) {
                scene_extract_vertexes( *segment );
            }
            segment++;
        }
    } while( (control & SEGMENT_END) == 0 );

    scene_extract_background();
    scene_compute_lut_fog();
    scene_backface_cull();

    vertex_buffer_unmap();
}

/**
 * Dump the current scene to file in a (mostly) human readable form
 */
void pvr2_scene_print( FILE *f )
{
    int i,j;

    fprintf( f, "Polygons: %d\n", pvr2_scene.poly_count );
    for( i=0; i<pvr2_scene.poly_count; i++ ) {
        struct polygon_struct *poly = &pvr2_scene.poly_array[i];
        fprintf( f, "  %08X ", (uint32_t)(((unsigned char *)poly->context) - pvr2_main_ram) );
        switch( poly->vertex_count ) {
        case 3: fprintf( f, "Tri     " ); break;
        case 4: fprintf( f, "Quad    " ); break;
        default: fprintf( f,"%d-Strip ", poly->vertex_count-2 ); break;
        }
        fprintf( f, "%08X %08X %08X ", poly->context[0], poly->context[1], poly->context[2] );
        if( poly->mod_vertex_index != -1 ) {
            fprintf( f, "%08X %08X\n", poly->context[3], poly->context[5] );
        } else {
            fprintf( f, "\n" );
        }

        for( j=0; j<poly->vertex_count; j++ ) {
            struct vertex_struct *v = &pvr2_scene.vertex_array[poly->vertex_index+j];
            fprintf( f, "    %.5f %.5f %.5f, (%.5f,%.5f)  %.5f,%.5f,%.5f,%.5f  %.5f %.5f %.5f %.5f\n", v->x, v->y, v->z, v->u, v->v,
                     v->rgba[0], v->rgba[1], v->rgba[2], v->rgba[3],
                     v->offset_rgba[0], v->offset_rgba[1], v->offset_rgba[2], v->offset_rgba[3] );
        }
        if( poly->mod_vertex_index != -1 ) {
            fprintf( f, "  ---\n" );
            for( j=0; j<poly->vertex_count; j++ ) {
                struct vertex_struct *v = &pvr2_scene.vertex_array[poly->mod_vertex_index+j];
                fprintf( f, "    %.5f %.5f %.5f, (%.5f,%.5f)  %.5f,%.5f,%.5f,%.5f  %.5f %.5f %.5f %.5f\n", v->x, v->y, v->z, v->u, v->v,
                         v->rgba[0], v->rgba[1], v->rgba[2], v->rgba[3],
                         v->offset_rgba[0], v->offset_rgba[1], v->offset_rgba[2], v->offset_rgba[3] );
            }
        }
    }

}

void pvr2_scene_dump()
{
    pvr2_scene_print(stdout);
}
