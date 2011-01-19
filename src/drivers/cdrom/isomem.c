/*
 * Copyright (c) 2007 Vreixo Formoso
 * Copyright (c) 2009 Thomas Schmitt
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 
 * or later as published by the Free Software Foundation. 
 * See COPYING file for details.
 *
 * Memory stream extracted for use in lxdream by Nathan Keynes 2010.
 */

#include <libisofs.h>

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#ifndef MIN
#define MIN(a,b) ((a)<=(b) ? (a) : (b))
#endif

#define ISO_MEM_FS_ID 4

static ino_t mem_serial_id = (ino_t)1;

typedef struct
{
    uint8_t *buf;
    ssize_t offset; /* -1 if stream closed */
    ino_t ino_id;
    size_t size;
} MemStreamData;

static
int mem_open(IsoStream *stream)
{
    MemStreamData *data;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = (MemStreamData*)stream->data;
    if (data->offset != -1) {
        return ISO_FILE_ALREADY_OPENED;
    }
    data->offset = 0;
    return ISO_SUCCESS;
}

static
int mem_close(IsoStream *stream)
{
    MemStreamData *data;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = (MemStreamData*)stream->data;
    if (data->offset == -1) {
        return ISO_FILE_NOT_OPENED;
    }
    data->offset = -1;
    return ISO_SUCCESS;
}

static
off_t mem_get_size(IsoStream *stream)
{
    MemStreamData *data;
    data = (MemStreamData*)stream->data;

    return (off_t)data->size;
}

static
int mem_read(IsoStream *stream, void *buf, size_t count)
{
    size_t len;
    MemStreamData *data;
    if (stream == NULL || buf == NULL) {
        return ISO_NULL_POINTER;
    }
    if (count == 0) {
        return ISO_WRONG_ARG_VALUE;
    }
    data = stream->data;

    if (data->offset == -1) {
        return ISO_FILE_NOT_OPENED;
    }

    if (data->offset >= data->size) {
        return 0; /* EOF */
    }

    len = MIN(count, data->size - data->offset);
    memcpy(buf, data->buf + data->offset, len);
    data->offset += len;
    return len;
}

static
int mem_is_repeatable(IsoStream *stream)
{
    return 1;
}

static
void mem_get_id(IsoStream *stream, unsigned int *fs_id, dev_t *dev_id,
                ino_t *ino_id)
{
    MemStreamData *data;
    data = (MemStreamData*)stream->data;
    *fs_id = ISO_MEM_FS_ID;
    *dev_id = 0;
    *ino_id = data->ino_id;
}

static
void mem_free(IsoStream *stream)
{
    MemStreamData *data;
    data = (MemStreamData*)stream->data;
    free(data->buf);
    free(data);
}

IsoStreamIface mem_stream_class = {
    0,
    "mem ",
    mem_open,
    mem_close,
    mem_get_size,
    mem_read,
    mem_is_repeatable,
    mem_get_id,
    mem_free
};

/**
 * Create a stream for reading from a arbitrary memory buffer.
 * When the Stream refcount reach 0, the buffer is free(3).
 *
 * @return
 *      1 sucess, < 0 error
 */
int iso_mem_stream_new(unsigned char *buf, size_t size, IsoStream **stream)
{
    IsoStream *str;
    MemStreamData *data;

    if (buf == NULL || stream == NULL) {
        return ISO_NULL_POINTER;
    }

    str = malloc(sizeof(IsoStream));
    if (str == NULL) {
        return ISO_OUT_OF_MEM;
    }
    data = malloc(sizeof(MemStreamData));
    if (data == NULL) {
        free(str);
        return ISO_OUT_OF_MEM;
    }

    /* fill data */
    data->buf = buf;
    data->size = size;
    data->offset = -1;
    data->ino_id = mem_serial_id++;

    str->refcount = 1;
    str->data = data;
    str->class = &mem_stream_class;

    *stream = str;
    return ISO_SUCCESS;
}
