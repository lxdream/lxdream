.section .text
.include "sh4/inc.s"

.global _test_sub
_test_sub:
	start_test
	
test_sub_1:	! test subing 0+0 = 0
	add #1, r12
	xor r0,r0
	xor r1,r1
	xor r2,r2
	sett
	sub r0,r1
	bf test_sub_1_fail
	cmp/eq r1, r2
	bt test_sub_2
test_sub_1_fail:	
	fail test_sub_str_k
	
test_sub_2:	! test 0+ constant 1 = 1
	add #1, r12
	xor r0, r0
	xor r1, r1
	mov.l test_sub_2_input, r0
	mov.l test_sub_2_result, r2
	clrt
	sub r0, r1
	bt test_sub_2_fail
	cmp/eq r1, r2
	bt test_sub_3
test_sub_2_fail:	
	fail test_sub_str_k
	bra test_sub_3
	nop
	
	.align 4
test_sub_2_input:
	.long 0x00000001
test_sub_2_result:
	.long 0xFFFFFFFF
	
test_sub_3:	! test 0 + constant -1 = -1
	add #1, r12
	mov.l test_sub_3_input_1, r0
	mov r0, r1
	mov.l test_sub_3_input_2, r1
	sett
	sub r0, r1
	bf test_sub_3_fail
	mov.l test_sub_3_result, r2
	cmp/eq r1, r2
	bt test_sub_4
test_sub_3_fail:	
	fail test_sub_str_k
	bra test_sub_4
	nop

	.align 4
test_sub_3_input_1:
	.long 0xFFFFFF84
test_sub_3_input_2:	
	.long 0x43217000
test_sub_3_result:
	.long 0x4321707C
	
test_sub_4:	! Test 0 result
	add #1, r12
	mov.l test_sub_4_op1, r2
	mov.l test_sub_4_op2, r1
	mov r2, r3
	sub r1, r3
	mov.l test_sub_4_result_1, r0
	cmp/eq r0, r3
	bf test_sub_4_fail
	mov r2, r3
	sub r3, r1
	mov.l test_sub_4_result_2, r2
	cmp/eq r1, r2
	bt test_sub_5
test_sub_4_fail:	
	fail test_sub_str_k
	bra test_sub_5
	nop
test_sub_4_op1:
	.long 0x00000001
test_sub_4_op2:
	.long 0xFFFFFFFF
test_sub_4_result_1:
	.long 0x00000002
test_sub_4_result_2:
	.long 0xFFFFFFFE

test_sub_5:
	add #1, r12
	mov.l test_sub_5_op, r2
	sett
	sub r2, r2
	bf test_sub_5_fail
	tst r2, r2
	bt test_sub_end
test_sub_5_fail:	
	fail test_sub_str_k
	bra test_sub_end
	nop
	
test_sub_5_op:	
	.long 0xABCD1234	
	
test_sub_end:
	end_test test_sub_str_k

test_sub_str:
	.string "SUB"

.align 4	
test_sub_str_k:	
	.long test_sub_str
	