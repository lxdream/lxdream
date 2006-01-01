/**
 * $Id: mmio.h,v 1.3 2006-01-01 08:09:17 nkeynes Exp $
 *
 * mmio.h defines a complicated batch of macros used to build up the 
 * memory-mapped I/O regions in a reasonably readable fashion.
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
#ifndef dream_mmio_H
#define dream_mmio_H 1

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

#include <stdint.h>
#include <stdlib.h>

#define PAGE_TABLE_ENTRIES 128*1024
#define PAGE_SIZE 4096
#define PAGE_BITS 12

#define PORT_R 1
#define PORT_W 2
#define PORT_MEM 4 /* store written value */
#define PORT_RW 3
#define PORT_MR 5
#define PORT_MRW 7
#define UNDEFINED 0xDEADBEEF /* This has to be a value that nothing inits to */

struct mmio_region {
    char *id, *desc;
    uint32_t base;
    int32_t (*io_read)(uint32_t addr);
    void (*io_write)(uint32_t addr, uint32_t val);
    char *mem;
    char *save_mem; /* Used to compare for gui updates */
    struct mmio_port {
        char *id, *desc;
        int width;
        uint32_t offset;
        uint32_t def_val;
        int flags;
        uint32_t *val;
    } ports[80];
    struct mmio_port **index; /* reverse lookup by address */
    int trace_flag; /* set to 1 to enable transfer traces */
};

void register_io_region( struct mmio_region *mmio );
void register_io_regions( struct mmio_region **mmiolist );

extern struct mmio_region *io_rgn[];
extern int num_io_rgns;

#define MMIO_READ( id, r ) *((int32_t *)(mmio_region_##id.mem + (r)))
#define MMIO_WRITE( id, r, v ) *((int32_t *)(mmio_region_##id.mem + (r))) = (v)
#define MMIO_REG( id, r ) ((int32_t *)(mmio_region_##id.mem + (r)))
#define MMIO_REGID( mid, r ) (mmio_region_##mid.index[(r)>>2] != NULL ? \
            mmio_region_##mid.index[(r)>>2]->id : "<UNDEF>" )
#define MMIO_REGDESC( mid, r) (mmio_region_##mid.index[(r)>>2] != NULL ? \
            mmio_region_##mid.index[(r)>>2]->desc : "Undefined register" )
#define MMIO_TRACE( mid ) mmio_region_##mid.trace_flag = 1
#define MMIO_NOTRACE( mid ) mmio_region_##mid.trace_flag = 0

#define MMIO_REGID_BYNUM( mid, r ) (io_rgn[mid]->index[(r)>>2] != NULL ? \
            io_rgn[mid]->index[(r)>>2]->id : "<UNDEF>" )
#define MMIO_REGDESC_BYNUM( mid, r ) (io_rgn[mid]->index[(r)>>2] != NULL ? \
            io_rgn[mid]->index[(r)>>2]->desc : "Undefined register" )
#define MMIO_NAME_BYNUM( mid ) (io_rgn[mid]->id)

#ifdef __cplusplus
}
#endif

#endif

#ifdef MMIO_IMPL

#ifndef MMIO_IMPL_INCLUDED
#define MMIO_IMPL_INCLUDED
#undef MMIO_REGION_BEGIN
#undef LONG_PORT
#undef WORD_PORT
#undef BYTE_PORT
#undef MMIO_REGION_END
#undef MMIO_REGION_LIST_BEGIN
#undef MMIO_REGION
#undef MMIO_REGION_LIST_END
#define MMIO_REGION_BEGIN(b,id,d) struct mmio_region mmio_region_##id = { #id, d, b, mmio_region_##id##_read, mmio_region_##id##_write, 0, 0, {
#define LONG_PORT( o,id,f,def,d ) { #id, d, 32, o, def, f },
#define WORD_PORT( o,id,f,def,d ) { #id, d, 16, o, def, f },
#define BYTE_PORT( o,id,f,def,d ) { #id, d, 8, o, def, f },
#define MMIO_REGION_END {NULL, NULL, 0, 0, 0, 0} } };
#define MMIO_REGION_LIST_BEGIN(id) struct mmio_region *mmio_list_##id[] = {
#define MMIO_REGION( id ) &mmio_region_##id,
#define MMIO_REGION_LIST_END NULL};

/* Stub defines for modules we haven't got to yet, or ones which don't
 * actually need any direct code on read and/or write
 */
#define MMIO_REGION_READ_STUBFN( id ) \
int32_t mmio_region_##id##_read( uint32_t reg ) { \
    int32_t val = MMIO_READ( id, reg ); \
    WARN( "Read from unimplemented module %s (%03X => %08X) [%s: %s]",\
          #id, reg, val, MMIO_REGID(id,reg), MMIO_REGDESC(id,reg) ); \
    return val; \
}
#define MMIO_REGION_WRITE_STUBFN( id ) \
void mmio_region_##id##_write( uint32_t reg, uint32_t val ) { \
    WARN( "Write to unimplemented module %s (%03X <= %08X) [%s: %s]", \
          #id, reg, val, MMIO_REGID(id,reg), MMIO_REGDESC(id,reg) ); \
    MMIO_WRITE( id, reg, val ); \
}
#define MMIO_REGION_STUBFNS( id ) \
    MMIO_REGION_READ_STUBFN( id ) \
    MMIO_REGION_WRITE_STUBFN( id )
#define MMIO_REGION_READ_DEFFN( id ) \
int32_t mmio_region_##id##_read( uint32_t reg ) { \
    return MMIO_READ( id, reg ); \
}
#define MMIO_REGION_WRITE_DEFFN( id ) \
void mmio_region_##id##_write( uint32_t reg, uint32_t val ) { \
    MMIO_WRITE( id, reg, val ); \
}
#define MMIO_REGION_DEFFNS( id ) \
    MMIO_REGION_READ_DEFFN( id ) \
    MMIO_REGION_WRITE_DEFFN( id )
#endif

#define MMIO_REGION_WRITE_FN( id, reg, val ) \
void mmio_region_##id##_write( uint32_t reg, uint32_t val )

#define MMIO_REGION_READ_FN( id, reg ) \
int32_t mmio_region_##id##_read( uint32_t reg )

#else

#ifndef MMIO_IFACE_INCLUDED
#define MMIO_IFACE_INCLUDED
#define MMIO_REGION_BEGIN(b,id,d) \
extern struct mmio_region mmio_region_##id; \
int32_t mmio_region_##id##_read(uint32_t); \
void mmio_region_##id##_write(uint32_t, uint32_t); \
enum mmio_region_##id##_port_t {
#define LONG_PORT( o,id,f,def,d ) id = o,
#define WORD_PORT( o,id,f,def,d ) id = o,
#define BYTE_PORT( o,id,f,def,d ) id = o,
#define MMIO_REGION_END };
#define MMIO_REGION_LIST_BEGIN(id) extern struct mmio_region *mmio_list_##id[];
#define MMIO_REGION( id )
#define MMIO_REGION_LIST_END
#endif

#endif

