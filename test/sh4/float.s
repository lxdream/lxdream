.section .text
.include "sh4/inc.s"
!
! Test float

.global _test_float
_test_float:	
	start_test

	xor r0,r0
	lds r0, fpscr
	
test_float_1:  ! Load 1.0 single precision
	add #1, r12
	add #1, r0
	fldi0 fr0
	fldi0 fr1
	lds r0, fpul
	float fpul, fr0
	sts fpul, r1
	cmp/eq r0, r1
	bf test_float_1_fail
	flds fr0, fpul
	sts fpul, r0
	mov.l test_float_1_result, r1
	cmp/eq r0,r1
	bf test_float_1_fail
	flds fr1, fpul
	sts fpul, r0
	tst r0, r0
	bt test_float_2
test_float_1_fail:	
	fail test_float_str_k
	bra test_float_2
	nop

.align 4
test_float_1_result:
	.long 0x3F800000

test_float_2:	! Load -1.0 double precision
	add #1, r12
	fldi0 fr0
	fldi1 fr1
	setpr
	xor r0,r0
	add #-1, r0
	lds r0, fpul
	float fpul, fr0
	sts fpul, r1
	cmp/eq r0, r1
	bf test_float_2_fail
	flds fr0, fpul
	sts fpul, r0
	flds fr1, fpul
	sts fpul, r2
	mov.l test_float_2_result_a, r1
	mov.l test_float_2_result_b, r3
	cmp/eq r0,r1
	bf test_float_2_fail
	cmp/eq r2,r3
	bt test_float_3
test_float_2_fail:	
	fail test_float_str_k
	bra test_float_3
	nop

test_float_2_result_a:
	.long 0xBFF00000
test_float_2_result_b:
	.long 0x00000000

test_float_3:   ! pr=0, sz=1
	add #1, r12
	clrpr
	fldi0 fr0
	fldi0 fr1
	fschg
	mov.l test_float_3_input, r0
	lds r0, fpul
	float fpul, fr0
	sts fpul, r1
	cmp/eq r0, r1
	bf test_float_3_fail
	flds fr0, fpul
	sts fpul, r0
	mov.l test_float_3_result, r1
	cmp/eq r0, r1
	bf test_float_3_fail
	flds fr1, fpul
	sts fpul, r0
	tst r0, r0
	bt test_float_4
test_float_3_fail:	
	fail test_float_str_k
	bra test_float_4
	nop
	
test_float_3_input:
	.long 0xCCCCCCCC
test_float_3_result:
	.long 0xCE4CCCCD

test_float_4:	! pr=1, sz=1
	add #1, r12
	fldi0 fr0
	fldi1 fr1
	setpr
	mov.l test_float_4_input, r0
	lds r0, fpul
	float fpul, fr0
	sts fpul, r1
	cmp/eq r0, r1
	bf test_float_4_fail
	flds fr0, fpul
	sts fpul, r0
	flds fr1, fpul
	sts fpul, r2
	mov.l test_float_4_result_a, r1
	mov.l test_float_4_result_b, r3
	cmp/eq r0,r1
	bf test_float_4_fail
	cmp/eq r2,r3
	bt test_float_5
test_float_4_fail:	
	fail test_float_str_k
	bra test_float_5
	nop

test_float_4_input:
	.long 0x7FFFFFFF
test_float_4_result_a:
	.long 0x41DFFFFF
test_float_4_result_b:
	.long 0xFFC00000


test_float_5:	! test w/ max +int, sz=0, pr=0, fr=1
	add #1, r12
	xor r0,r0
	lds r0, fpscr
	fldi0 fr0
	fldi0 fr1
	frchg
	fldi0 fr0
	fldi0 fr1
	mov.l test_float_5_input, r0
	lds r0, fpul
	float fpul, fr0
	sts fpul, r1
	cmp/eq r0, r1
	bf test_float_5_fail
	flds fr0, fpul
	sts fpul, r0
	mov.l test_float_5_result, r1
	cmp/eq r0, r1
	bf test_float_5_fail
	flds fr1, fpul
	sts fpul, r0
	tst r0, r0
	bf test_float_5_fail
	lds r0, fpscr
	flds fr0, fpul
	sts fpul, r0
	tst r0, r0
	bt test_float_6
test_float_5_fail:
	fail test_float_str_k
	bra test_float_6
	nop
	
test_float_5_input:
	.long 0x7FFFFFFF
test_float_5_result:
	.long 0x4F000000

test_float_6: ! Test max -int
	add #1, r12
	mov.l test_float_6_input, r0
	lds r0, fpul
	float fpul, fr5
	sts fpul, r1
	cmp/eq r0, r1
	bf test_float_6_fail
	flds fr5, fpul
	sts fpul, r2
	mov.l test_float_6_result, r1
	cmp/eq r1, r2
	bt test_float_end

test_float_6_fail:
	fail test_float_str_k
	bra test_float_end
	nop
	
test_float_6_input:
	.long 0x80000000
test_float_6_result:
	.long 0xCF000000
	
test_float_end:
	end_test test_float_str_k
	
test_float_str:
	.string "FLOAT"
	
.align 4
test_float_str_k:
	.long test_float_str