.section .text
.include "sh4/inc.s"
!

.global _test_bsr
_test_bsr:	
	start_test
	mov.l r11, @-r15
	
test_bsr_1:	! Ordinary case
	add #1, r12
	sts pr, r11
	mova test_bsr_1_fail, r0
	mov r0, r5
	xor r1, r1
	bsr test_bsr_1_target
	add #1, r1
test_bsr_1_fail:	
	fail test_bsr_str_k
	bra test_bsr_2
	nop
test_bsr_1_target:
	sts pr, r3
	cmp/eq r5,r3
	bf test_bsr_1_fail
	mov #1, r2
	cmp/eq r1, r2
	bf test_bsr_1_fail
	
test_bsr_2:	! Write PR in delay slot
	add #1, r12
	
	bsr test_bsr_2_target
	lds r11, pr
test_bsr_2_fail:
	fail test_bsr_str_k
	bra test_bsr_3
	nop
test_bsr_2_target:
	sts pr, r4
	cmp/eq r4,r11
	bf test_bsr_2_fail

test_bsr_3:	! Read PR in delay slot
	add #1, r12

	mova test_bsr_3_fail, r0
	mov r0, r5
	bsr test_bsr_3_target
	sts pr, r2
test_bsr_3_fail:
	fail test_bsr_str_k
	bra test_bsr_4
test_bsr_3_target:
	cmp/eq r2, r5
	bf test_bsr_3_fail
	sts pr, r3
	cmp/eq r3, r5
	bf test_bsr_3_fail

test_bsr_4:	! Exception in delay slot
	add #1, r12

	mova test_bsr_4_fail, r0
	add #1, r0
	expect_exc 0x000001A0
	mova test_bsr_4_rte, r0
	mov r0, r11
test_bsr_4_fault_pc:	
	bsr test_bsr_4_fail
	bt test_bsr_4_rte
test_bsr_4_rte:	
	sts pr, r1
	cmp/eq r1, r11
	bf test_bsr_4_fail
	assert_exc_caught test_bsr_str_k test_bsr_4_fault_pc
	bra test_bsr_5
	nop
test_bsr_4_fail:
	fail test_bsr_str_k

test_bsr_5:	
	
test_bsr_end:
	mov.l @r15+, r11
	end_test test_bsr_str_k

! Branch point used for tests that should never be reached (under correct
! operation. Returns immediately, which should hit the "no exception" test
test_bsr_fail:
	rts
	nop

test_bsr_str_k:
	.long test_bsr_str
test_bsr_str:
	.string "BSR"

	