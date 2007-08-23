.section .text
.include "sh4/inc.s"
!
! Test fmov (all variants)
! (not arithmetic)

.global _test_fmov
_test_fmov:	
	start_test

	xor r0,r0
	lds r0, fpscr
	
test_fmov_1:  ! single precision reg-to-reg
	add #1, r12

	fldi0 fr0
	fldi1 fr1
	flds fr0, fpul
	sts fpul, r0
	tst r0, r0
	bf test_fmov_1_fail
	fmov fr1, fr0
	flds fr0, fpul
	sts fpul, r0
	mov.l test_fmov_1_result, r1
	cmp/eq r0, r1
	bt test_fmov_2
test_fmov_1_fail:
	fail test_fmov_str_k
	bra test_fmov_2
	nop

test_fmov_1_result:
	.long 0x3F800000

test_fmov_2:	! reg-to-reg double prec
	add #1, r12
	mov.l test_fmov_2_input_a, r1
	lds r1, fpul
	fsts fpul, fr4
	mov.l test_fmov_2_input_b, r3
	lds r3, fpul
	fsts fpul, fr5
	fldi0 fr8
	fldi0 fr9
	fschg
	flds fr8, fpul
	sts fpul, r0
	tst r0, r0
	bf test_fmov_2_fail
	flds fr9, fpul
	sts fpul, r0
	tst r0, r0
	bf test_fmov_2_fail
	fmov fr4, fr8
	flds fr8, fpul
	sts fpul, r0
	flds fr9, fpul
	sts fpul, r2
	cmp/eq r0, r1
	bf test_fmov_2_fail
	cmp/eq r2, r3
	bt test_fmov_3
test_fmov_2_fail:
	fail test_fmov_str_k
	bra test_fmov_3
	nop
test_fmov_2_input_a:
	.long 0x12345678
test_fmov_2_input_b:
	.long 0x9ABCDEF0

test_fmov_3: ! double size DRm to XDn
	add #1, r12
	frchg
	fldi0 fr8
	fldi0 fr9
	frchg
	fldi0 fr8
	fldi0 fr9
	mov.l test_fmov_3_input_a, r2
	lds r2, fpul
	fsts fpul, fr2
	mov.l test_fmov_3_input_b, r3
	lds r3, fpul
	fsts fpul, fr3

	fmov fr2, fr9
	flds fr8, fpul
	sts fpul, r0
	flds fr9, fpul
	sts fpul, r1
	tst r0, r0
	bf test_fmov_3_fail
	tst r1, r1
	bf test_fmov_3_fail
	frchg
	flds fr8, fpul
	sts fpul, r0
	flds fr9, fpul
	sts fpul, r1
	cmp/eq r0, r2
	bf test_fmov_3_fail
	cmp/eq r1, r3
	bt test_fmov_4

test_fmov_3_fail:
	fail test_fmov_str_k
	bra test_fmov_4
	nop

test_fmov_3_input_a:
	.long 0x86421357
test_fmov_3_input_b:
	.long 0x97532468
	
test_fmov_4: ! double size XDm to DRn
	add #1, r12
	mov.l test_fmov_4_input_a, r2
	lds r2, fpul
	fsts fpul, fr6
	mov.l test_fmov_4_input_b, r3
	lds r3, fpul
	fsts fpul, fr7
	fldi0 fr0
	fldi0 fr1
	frchg
	fldi0 fr6
	fldi0 fr7

	fmov fr7, fr0
	flds fr0, fpul
	sts fpul, r0
	flds fr1, fpul
	sts fpul, r1
	cmp/eq r0, r2
	bf test_fmov_4_fail
	cmp/eq r1, r3
	bf test_fmov_4_fail
	frchg
	flds fr0, fpul
	sts fpul, r0
	flds fr1, fpul
	sts fpul, r1
	tst r0, r0
	bf test_fmov_4_fail
	tst r1, r1
	bt test_fmov_5
	
test_fmov_4_fail:
	fail test_fmov_str_k
	bra test_fmov_5
	nop

test_fmov_4_input_a:
	.long 0xACADACA0
test_fmov_4_input_b:
	.long 0x12233445
	

test_fmov_5: ! double size @Rm to DRn, DRm to @Rn
	add #1, r12
	mova test_fmov_5_data_a, r0
	mov r0, r4
	xor r1, r1
	mov.l r1, @r0
	add #4, r0
	mov.l r1, @r0
	mova test_fmov_5_input_a, r0
	fmov @r0, fr8
	mov.l test_fmov_5_input_a, r0
	mov.l test_fmov_5_input_b, r1
	flds fr8, fpul
	sts fpul, r5
	flds fr9, fpul
	sts fpul, r6
	cmp/eq r0, r5
	bf test_fmov_5_fail
	cmp/eq r1, r6
	bf test_fmov_5_fail
	fmov fr8, @r4
	mov.l test_fmov_5_data_a, r2
	mov.l test_fmov_5_data_b, r3
	cmp/eq r0, r2
	bf test_fmov_5_fail
	cmp/eq r1, r3
	bt test_fmov_6
test_fmov_5_fail:
	fail test_fmov_str_k
	bra test_fmov_6
	nop
	
test_fmov_5_input_a:
	.long 0xFEEDBEEF
test_fmov_5_input_b:
	.long 0xDEAD1234
test_fmov_5_data_a:
	.long 0
test_fmov_5_data_b:	
	.long 0

test_fmov_6:	! double size @Rm+ to DRn, DRm to @-Rn
	add #1, r12
	mova test_fmov_6_data_a, r0
	mov r0, r4
	xor r1, r1
	mov.l r1, @r4
	add #4, r4
	mov.l r1, @r4
	add #4, r4
	mova test_fmov_6_input_a, r0
	mov r0, r7
	fmov @r7+, fr10
	mov.l test_fmov_6_input_a, r0
	mov.l test_fmov_6_input_b, r1
	flds fr10, fpul
	sts fpul, r5
	flds fr11, fpul
	sts fpul, r6
	cmp/eq r0, r5
	bf test_fmov_6_fail
	cmp/eq r1, r6
	bf test_fmov_6_fail
	fmov fr10, @-r4
	mov.l test_fmov_6_data_a, r2
	mov.l test_fmov_6_data_b, r3
	cmp/eq r0, r2
	bf test_fmov_6_fail
	cmp/eq r1, r3
	bf test_fmov_6_fail
	mova test_fmov_6_data_a, r0
	cmp/eq r0, r4
	bf test_fmov_6_fail
	cmp/eq r0, r7
	bt test_fmov_7
test_fmov_6_fail:
	fail test_fmov_str_k
	bra test_fmov_7
	nop
	
test_fmov_6_input_a:
	.long 0x42318576
test_fmov_6_input_b:
	.long 0xF0AFD34F
test_fmov_6_data_a:
	.long 0
test_fmov_6_data_b:	
	.long 0
	
test_fmov_7:	! double size @Rm,@R0 to DRn, DRm to @Rn,@R0
	add #1, r12
	mova test_fmov_7_data_a, r0
	mov r0, r4
	xor r1, r1
	mov.l r1, @r4
	add #4, r4
	mov.l r1, @r4
	add #48, r4
	mova test_fmov_7_input_a, r0
	mov r0, r7
	xor r0, r0
	add #-31, r7
	add #31, r0
	fmov @(r0,r7), fr10
	mov.l test_fmov_7_input_a, r0
	mov.l test_fmov_7_input_b, r1
	flds fr10, fpul
	sts fpul, r5
	flds fr11, fpul
	sts fpul, r6
	cmp/eq r0, r5
	bf test_fmov_7_fail
	cmp/eq r1, r6
	bf test_fmov_7_fail
	xor r0, r0
	add #-52, r0
	fmov fr10, @(r0,r4)
	mov.l test_fmov_7_input_a, r0
	mov.l test_fmov_7_data_a, r2
	mov.l test_fmov_7_data_b, r3
	cmp/eq r0, r2
	bf test_fmov_7_fail
	cmp/eq r1, r3
	bf test_fmov_7_fail
	mova test_fmov_7_data_a, r0
	add #52, r0
	cmp/eq r0, r4
	bf test_fmov_7_fail
	mova test_fmov_7_input_a, r0
	add #-31, r0
	cmp/eq r0, r7
	bt test_fmov_8
test_fmov_7_fail:
	fail test_fmov_str_k
	bra test_fmov_8
	nop
	
test_fmov_7_input_a:
	.long 0xABBACADA
test_fmov_7_input_b:
	.long 0x43546576
test_fmov_7_data_a:
	.long 0
test_fmov_7_data_b:	
	.long 0

test_fmov_8:
	
test_fmov_end:
	xor r0, r0
	lds r0, fpscr
	end_test test_fmov_str_k
	
test_fmov_str:
	.string "FMOV"
	
.align 4
test_fmov_str_k:
	.long test_fmov_str
