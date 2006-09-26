.section .text
.include "sh4/inc.s"

.global _test_add
_test_add:
	start_test
	
test_add_1:	! test adding 0+0 = 0
	add #1, r12
	xor r0,r0
	xor r1,r1
	xor r2,r2
	add r0,r1
	cmp/eq r1, r2
	bt test_add_2
	fail test_add_str_k
	
test_add_2:	! test 0+ constant 1 = 1
	add #1, r12
	xor r0, r0
	xor r1, r1
	add #1, r1
	mov.l test_add_2_result, r2
	cmp/eq r1, r2
	bt test_add_3
	fail test_add_str_k
	bra test_add_3
	nop
	
	.align 4
test_add_2_result:
	.long 0x00000001
	
test_add_3:	! test 0 + constant -1 = -1
	add #1, r12
	xor r0, r0
	xor r1, r1
	add #-1, r1
	mov.l test_add_3_result, r2
	cmp/eq r1, r2
	bt test_add_4
	fail test_add_str_k
	bra test_add_4
	nop

	.align 4
test_add_3_result:
	.long 0xFFFFFFFF
	
test_add_4:	! test a+b = c w/ overflow
	add #1, r12
	mov.l test_add_4_op1, r4
	mov.l test_add_4_op2, r5
	mov.l test_add_4_result, r0
	add r4, r5
	cmp/eq r5, r0
	bt test_add_5
	fail test_add_str_k
	bra test_add_5
	nop

	.align 4
test_add_4_op1:
	.long 0x98765432
test_add_4_op2:
	.long 0xA1234567
test_add_4_result:
	.long 0x39999999

test_add_5:	! test carry neither used nor set (ala ADDC)
	add #1, r12
	mov.l test_add_5_op1, r4
	mov.l test_add_5_op2, r5
	stc sr, r6
	xor r0,r0
	add #1, r0
	stc sr, r1
	or r0,r6
	ldc r6, sr
	add r5,r4
	mov.l test_add_5_result, r7
	cmp/eq r7, r4
	bt test_add_5_b
	fail test_add_str_k
	bra test_add_6
	nop
test_add_5_b:
	and r0, r1
	cmp/eq r0, r1
	bt test_add_6
	fail test_add_str_k
	bra test_add_6
	nop
	
test_add_5_op1:
	.long 0x11111111
test_add_5_op2:	
	.long 0x1000FFFF
test_add_5_result:	
	.long 0x21121110
	
test_add_6:	! test maximum negative immediate
	add #1, r12
	xor r0,r0
	add #128, r0
	mov.l test_add_6_result, r1
	cmp/eq r0, r1
	bt test_add_7
	fail test_add_str_k
	bra test_add_7
	nop
test_add_6_result:
	.long 0xFFFFFF80

test_add_7:	! test maximum positive immediate
	add #1, r12
	xor r0,r0
	add #127, r0
	mov.l test_add_7_result, r1
	cmp/eq r0, r1
	bt test_add_8
	fail test_add_str_k
	bra test_add_8
	nop
test_add_7_result:
	.long 0x0000007F

test_add_8:	! Test example from manual
	add #1, r12
	mov.l test_add_8_op1, r3
	add #-2, R3
	mov.l test_add_8_result, r1
	cmp/eq r3,r1
	bt test_add_end
	fail test_add_str_k
	bra test_add_end
	nop
test_add_8_op1:
	.long 0x00000001
test_add_8_result:
	.long 0xFFFFFFFF
	
test_add_end:
	end_test test_add_str_k

test_add_str:
	.string "ADD"

.align 4	
test_add_str_k:	
	.long test_add_str
	