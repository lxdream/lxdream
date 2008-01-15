/**
 * $Id$
 *
 * YUV420 and YUV422 decoding
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
#include "dream.h"
#include "asic.h"
#include "pvr2/pvr2.h"

#define YUV420_BLOCK_SIZE 384
#define YUV422_BLOCK_SIZE 512

#define FORMAT_YUV420 0
#define FORMAT_YUV422 1


static int yuv_block_size[2] = { YUV420_BLOCK_SIZE, YUV422_BLOCK_SIZE };

struct yuv_state {
    uint32_t target;
    int width;
    int height;
    int input_format;
    unsigned char data[512];
    int data_length;
    int x, y;
} pvr2_yuv_state;

/**
 * Transformation table for yuv420.
 */
uint16_t yuv420_lut[512] = { 0, 128, 64, 129, 1, 130, 65, 131, 2, 132, 66, 133, 3, 134, 67, 135, 4, 192, 68, 193, 5, 194, 69, 195, 6, 196, 70, 197, 7, 198, 71, 199,
			 0, 136, 64, 137, 1, 138, 65, 139, 2, 140, 66, 141, 3, 142, 67, 143, 4, 200, 68, 201, 5, 202, 69, 203, 6, 204, 70, 205, 7, 206, 71, 207,
			 8, 144, 72, 145, 9, 146, 73, 147, 10, 148, 74, 149, 11, 150, 75, 151, 12, 208, 76, 209, 13, 210, 77, 211, 14, 212, 78, 213, 15, 214, 79, 215,
			 8, 152, 72, 153, 9, 154, 73, 155, 10, 156, 74, 157, 11, 158, 75, 159, 12, 216, 76, 217, 13, 218, 77, 219, 14, 220, 78, 221, 15, 222, 79, 223,
			 16, 160, 80, 161, 17, 162, 81, 163, 18, 164, 82, 165, 19, 166, 83, 167, 20, 224, 84, 225, 21, 226, 85, 227, 22, 228, 86, 229, 23, 230, 87, 231,
			 16, 168, 80, 169, 17, 170, 81, 171, 18, 172, 82, 173, 19, 174, 83, 175, 20, 232, 84, 233, 21, 234, 85, 235, 22, 236, 86, 237, 23, 238, 87, 239,
			 24, 176, 88, 177, 25, 178, 89, 179, 26, 180, 90, 181, 27, 182, 91, 183, 28, 240, 92, 241, 29, 242, 93, 243, 30, 244, 94, 245, 31, 246, 95, 247,
			 24, 184, 88, 185, 25, 186, 89, 187, 26, 188, 90, 189, 27, 190, 91, 191, 28, 248, 92, 249, 29, 250, 93, 251, 30, 252, 94, 253, 31, 254, 95, 255,
			 32, 256, 96, 257, 33, 258, 97, 259, 34, 260, 98, 261, 35, 262, 99, 263, 36, 320, 100, 321, 37, 322, 101, 323, 38, 324, 102, 325, 39, 326, 103, 327,
			 32, 264, 96, 265, 33, 266, 97, 267, 34, 268, 98, 269, 35, 270, 99, 271, 36, 328, 100, 329, 37, 330, 101, 331, 38, 332, 102, 333, 39, 334, 103, 335,
			 40, 272, 104, 273, 41, 274, 105, 275, 42, 276, 106, 277, 43, 278, 107, 279, 44, 336, 108, 337, 45, 338, 109, 339, 46, 340, 110, 341, 47, 342, 111, 343,
			 40, 280, 104, 281, 41, 282, 105, 283, 42, 284, 106, 285, 43, 286, 107, 287, 44, 344, 108, 345, 45, 346, 109, 347, 46, 348, 110, 349, 47, 350, 111, 351,
			 48, 288, 112, 289, 49, 290, 113, 291, 50, 292, 114, 293, 51, 294, 115, 295, 52, 352, 116, 353, 53, 354, 117, 355, 54, 356, 118, 357, 55, 358, 119, 359,
			 48, 296, 112, 297, 49, 298, 113, 299, 50, 300, 114, 301, 51, 302, 115, 303, 52, 360, 116, 361, 53, 362, 117, 363, 54, 364, 118, 365, 55, 366, 119, 367,
			 56, 304, 120, 305, 57, 306, 121, 307, 58, 308, 122, 309, 59, 310, 123, 311, 60, 368, 124, 369, 61, 370, 125, 371, 62, 372, 126, 373, 63, 374, 127, 375,
			 56, 312, 120, 313, 57, 314, 121, 315, 58, 316, 122, 317, 59, 318, 123, 319, 60, 376, 124, 377, 61, 378, 125, 379, 62, 380, 126, 381, 63, 382, 127, 383 };

/**
 * Transformation table for YUV422 inputs
 */
uint16_t yuv422_lut[512] = { 0, 128, 64, 129, 1, 130, 65, 131, 2, 132, 66, 133, 3, 134, 67, 135, 4, 192, 68, 193, 5, 194, 69, 195, 6, 196, 70, 197, 7, 198, 71, 199,
			     8, 136, 72, 137, 9, 138, 73, 139, 10, 140, 74, 141, 11, 142, 75, 143, 12, 200, 76, 201, 13, 202, 77, 203, 14, 204, 78, 205, 15, 206, 79, 207,
			     16, 144, 80, 145, 17, 146, 81, 147, 18, 148, 82, 149, 19, 150, 83, 151, 20, 208, 84, 209, 21, 210, 85, 211, 22, 212, 86, 213, 23, 214, 87, 215,
			     24, 152, 88, 153, 25, 154, 89, 155, 26, 156, 90, 157, 27, 158, 91, 159, 28, 216, 92, 217, 29, 218, 93, 219, 30, 220, 94, 221, 31, 222, 95, 223,
			     32, 160, 96, 161, 33, 162, 97, 163, 34, 164, 98, 165, 35, 166, 99, 167, 36, 224, 100, 225, 37, 226, 101, 227, 38, 228, 102, 229, 39, 230, 103, 231,
			     40, 168, 104, 169, 41, 170, 105, 171, 42, 172, 106, 173, 43, 174, 107, 175, 44, 232, 108, 233, 45, 234, 109, 235, 46, 236, 110, 237, 47, 238, 111, 239,
			     48, 176, 112, 177, 49, 178, 113, 179, 50, 180, 114, 181, 51, 182, 115, 183, 52, 240, 116, 241, 53, 242, 117, 243, 54, 244, 118, 245, 55, 246, 119, 247,
			     56, 184, 120, 185, 57, 186, 121, 187, 58, 188, 122, 189, 59, 190, 123, 191, 60, 248, 124, 249, 61, 250, 125, 251, 62, 252, 126, 253, 63, 254, 127, 255,
			     256, 384, 320, 385, 257, 386, 321, 387, 258, 388, 322, 389, 259, 390, 323, 391, 260, 448, 324, 449, 261, 450, 325, 451, 262, 452, 326, 453, 263, 454, 327, 455,
			     264, 392, 328, 393, 265, 394, 329, 395, 266, 396, 330, 397, 267, 398, 331, 399, 268, 456, 332, 457, 269, 458, 333, 459, 270, 460, 334, 461, 271, 462, 335, 463,
			     272, 400, 336, 401, 273, 402, 337, 403, 274, 404, 338, 405, 275, 406, 339, 407, 276, 464, 340, 465, 277, 466, 341, 467, 278, 468, 342, 469, 279, 470, 343, 471,
			     280, 408, 344, 409, 281, 410, 345, 411, 282, 412, 346, 413, 283, 414, 347, 415, 284, 472, 348, 473, 285, 474, 349, 475, 286, 476, 350, 477, 287, 478, 351, 479,
			     288, 416, 352, 417, 289, 418, 353, 419, 290, 420, 354, 421, 291, 422, 355, 423, 292, 480, 356, 481, 293, 482, 357, 483, 294, 484, 358, 485, 295, 486, 359, 487,
			     296, 424, 360, 425, 297, 426, 361, 427, 298, 428, 362, 429, 299, 430, 363, 431, 300, 488, 364, 489, 301, 490, 365, 491, 302, 492, 366, 493, 303, 494, 367, 495,
			     304, 432, 368, 433, 305, 434, 369, 435, 306, 436, 370, 437, 307, 438, 371, 439, 308, 496, 372, 497, 309, 498, 373, 499, 310, 500, 374, 501, 311, 502, 375, 503,
			     312, 440, 376, 441, 313, 442, 377, 443, 314, 444, 378, 445, 315, 446, 379, 447, 316, 504, 380, 505, 317, 506, 381, 507, 318, 508, 382, 509, 319, 510, 383, 511 };

/**
 * Input is 8x8 U, 8x8 V, 8x8 Y00, 8x8 Y01, 8x8 Y10, 8x8 Y11, 8 bits each,
 * for a total of 384 bytes.
 * Output is UVYV = 32 bits = 2 horizontal pixels, 8x16 = 512 bytes
 */
void pvr2_decode_yuv420( unsigned char *dest, unsigned char *src )
{
    int i;
    for( i=0; i<512; i++ ) {
	dest[i] = src[yuv420_lut[i]];
    }
}

void pvr2_decode_yuv422( unsigned char *dest, unsigned char *src )
{
    int i;
    for( i=0; i<512; i++ ) {
	dest[i] = src[yuv422_lut[i]];
    }
}

/**
 * Process a single macroblock of YUV data and write it out to 
 * texture vram.
 */
void pvr2_yuv_process_block( unsigned char *data )
{
    unsigned char output[512];

    if( pvr2_yuv_state.input_format == FORMAT_YUV420 ) {
	pvr2_decode_yuv420( output, data );
    } else {
	pvr2_decode_yuv422( output, data );
    }
    
    uint32_t target = pvr2_yuv_state.target + 
	(pvr2_yuv_state.y * pvr2_yuv_state.width * 512) +
	(pvr2_yuv_state.x * 32);

    pvr2_vram64_write_stride( target, output, 32, pvr2_yuv_state.width*32, 16 );
    if( ++pvr2_yuv_state.x >= pvr2_yuv_state.width ) {
	pvr2_yuv_state.x = 0;
	pvr2_yuv_state.y++;
	if( pvr2_yuv_state.y >= pvr2_yuv_state.height ) {
	    asic_event( EVENT_PVR_YUV_DONE );
	    pvr2_yuv_state.y = 0;
	}
    }
    
    MMIO_WRITE( PVR2, YUV_COUNT, MMIO_READ( PVR2, YUV_COUNT ) + 1 );
}

/**
 * Receive data from the SH4, usually via DMA. This method is mainly responsible
 * for buffering the data into macroblock chunks and then passing it on to the
 * real processing
 */
void pvr2_yuv_write( unsigned char *data, uint32_t length )
{
    int block_size = yuv_block_size[pvr2_yuv_state.input_format];

    if( pvr2_yuv_state.data_length != 0 ) { /* Append to existing data */
	int tmp = MIN( length, block_size - pvr2_yuv_state.data_length );
	memcpy( pvr2_yuv_state.data + pvr2_yuv_state.data_length, 
		data, tmp );
	pvr2_yuv_state.data_length += tmp;
	data += tmp;
	length -= tmp;
	if( pvr2_yuv_state.data_length == block_size ) {
	    pvr2_yuv_process_block( pvr2_yuv_state.data );
	}
    }

    while( length >= block_size ) {
	pvr2_yuv_process_block( data );
	data += block_size;
	length -= block_size;
    }
	    
    if( length != 0 ) { /* Save the left over data */
	memcpy( pvr2_yuv_state.data, data, length );
	pvr2_yuv_state.data_length = length;
    }
}

void pvr2_yuv_init( uint32_t target )
{
    pvr2_yuv_state.target = target;
    pvr2_yuv_state.x = 0;
    pvr2_yuv_state.y = 0;
    pvr2_yuv_state.data_length = 0;
    MMIO_WRITE( PVR2, YUV_COUNT, 0 );
}

void pvr2_yuv_set_config( uint32_t config )
{
    pvr2_yuv_state.width = (config & 0x3f) + 1;
    pvr2_yuv_state.height = ((config>>8) & 0x3f) +1;
    pvr2_yuv_state.input_format = (config & 0x01000000) ? FORMAT_YUV422 : FORMAT_YUV420;
    if( config & 0x00010000 ) {
	pvr2_yuv_state.height *= pvr2_yuv_state.width;
	pvr2_yuv_state.width = 1;
    }
}

void pvr2_yuv_save_state( FILE * f )
{
    fwrite( &pvr2_yuv_state, sizeof(pvr2_yuv_state), 1, f );
}

int pvr2_yuv_load_state( FILE *f )
{
    if( fread( &pvr2_yuv_state, sizeof(pvr2_yuv_state), 1, f ) != 1 ) {
	return 1;
    } else {
	return 0;
    }
}
