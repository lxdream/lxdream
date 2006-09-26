.section .text
.include "sh4/inc.s"
!
! Test AND Rm,Rn operation
!
.global _test_and
_test_and:
	start_test
	mov.l r11, @-r15
	mov.l test_and_data_k, r11

test_and_loop:
	mov.l test_and_data_end_k, r4
	cmp/eq r11, r4
	bt test_and_end
	add #1, r12

	clc
	mov.l @r11+, r0
	mov.l @r11+, r1
	and r0, r1
	stc sr, r4
	mov.l @r11+, r2
	mov.l @r11+, r3
	cmp/eq r1, r2
	bt test_and_b
	fail test_and_str_k
	bra test_and_loop
	nop
test_and_b:
	ldc r4, sr
	xor r0, r0
	add #1, r0
	and r0, r4
	cmp/eq r3, r4
	bt test_and_loop
	fail test_and_str_k
	bra test_and_loop
	nop

test_and_end:
	mov.l @r15+, r11
	end_test test_and_str_k

	.align 4	
test_and_data_k:
	.long test_and_data
test_and_data:
test_and_data_1:
	.long 0xFFFFFFFF
	.long 0x00000000
	.long 0x00000000
	.long 0x00000000

	.long 0x55555555
	.long 0xAAAAAAAA
	.long 0x00000000
	.long 0x00000000

	.long 0xFFFFFFFF
	.long 0xA5A5A5A5
	.long 0xA5A5A5A5
	.long 0x00000000

	.long 0xFFFFFFFF
	.long 0xFFFFFFFF
	.long 0xFFFFFFFF
	.long 0x00000000

	.long 0x12345678
	.long 0x98765432
	.long 0x10345430
	.long 0x00000000

	.long 0x00FFFFFF
	.long 0x98765432
	.long 0x00765432
	.long 0x00000000
	
test_and_data_end:	
	.align 4
test_and_data_end_k:
	.long test_and_data_end	
test_and_str_k:
	.long test_and_str
test_and_str:
	.string "AND"
	