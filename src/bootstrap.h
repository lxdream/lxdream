/*
 * IP.BIN related code. Ref: http://mc.pp.se/dc/ip0000.bin.html
 */
#ifndef dc_ipbin_H
#define dc_ipbin_H 1

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

#define BOOTSTRAP_LOAD_ADDR 0x8C008000
#define BOOTSTRAP_SIZE 32768

typedef struct dc_bootstrap_head {
    char hardware_id[16]; /* must be "SEGA SEGAKATANA " */ 
    char maker_id[16];    /* ditto,  "SEGA ENTERPRISES" */
    char crc[4];
    char padding;         /* normally ascii space */
    char gdrom_id[6];
    char disc_no[5];
    char regions[8];
    char peripherals[8];
    char product_id[10];
    char product_ver[6];
    char product_date[16];
    char boot_file[16];
    char vendor_id[16];
    char product_name[128];
} *dc_bootstrap_head_t;

/* Expansion units */
#define DC_PERIPH_WINCE    0x0000001
#define DC_PERIPH_VGABOX   0x0000010
#define DC_PERIPH_OTHER    0x0000100
#define DC_PERIPH_PURUPURU 0x0000200
#define DC_PERIPH_MIKE     0x0000400
#define DC_PERIPH_MEMCARD  0x0000800
/* Basic requirements */
#define DC_PERIPH_BASIC    0x0001000 /* Basic controls - start, a, b, arrows */
#define DC_PERIPH_C_BUTTON 0x0002000
#define DC_PERIPH_D_BUTTON 0x0004000
#define DC_PERIPH_X_BUTTON 0x0008000
#define DC_PERIPH_Y_BUTTON 0x0010000
#define DC_PERIPH_Z_BUTTON 0x0020000
#define DC_PERIPH_EXP_DIR  0x0040000 /* Expanded direction buttons */
#define DC_PERIPH_ANALOG_R 0x0080000 /* Analog R trigger */
#define DC_PERIPH_ANALOG_L 0x0100000 /* Analog L trigger */
#define DC_PERIPH_ANALOG_H 0x0200000 /* Analog horizontal controller */
#define DC_PERIPH_ANALOG_V 0x0400000 /* Analog vertical controller */
#define DC_PERIPH_EXP_AH   0x0800000 /* Expanded analog horizontal (?) */
#define DC_PERIPH_EXP_AV   0x1000000 /* Expanded analog vertical (?) */
/* Optional peripherals */
#define DC_PERIPH_GUN      0x2000000
#define DC_PERIPH_KEYBOARD 0x4000000
#define DC_PERIPH_MOUSE    0x8000000

void parse_ipbin(char *data);

#ifdef __cplusplus
}
#endif
#endif
