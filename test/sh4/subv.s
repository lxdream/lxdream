.section .text
.include "sh4/inc.s"

.global _test_subv
_test_subv:
	start_test
	mov.l r11, @-r15
	mov.l test_subv_data_k, r11
	
test_subv_loop:
	mov.l test_subv_data_end_k, r4
	cmp/eq r11, r4
	bt test_subv_end
	add #1, r12

	clc
	mov.l @r11+, r0
	mov.l @r11+, r1
	subv r0, r1
	stc sr, r4
	mov.l @r11+, r2
	mov.l @r11+, r3
	cmp/eq r1, r2
	bt test_subv_b
	fail test_subv_str_k
	bra test_subv_loop
	nop
test_subv_b:
	ldc r4, sr
	xor r0, r0
	add #1, r0
	and r0, r4
	cmp/eq r3, r4
	bt test_subv_loop
	fail test_subv_str_k
	bra test_subv_loop
	nop

test_subv_end:
	mov.l @r15+, r11
	end_test test_subv_str_k

	.align 4	
test_subv_data_k:
	.long test_subv_data
test_subv_data:
test_subv_data_1:
	.long 0x00000000
	.long 0x00000000
	.long 0x00000000
	.long 0x00000000

	.long 0x0000000F
	.long 0x7FFFFFFF
	.long 0x7FFFFFF0
	.long 0x00000000
	
	.long 0x0000000F
	.long 0xFFFFFFFF
	.long 0xFFFFFFF0
	.long 0x00000000

	.long 0xFFFFFFF0
	.long 0xFFFFFFE0
	.long 0xFFFFFFF0
	.long 0x00000000

	.long 0x00000040
	.long 0x00000020
	.long 0xFFFFFFE0
	.long 0x00000000

	.long 0xFFFFFF80
	.long 0xFFFFFFB2
	.long 0x00000032
	.long 0x00000000

	.long 0x00000001
	.long 0x80000000
	.long 0x7FFFFFFF
	.long 0x00000001

	.long 0xFFFFFFFF
	.long 0x7FFFFFFF
	.long 0x80000000
	.long 0x00000001

	.long 0x98765432
	.long 0x30ECA864
	.long 0x98765432
	.long 0x00000001

	.long 0x43214321
	.long 0x86428642
	.long 0x43214321
	.long 0x00000001
	
test_subv_data_end:	
	.align 4
test_subv_data_end_k:
	.long test_subv_data_end	
test_subv_str_k:
	.long test_subv_str
test_subv_str:
	.string "SUBV"
	