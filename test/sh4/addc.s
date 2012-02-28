.section .text
.include "sh4/inc.s"

.global _test_addc
_test_addc:
	start_test

test_addc_1:	! test adding 0+0 = 0
	clc
	add #1, r12
	xor r0,r0
	xor r1,r1
	xor r2,r2
	addc r0,r1
	stc sr, r4
	cmp/eq r1, r2
	bt test_addc_1_b
	fail test_addc_str_k
	bra test_addc_2
	nop
test_addc_1_b:
	ldc r4, sr
	assert_t_clear test_addc_str_k
	
test_addc_2:	! test 0+ constant 1 = 1
	add #1, r12
	clc
	xor r0, r0
	xor r1, r1
	add #1, r0
	addc r0, r1
	stc sr, r4
	mov.l test_addc_2_result, r2
	cmp/eq r1, r2
	bt test_addc_2_b
	fail test_addc_str_k
	bra test_addc_3
	nop
test_addc_2_b:
	ldc r4, sr
	assert_t_clear test_addc_str_k
	bra test_addc_3
	nop
	
	.align 4
test_addc_2_result:
	.long 0x00000001
	
test_addc_3:	! test 0 + constant -1 = -1
	add #1, r12
	clc
	xor r0, r0
	xor r1, r1
	add #-1, r0
	addc r0, r1
	mov.l test_addc_3_result, r2
	stc sr, r3
	cmp/eq r1, r2
	bt test_addc_3_b
	fail test_addc_str_k
	bra test_addc_4
	nop
test_addc_3_b:	
	ldc r3, sr
	assert_t_clear test_addc_str_k
	bra test_addc_4
	nop
	
	.align 4
test_addc_3_result:
	.long 0xFFFFFFFF
	
test_addc_4:	! test a+b = c w/ carry set
	add #1, r12
	clc
	mov.l test_addc_4_op1, r4
	mov.l test_addc_4_op2, r5
	mov.l test_addc_4_result, r0
	addc r4, r5
	stc sr, r1
	cmp/eq r5, r0
	bt test_addc_4_b
	fail test_addc_str_k
	bra test_addc_5
	nop
test_addc_4_b:
	ldc r1, sr
	assert_t_set test_addc_str_k
	bra test_addc_5
	nop

	.align 4
test_addc_4_op1:
	.long 0x98765432
test_addc_4_op2:
	.long 0xA1234567
test_addc_4_result:
	.long 0x39999999

test_addc_5:	! test carry used and cleared
	add #1, r12
	mov.l test_addc_5_op1, r4
	mov.l test_addc_5_op2, r5
	stc sr, r6
	xor r0,r0
	add #1, r0
	or r0,r6
	ldc r6, sr
	addc r5,r4
	stc sr, r1
	mov.l test_addc_5_result, r7
	cmp/eq r7, r4
	bt test_addc_5_b
	fail test_addc_str_k
	bra test_addc_6
	nop
test_addc_5_b:
	ldc r1, sr
	assert_t_clear test_addc_str_k
	bra test_addc_6
	nop
	
test_addc_5_op1:
	.long 0x11111111
test_addc_5_op2:	
	.long 0x1000FFFF
test_addc_5_result:	
	.long 0x21121111
	
test_addc_6: ! test carry set on full rollover (ie n + 0xFFFFFFFF + carry )
	add #1, r12
	setc
	mov.l test_addc_6_op1, r5
	mov.l test_addc_6_op2, r6
	addc r5, r6
	stc sr, r1
	cmp/eq r5, r6
	bt test_addc_6_b
	fail test_addc_str_k
	bra test_addc_7
	nop
test_addc_6_b:
	ldc r1, sr
	assert_t_set test_addc_str_k
	bra test_addc_7	
	nop
		
test_addc_6_op1:
	.long 0x12346789
test_addc_6_op2:
	.long 0xFFFFFFFF

	
test_addc_7:
	add #1, r12
	clc
	mov.l test_addc_7_op1, r5
	mov.l test_addc_7_op2, r6
	addc r5, r6
	stc sr, r1
	mov.l test_addc_7_result, r2
	cmp/eq r2, r6
	bt test_addc_7_b
	fail test_addc_str_k
	bra test_addc_8
	nop
test_addc_7_b:
	ldc r1, sr
	assert_t_set test_addc_str_k
	bra test_addc_8	
	nop
		
test_addc_7_op1:
	.long 0x98765432
test_addc_7_op2:
	.long 0xFFFFFFFF
test_addc_7_result:	
	.long 0x98765431	

test_addc_8:
	add #1, r12
	setc
	xor r0,r0
	addc r0, r0
	stc sr, r3
	mov.l test_addc_8_result, r1
	cmp/eq r0, r1
	bt test_addc_8_b
	fail test_addc_str_k
	bra test_addc_9
	nop
test_addc_8_b:
	ldc r3, sr
	assert_t_clear test_addc_str_k
	bra test_addc_9
	nop

test_addc_8_result:
	.long 0x00000001

test_addc_9:	
test_addc_end:
	end_test test_addc_str_k

test_addc_str:
	.string "ADDC"

.align 4	
test_addc_str_k:	
	.long test_addc_str
