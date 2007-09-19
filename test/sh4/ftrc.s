	.section .text
.include "sh4/inc.s"
!
! Test ftrc

.global _test_ftrc
_test_ftrc:	
	start_test

	xor r0,r0
	lds r0, fpscr
	
test_ftrc_1:  ! Load 1.0 single precision
	add #1, r12
	mov.l test_ftrc_1_input, r0
	lds r0, fpul
	fsts fpul, fr0
	ftrc fr0, fpul
	sts fpul, r0
	mov.l test_ftrc_1_result, r1
	cmp/eq r0,r1
	bt test_ftrc_2
test_ftrc_1_fail:	
	fail test_ftrc_str_k
	bra test_ftrc_2
	nop

.align 4
test_ftrc_1_input:
	.long 0x3F800000
test_ftrc_1_result:
	.long 0x00000001

test_ftrc_2:	! Load -1.0 double precision
	add #1, r12
	setpr
	mov.l test_ftrc_2_input_a, r0
	lds r0, fpul
	fsts fpul, fr0
	mov.l test_ftrc_2_input_b, r0
	lds r0, fpul
	fsts fpul, fr1
	ftrc fr0, fpul
	sts fpul, r0
	mov.l test_ftrc_2_result, r1
	cmp/eq r0,r1
	bt test_ftrc_3
test_ftrc_2_fail:	
	fail test_ftrc_str_k
	bra test_ftrc_3
	nop

test_ftrc_2_input_a:
	.long 0xBFF00000
test_ftrc_2_input_b:
	.long 0x00000000
test_ftrc_2_result:
	.long 0xFFFFFFFF

test_ftrc_3:   ! pr=0, sz=1
	add #1, r12
	clrpr
	mov.l test_ftrc_3_input, r0
	lds r0, fpul
	fsts fpul, fr0
	fschg
	ftrc fr0, fpul
	sts fpul, r0
	mov.l test_ftrc_3_result, r1
	cmp/eq r0, r1
	bt test_ftrc_4
test_ftrc_3_fail:	
	fail test_ftrc_str_k
	bra test_ftrc_4
	nop
	
test_ftrc_3_input:
	.long 0xCE4CCCCD
test_ftrc_3_result:
	.long 0xCCCCCCC0

test_ftrc_4:	! pr=1, sz=1
	add #1, r12
	fldi0 fr0
	fldi1 fr1
	setpr
	mov.l test_ftrc_4_input_a, r0
	lds r0, fpul
	fsts fpul, fr0
	mov.l test_ftrc_4_input_b, r0
	lds r0, fpul
	fsts fpul, fr1
	ftrc fr0, fpul
	sts fpul, r0
	mov.l test_ftrc_4_result, r1
	cmp/eq r0,r1
	bt test_ftrc_5
test_ftrc_4_fail:	
	fail test_ftrc_str_k
	bra test_ftrc_5
	nop

test_ftrc_4_input_a:
	.long 0x40FFFF11
test_ftrc_4_input_b:
	.long 0x11111111
test_ftrc_4_result:
	.long 0x0001FFF1


test_ftrc_5:	! test w/ max +int, sz=0, pr=0, fr=1
	add #1, r12
	xor r0,r0
	lds r0, fpscr
	fldi0 fr0
	fldi0 fr1
	frchg
	fldi0 fr0
	fldi0 fr1
	mov.l test_ftrc_5_input, r0
	lds r0, fpul
	fsts fpul, fr0
	ftrc fr0, fpul
	sts fpul, r0
	mov.l test_ftrc_5_result, r1
	cmp/eq r0, r1
	bf test_ftrc_5_fail
	flds fr1, fpul
	sts fpul, r0
	tst r0, r0
	bf test_ftrc_5_fail
	lds r0, fpscr
	flds fr0, fpul
	sts fpul, r0
	tst r0, r0
	bt test_ftrc_6
test_ftrc_5_fail:
	fail test_ftrc_str_k
	bra test_ftrc_6
	nop
	
test_ftrc_5_input:
	.long 0x4F000000
test_ftrc_5_result:
	.long 0x7FFFFFFF

test_ftrc_6: ! Test max -int
	add #1, r12
	mov.l test_ftrc_6_input, r0
	lds r0, fpul
	fsts fpul, fr5
	ftrc fr5, fpul
	sts fpul, r2
	mov.l test_ftrc_6_result, r1
	cmp/eq r1, r2
	bt test_ftrc_7

test_ftrc_6_fail:
	fail test_ftrc_str_k
	bra test_ftrc_7
	nop
	
test_ftrc_6_input:
	.long 0xCF000000
test_ftrc_6_result:
	.long 0x80000000

test_ftrc_7:	! Test >max +int
	add #1, r12
	mov.l test_ftrc_7_input, r0
	lds r0, fpul
	fsts fpul, fr7
	ftrc fr7, fpul
	sts fpul, r2
	mov.l test_ftrc_7_result, r1
	cmp/eq r1, r2
	bt test_ftrc_8
test_ftrc_7_fail:
	fail test_ftrc_str_k
	bra test_ftrc_8
	nop
	
test_ftrc_7_input:
	.long 0x7E111111
test_ftrc_7_result:
	.long 0x7FFFFFFF

test_ftrc_8: ! test < min -int
	add #1, r12
	mov.l test_ftrc_8_input, r0
	lds r0, fpul
	fsts fpul, fr9
	ftrc fr9, fpul
	sts fpul, r2
	mov.l test_ftrc_8_result, r1
	cmp/eq r1, r2
	bt test_ftrc_9
test_ftrc_8_fail:
	fail test_ftrc_str_k
	bra test_ftrc_9
	nop
	
test_ftrc_8_input:
	.long 0xFE111111
test_ftrc_8_result:
	.long 0x80000000

test_ftrc_9:	! Test >max +int pr=1
	add #1, r12
	setpr
	mov.l test_ftrc_9_input_a, r0
	lds r0, fpul
	fsts fpul, fr6
	mov.l test_ftrc_9_input_b, r0
	lds r0, fpul
	fsts fpul, fr7
	ftrc fr6, fpul
	sts fpul, r2
	mov.l test_ftrc_9_result, r1
	cmp/eq r1, r2
	bt test_ftrc_10
test_ftrc_9_fail:
	fail test_ftrc_str_k
	bra test_ftrc_10
	nop
	
test_ftrc_9_input_a:
	.long 0x41DFFFFF
test_ftrc_9_input_b:
	.long 0xFFC00000
test_ftrc_9_result:
	.long 0x7FFFFFFF

test_ftrc_10: ! test < min -int
	add #1, r12
	mov.l test_ftrc_10_input_a, r0
	lds r0, fpul
	fsts fpul, fr8
	mov.l test_ftrc_10_input_b, r0
	lds r0, fpul
	fsts fpul, fr9
	ftrc fr8, fpul
	sts fpul, r2
	mov.l test_ftrc_10_result, r1
	cmp/eq r1, r2
	bt test_ftrc_11
test_ftrc_10_fail:
	fail test_ftrc_str_k
	bra test_ftrc_11
	nop
	
test_ftrc_10_input_a:
	.long 0xFE111111
test_ftrc_10_input_b:
	.long 0x11111111
test_ftrc_10_result:
	.long 0x80000000

test_ftrc_11: ! test undefined instruction, pr=1
	add #1, r12
	mov.l test_ftrc_11_input_a, r0
	lds r0, fpul
	fsts fpul, fr0
	mov.l test_ftrc_11_input_b, r1
	lds r1, fpul
	fsts fpul, fr1
	mov.l test_ftrc_11_input_c, r0
	lds r0, fpul
	fsts fpul, fr2
	xor r0, r0
	not r0, r0
	lds r0, fpul
	ftrc fr1, fpul 
	sts fpul, r1
	mov.l test_ftrc_11_result, r2
	cmp/eq r1, r2
	bt test_ftrc_12
test_ftrc_11_fail:
	fail test_ftrc_str_k
	bra test_ftrc_12
	nop
test_ftrc_11_input_a:
	.long 0x40FFFF11
test_ftrc_11_input_b:
	.long 0x11111111
test_ftrc_11_input_c:
	.long 0x42FFFF11
test_ftrc_11_result:
	.long 0x00000000

test_ftrc_12:   ! single precision numeric tests (rounding)
	mov.l r11, @-r15
	mov.l r10, @-r15
	mova test_ftrc_12_data, r0
	mov r0, r10
	mov #4, r11
	clrpr
test_ftrc_12_loop:	
	add #1, r12
	fmov @r10+, fr5
	ftrc fr5, fpul
	sts fpul, r4
	mov.l @r10+, r5
	cmp/eq r4, r5
	bt test_ftrc_12_ok
	fail test_ftrc_str_k
test_ftrc_12_ok:
	dt r11
	bf test_ftrc_12_loop
	bra test_ftrc_end
	nop
test_ftrc_12_data:
	.long 0x449a5314
	.long 0x000004D2
	.long 0xC5A9C785
	.long 0xFFFFEAC8
	.long 0x49098291
	.long 0x00089829
	.long 0xC2DA999A
	.long 0xFFFFFF93
	
test_ftrc_end:
	mov.l @r15+, r10
	mov.l @r15+, r11
	end_test test_ftrc_str_k
	
test_ftrc_str:
	.string "FTRC"
	
.align 4
test_ftrc_str_k:
	.long test_ftrc_str
