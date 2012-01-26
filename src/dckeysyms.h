/**
 * $Id$
 *
 * Keysym definitions for the dreamcast keyboard.
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

#ifndef lxdream_dckeysyms_H
#define lxdream_dckeysyms_H 1


#define DCKB_NONE       0x00
#define DCKB_ERROR      0x01
#define DCKB_a          0x04
#define DCKB_b          0x05
#define DCKB_c          0x06
#define DCKB_d          0x07
#define DCKB_e          0x08
#define DCKB_f          0x09
#define DCKB_g          0x0A
#define DCKB_h          0x0B
#define DCKB_i          0x0C
#define DCKB_j          0x0D
#define DCKB_k          0x0E
#define DCKB_l          0x0F
#define DCKB_m          0x10
#define DCKB_n          0x11
#define DCKB_o          0x12
#define DCKB_p          0x13
#define DCKB_q          0x14
#define DCKB_r          0x15
#define DCKB_s          0x16
#define DCKB_t          0x17
#define DCKB_u          0x18
#define DCKB_v          0x19
#define DCKB_w          0x1A
#define DCKB_x          0x1B
#define DCKB_y          0x1C
#define DCKB_z          0x1D
#define DCKB_1          0x1E
#define DCKB_2          0x1F
#define DCKB_3          0x20
#define DCKB_4          0x21
#define DCKB_5          0x22
#define DCKB_6          0x23
#define DCKB_7          0x24
#define DCKB_8          0x25
#define DCKB_9          0x26
#define DCKB_0          0x27
#define DCKB_Return     0x28
#define DCKB_Escape     0x29
#define DCKB_BackSpace  0x2A
#define DCKB_Tab        0x2B
#define DCKB_space      0x2C
#define DCKB_minus      0x2D
#define DCKB_equal      0x2E
#define DCKB_bracketleft 0x2F
#define DCKB_bracketright 0x30
#define DCKB_backslash  0x31
#define DCKB_semicolon  0x33
#define DCKB_apostrophe 0x34
#define DCKB_grave      0x35
#define DCKB_comma      0x36
#define DCKB_period     0x37
#define DCKB_slash      0x38
#define DCKB_Caps_Lock  0x39
#define DCKB_F1         0x3A
#define DCKB_F2         0x3B
#define DCKB_F3         0x3C
#define DCKB_F4         0x3D
#define DCKB_F5         0x3E
#define DCKB_F6         0x3F
#define DCKB_F7         0x40
#define DCKB_F8         0x41
#define DCKB_F9         0x42
#define DCKB_F10        0x43
#define DCKB_F11        0x44
#define DCKB_F12        0x45
#define DCKB_Print_Screen 0x46
#define DCKB_Scroll_Lock 0x47
#define DCKB_Pause      0x48
#define DCKB_Insert     0x49
#define DCKB_Home       0x4A
#define DCKB_Page_Up    0x4B
#define DCKB_Delete     0x4C
#define DCKB_End        0x4D
#define DCKB_Page_Down  0x4E
#define DCKB_Right      0x4F
#define DCKB_Left       0x50
#define DCKB_Down       0x51
#define DCKB_Up         0x52
#define DCKB_Num_Lock   0x53
#define DCKB_KP_Divide  0x54
#define DCKB_KP_Multiply 0x55
#define DCKB_KP_Subtract 0x56
#define DCKB_KP_Add     0x57
#define DCKB_KP_Enter   0x58
#define DCKB_KP_End     0x59
#define DCKB_KP_Down    0x5A
#define DCKB_KP_Page_Down 0x5B
#define DCKB_KP_Left    0x5C
#define DCKB_KP_Begin   0x5D
#define DCKB_KP_Right   0x5E
#define DCKB_KP_Home    0x5F
#define DCKB_KP_Up      0x60
#define DCKB_KP_Page_Up 0x61
#define DCKB_KP_Insert  0x62
#define DCKB_KP_Delete  0x63
#define DCKB_S3         0x65

/* Modifier keys */

#define DCKB_Control_L  0xFF01
#define DCKB_Shift_L    0xFF02
#define DCKB_Alt_L      0xFF04
#define DCKB_Meta_L     0xFF08 /* S1 */
#define DCKB_Control_R  0xFF10
#define DCKB_Shift_R    0xFF20
#define DCKB_Alt_R      0xFF40
#define DCKB_Meta_R     0xFF80 /* S2 */

#endif /* !lxdream_dckeysyms_H */
