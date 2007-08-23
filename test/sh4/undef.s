.section .text
.include "sh4/inc.s"
!
! Test for undefined/unknown instructions. The only "official"
! undefined instruction is 0xFFFD, but this tests everything
! that doesn't match a known instruction pattern. Undefined
! instructions are expected to raise general-illegal when not
! in a delay slot, and slot-illegal when in a delay slot.

.global _test_undefined
_test_undefined:	
	start_test

test_undef_1:	! First the official one
	add #1, r12
	expect_exc 0x00000180
test_undef_1_pc:
	.word 0xFFFD
	assert_exc_caught test_undef_str_k test_undef_1_pc

test_undef_1a:	! 0xFFFD with FPU disabled - should still be an 0x180
	add #1, r12
	stc sr, r0
	xor r1, r1
	add #32, r1
	shll2 r1
	shll8 r1
	or r0, r1
	ldc r1, sr
	expect_exc 0x00000180
test_undef_1a_pc:
	.word 0xFFFD
	assert_exc_caught test_undef_str_k test_undef_1a_pc
	stc sr, r0
	xor r1, r1
	add #32, r1
	shll2 r1
	shll8 r1
	not r1, r1
	and r0, r1
	ldc r1, sr
	
! Gaps in the STC range (0x0nn2)
test_undef_2:	! 0x52
	add #1, r12
	expect_exc 0x00000180
test_undef_2_pc:
	.word 0x0052
	assert_exc_caught test_undef_str_k test_undef_2_pc

test_undef_3:	! 0x62
	add #1, r12
	expect_exc 0x00000180
test_undef_3_pc:
	.word 0x0062
	assert_exc_caught test_undef_str_k test_undef_3_pc

test_undef_4:	! 0x72
	add #1, r12
	expect_exc 0x00000180
test_undef_4_pc:
	.word 0x0072
	assert_exc_caught test_undef_str_k test_undef_4_pc

! Test undefined FP instructions w/ and w/o FP disable
test_undef_fpu_1:
	add #1, r12
	expect_exc 0x00000180
test_undef_fpu_1_pc:
	.word 0xF0CD
	assert_exc_caught test_undef_str_k test_undef_fpu_1_pc
	
test_undef_end:
	end_test test_undef_str_k

test_undef_str_k:
	.long test_undef_str
test_undef_str:
	.string "UNDEFINED-INSTRUCTION"
