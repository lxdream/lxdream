/**
 * $Id: livevar.h 931 2008-10-31 02:57:59Z nkeynes $
 * 
 * IR optimizations
 *
 * Copyright (c) 2009 Nathan Keynes.
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

#ifndef lxdream_xiropt_H
#define lxdream_xiropt_H 1

#include "lxdream.h"
#include "xlat/xir.h"

/**
 * Live range data-structure. For each value defined by a program block,
 * we track it's def, last use, and distance between. This is slightly 
 * complicated by two additional concerns around dead store elimination:
 *   * A value may be coherent or dirty (ie value is consistent with the
 *     in-memory state data or not)
 *   * At the end of it's range, a value may be live, unconditionally 
 *     dead, or conditionally dead, depending on whether the home memory
 *     location is overwritten before the end of the block. 
 * A value is _conditionally_ dead if it is overwritten within the block, 
 * but may be exposed by an exception.
 * 
 * We represent this by an additional field visible_length - the length of
 * time (in instructions) that the value is (potentially) externally visible.
 * It takes the following values:
 *   -1  - Always visible (ie live at end of block)
 *    0  - Never visible  (ie coherent)
 *   == use_length - dead after last use
 *   > use_length  - eventually dead at end of visibility
 */
struct live_range {
    xir_op_t def; /* Value defining instruction */
    xir_offset_t def_offset; /* Offset of def relative to start of block */
    xir_op_t range_end;  /* Last use of the value */
    xir_offset_t use_length; /* Length of range to last real use */
    xir_offset_t visible_length; /* Length of full range of visibility */
};

/**
 * Replaces registers with immediates where the value is constant. Also
 * kills align instructions where the value can be determined to be
 * aligned already (either constant address or redundant alignment check),
 * and sat* instructions where S can be determined.
 * 
 * Performs a single forward pass over the IR.
 */
void xir_constant_propagation( xir_basic_block_t xbb, xir_op_t begin, xir_op_t end );

/**
 * Kill any instructions where the result cannot be exposed - that is, the value
 * is overwritten before the end of the block. Values that may be exposed by an
 * exception (but are otherwise dead) are removed by adding repair code to the
 * exception path where possible (essentially if the value can be reconstructed
 * from live values).
 * 
 * Performs a single backwards pass over the IR
 */ 
void xir_dead_code_elimination( xir_basic_block_t xbb, xir_op_t begin, xir_op_t end );

/**
 * Compute live range data for the code in the range start..end.
 * @param start First instruction to consider
 * @param end terminating instruction (not included in analysis). NULL for 
 * entire block.
 * @param live_ranges output buffer to receive live-range data
 * @param live_ranges_size Number of entries in live_ranges.
 * @return TRUE on success, FALSE if the algorithm ran out of buffer space.
 */ 
gboolean xir_live_range_calculate( xir_op_t begin, xir_op_t end, 
                               struct live_range *live_ranges, unsigned int live_ranges_size );

void xir_live_range_dump( struct live_range *ranges );


#endif /* !lxdream_livevar_H */
