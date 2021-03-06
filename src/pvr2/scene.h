/**
 * $Id$
 *
 * PVR2 scene description structure (pvr2-private)
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

#ifndef lxdream_scene_H
#define lxdream_scene_H 1

#ifdef __cplusplus
extern "C" {
#endif

/************************* Intermediate vertex buffer ************************/

typedef enum { 
    SORT_NEVER = 0, 
    SORT_TILEFLAG = 1, /* In this mode, sorting is controlled by the per-segment flag */
    SORT_ALWAYS = 2 
} tile_sort_mode_t;

typedef enum { SHADOW_NONE=0, SHADOW_CHEAP=1, SHADOW_FULL=2 } shadow_mode_t;


struct vertex_struct {
    float u,v,r,tex_mode; /* tex-coord quad */
    float x,y,z,w;
    float rgba[4];
    float offset_rgba[4];
};

struct polygon_struct {
    uint32_t *context;
    uint32_t vertex_count; // number of vertexes in polygon
    uint32_t tex_id;
    int32_t vertex_index; // index of first vertex in vertex buffer
    uint32_t mod_tex_id;
    int32_t mod_vertex_index; // index of first modified vertex in vertex buffer
    struct polygon_struct *next; // chain for tri/quad arrays
    struct polygon_struct *sub_next; // chain for internal sub-polygons
};

void pvr2_scene_init(void);
void pvr2_scene_read(void);
void pvr2_scene_finished(void);
void pvr2_scene_shutdown();

uint32_t pvr2_scene_buffer_width();
uint32_t pvr2_scene_buffer_height();

extern unsigned char *video_base;

/**
 * Maximum possible size of the vertex buffer. This is figured as follows:
 * PVR2 polygon buffer is limited to 4MB. The tightest polygon format 
 * is 3 vertexes in 48 bytes = 16 bytes/vertex, (shadow triangle) 
 * (the next tightest is 8 vertex in 140 bytes (6-strip colour-only)).
 * giving a theoretical maximum of 262144 vertexes.
 * The expanded structure is 44 bytes/vertex, giving 
 * 11534336 bytes...
 */
#define MAX_VERTEXES 262144
#define MAX_VERTEX_BUFFER_SIZE (MAX_VERTEXES*sizeof(struct vertex_struct))
#define MIN_VERTEX_BUFFER_SIZE (2024*1024)

/**
 * Maximum polygons - smallest is 1 polygon in 48 bytes, giving
 * 87381, plus 1 for the background. Allow the same amount again
 * for split polygons (worst case)
 * 
 */
#define MAX_POLYGONS (87382*2)
#define MAX_POLY_BUFFER_SIZE (MAX_POLYGONS*sizeof(struct polygon_struct))
#define BUF_POLY_MAP_SIZE (4 MB)

/*************************************************************************/

/* Scene data - this structure holds all the intermediate data used during
 * the rendering process. 
 *
 * Special note: if vbo_supported == FALSE, then vertex_array points to a
 * malloced chunk of system RAM. Otherwise, vertex_array will be either NULL
 * (if the VBO is unmapped), or a pointer into a chunk of GL managed RAM
 * (possibly direct-mapped VRAM).
 */
struct pvr2_scene_struct {
    /** GL ID of the VBO used by the scene (or 0 if VBOs are not in use). */
    GLuint vbo_id;
    /** Pointer to the vertex array data, or NULL for unmapped VBOs */
    struct vertex_struct *vertex_array;
    /** Current allocated size (in bytes) of the vertex array */
    uint32_t vertex_array_size;
    /** Total number of vertexes in the scene (note modified vertexes
     * count for 2 vertexes */
    uint32_t vertex_count;

    /** Pointer to the polygon data for the scene (main ram). 
     * This will always have room for at least MAX_POLYGONS */
    struct polygon_struct *poly_array;
    /** Pointer to the background polygon. This is always a quad, and
     * normally the last member of poly_array */
    struct polygon_struct *bkgnd_poly;
    /** Total number of polygons in the scene */
    uint32_t poly_count;

    /** Image bounds in 3D - x1,x2,y1,y2,z1,z2 
     * x and y values are determined by the clip planes, while z values are
     * determined from the vertex data itself.
     */
    float bounds[6];

    /* Total size of the image buffer, determined by the tile map used to
     * render the scene */
    uint32_t buffer_width, buffer_height;

    /** Specifies the translucency auto-sort mode for the scene */
    tile_sort_mode_t sort_mode;
    
    shadow_mode_t shadow_mode;

    float fog_lut_colour[4];
    float fog_vert_colour[4];
    
    /** Pointer to the start of the tile segment list in PVR2 VRAM (32-bit) */
    struct tile_segment *segment_list;
    /** Map from PVR2 polygon address to an element of poly_array. */
    struct polygon_struct **buf_to_poly_map;
    /** Pointer to the start of the raw polygon buffer in PVR2 VRAM (32-bit).
     * Also only used during parsing */
    uint32_t *pvr2_pbuf;
    /** Current vertex index during parsing */
    uint32_t vertex_index;
};

/**
 * Current scene structure. Note this should only be written to by vertex bufer
 * functions
 */
extern struct pvr2_scene_struct pvr2_scene;

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_scene_H */
