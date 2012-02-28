/**
 * $Id$
 *
 * mmio register code generator
 *
 * Copyright (c) 2010 Nathan Keynes.
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

#ifndef lxdream_genmmio_H
#define lxdream_genmmio_H 1

#include <stdint.h>
#include <glib/glist.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    REG_CONST,
    REG_RW,
    REG_RO,
    REG_WO,
    REG_MIRROR, /* Additional address for an existing register - value is the offset of the target reg */
} register_mode_t;

typedef enum {
    REG_I8,
    REG_I16,
    REG_I32,
    REG_I64,
    REG_F32,
    REG_F64,
    REG_STRING,
} register_type_t;

typedef enum {
    ENDIAN_DEFAULT,
    ENDIAN_LITTLE,
    ENDIAN_BIG
} register_endian_t;

typedef enum {
    ACCESS_DEFAULT,
    ACCESS_ANY,
    ACCESS_NOOFFSET,
    ACCESS_EXACT
} register_access_t;

typedef enum {
    TRACE_DEFAULT,
    TRACE_NEVER,
    TRACE_ALWAYS
} register_trace_t;

typedef enum {
    TEST_DEFAULT,
    TEST_OFF,
    TEST_ON
} register_test_t;

union apval {
    uint64_t i;
    char a[8];
    const char *s;
};

struct action {
    const char *filename;
    int lineno;
    const char *text;
};

typedef struct regflags {
    register_endian_t endian;
    register_access_t access;
    register_trace_t traceFlag;
    register_test_t testFlag;
    union apval maskValue;
    unsigned int fillSizeBytes;
    union apval fillValue;
} *regflags_t;

typedef struct regdef {
    const char *name;
    const char *description;
    uint32_t offset;
    unsigned numBytes;
    unsigned numElements;
    unsigned stride;
    register_mode_t mode;
    register_type_t type;
    gboolean initUndefined;
    union apval initValue;
    struct regflags flags;
    struct action *action;
} *regdef_t;

typedef struct regblock {
    const char *name;
    const char *description;
    uint32_t address;
    struct regflags flags;
    unsigned numRegs;
    unsigned blockSize;
    regdef_t regs[];
} *regblock_t;



GList *ioparse( const char *filename, GList *list );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_genmmio_H */
