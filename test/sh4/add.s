	.section .text
.global _test_add
_test_add:
	mov.l r14, @-r15
	sts.l pr, @-r15
	mov.l r12, @-r15
	mov.l r13, @-r15
	mov r15, r14
	xor r12,r12
	xor r13,r13
# r12 is the test counter
# r13 is the failed-test counter
	
test_add_1:	# test adding 0+0 = 0
	add #1, r12
	xor r0,r0
	xor r1,r1
	xor r2,r2
	add r0,r1
	cmp/eq r1, r2
	bt test_add_2
	add #1, r13
	
test_add_2:	# test 0+ constant 1 = 1
	add #1, r12
	xor r0, r0
	xor r1, r1
	add #1, r1
	mov.l test_add_2_result, r2
	cmp/eq r1, r2
	bt test_add_3
	add #1, r13
	bra test_add_3
	nop
	
	.align 4
test_add_2_result:
	.long 0x00000001
	
test_add_3:  # test 0 + constant -1 = -1
	add #1, r12
	xor r0, r0
	xor r1, r1
	add #-1, r1
	mov.l test_add_3_result, r2
	cmp/eq r1, r2
	bt test_add_4
	add #1, r13
	bra test_add_4
	nop

	.align 4
test_add_3_result:
	.long 0xFFFFFFFF
	
test_add_4:  # test a+b = c w/ overflow
	add #1, r12
	mov.l test_add_4_op1, r4
	mov.l test_add_4_op2, r5
	mov.l test_add_4_result, r0
	add r4, r5
	cmp/eq r5, r0
	bt test_add_5
	add #1, r13
	bra test_add_5
	nop

	.align 4
test_add_4_op1:
	.long 0x98765432
test_add_4_op2:
	.long 0xA1234567
test_add_4_result:
	.long 0x39999999

test_add_5:	# test carry neither used nor set (ala ADDC)
	add #1, r12
	mov.l test_add_5_op1, r8
	mov.l test_add_5_op2, r9
	stc sr, r10
	xor r0,r0
	add #1, r0
	or r0,r10
	ldc r10, sr
	add r9,r8
	mov.l test_add_5_result, r11
	cmp/eq r11, r8
	bt test_add_5_b
	add #1, r13
	mov.l test_print_failure_k, r3
	mov r12, r5
	mov.l test_add_str_k, r4
	jsr @r3
	nop
	bra test_add_6
	nop
test_add_5_b:
	stc sr, r1
	and r0, r1
	cmp/eq r0, r1
	bt test_add_6
	add #1, r13
	mov.l test_print_failure_k, r3
	mov r12, r5
	mov.l test_add_str_k, r4
	jsr @r3
	nop
	bra test_add_6
	nop
	
test_add_5_op1:
	.long 0x11111111
test_add_5_op2:	
	.long 0x1000FFFF
test_add_5_result:	
	.long 0x21121110
	
test_add_6:	
	
test_add_end:
	mov.l test_add_str_k, r4
	mov r13, r5
	mov r12, r6
	mov.l test_print_result_k, r1
	jsr @r1
	mov r14, r15
	mov.l @r15+, r13
	mov.l @r15+, r12
	lds.l @r15+, pr
	mov.l @r15+, r14
	rts
	nop

	.align 2
test_add_str:
	.string "ADD"
	.align 2
	
test_add_str_k:	
	.long test_add_str
test_print_result_k:
	.long _test_print_result
test_print_failure_k:
	.long _test_print_failure
	