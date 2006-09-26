.section .text
.include "sh4/inc.s"
!
! Test for all cases that raise a slot-illegal exception (according to the SH4
! manual). See Page 103 of the Hitachi manual

.global _test_slot_illegal
_test_slot_illegal:	
	start_test

! First the easy ones - instructions not permitted in delay slots at any
! time:
! JMP, JSR, BRA, BRAF, BSR, BSRF, RTS, RTE, BT, BF, BT/S, BF/S, TRAPA,
! LDC (to SR), MOV pcrel, MOVA
!
! Note that the tests use BSR as the branch instruction, and assume it
! functions correctly.
	
test_slot_1:	!JMP
	add #1, r12
	expect_exc 0x000001A0
test_slot_1_pc:	
	bsr test_slot_fail
	jmp @r3
	assert_exc_caught test_slot_str_k1 test_slot_1_pc

test_slot_2:	! JSR
	add #1, r12
	expect_exc 0x000001A0
test_slot_2_pc:
	bsr test_slot_fail
	jsr @r3
	assert_exc_caught test_slot_str_k1 test_slot_2_pc
	bra test_slot_3
	nop
test_slot_str_k1:
	.long test_slot_str
	
test_slot_3:	! BRA
	add #1, r12
	expect_exc 0x000001A0
test_slot_3_pc:
	bsr test_slot_fail
	bra test_slot_fail
	assert_exc_caught test_slot_str_k test_slot_3_pc

test_slot_4:	! BRAF
	add #1, r12
	expect_exc 0x000001A0
test_slot_4_pc:
	bsr test_slot_fail
	braf r3
	assert_exc_caught test_slot_str_k test_slot_4_pc

test_slot_5:	! BSR
	add #1, r12
	expect_exc 0x000001A0
test_slot_5_pc:
	bsr test_slot_fail
	bsr test_slot_fail
	assert_exc_caught test_slot_str_k test_slot_5_pc

test_slot_6:	! BSRF
	add #1, r12
	expect_exc 0x000001A0
test_slot_6_pc:
	bsr test_slot_fail
	bsrf r3
	assert_exc_caught test_slot_str_k test_slot_6_pc

test_slot_7:	! BF
	add #1, r12
	expect_exc 0x000001A0
test_slot_7_pc:
	bsr test_slot_fail
	bf test_slot_7_fail
test_slot_7_fail:	
	assert_exc_caught test_slot_str_k test_slot_7_pc

test_slot_8:	! BT
	add #1, r12
	expect_exc 0x000001A0
test_slot_8_pc:
	bsr test_slot_fail
	bt test_slot_8_fail
test_slot_8_fail:	
	assert_exc_caught test_slot_str_k test_slot_8_pc

test_slot_9:	! BF/S
	add #1, r12
	expect_exc 0x000001A0
test_slot_9_pc:
	bsr test_slot_fail
	bf/s test_slot_9_fail
test_slot_9_fail:	
	assert_exc_caught test_slot_str_k test_slot_9_pc

test_slot_10:	! BT/S
	add #1, r12
	expect_exc 0x000001A0
test_slot_10_pc:
	bsr test_slot_fail
	bt/s test_slot_10_fail
test_slot_10_fail:	
	assert_exc_caught test_slot_str_k test_slot_10_pc

test_slot_11:	! TRAPA
	add #1, r12
	expect_exc 0x000001A0
test_slot_11_pc:
	bsr test_slot_fail
	trapa #12
	assert_exc_caught test_slot_str_k test_slot_11_pc

test_slot_12:	! LDC r0, sr
	add #1, r12
	expect_exc 0x000001A0
	stc sr, r0
test_slot_12_pc:
	bsr test_slot_fail
	ldc r0, sr
	assert_exc_caught test_slot_str_k test_slot_12_pc

test_slot_13:	! LDC @r0, sr
	add #1, r12
	expect_exc 0x000001A0
	stc sr, r1
	mova test_slot_13_temp, r0
	mov.l r1, @r0
test_slot_13_pc:
	bsr test_slot_fail
	ldc.l @r0+, sr
	assert_exc_caught test_slot_str_k test_slot_13_pc
	bra test_slot_14
	nop
test_slot_13_temp:
	.long 0
	
test_slot_14:	! MOVA
	add #1, r12
	expect_exc 0x000001A0
test_slot_14_pc:
	bsr test_slot_fail
	mova test_slot_15, r0
	assert_exc_caught test_slot_str_k test_slot_14_pc

test_slot_15:	! MOV.W pcrel, Rn
	add #1, r12
	expect_exc 0x000001A0
test_slot_15_pc:
	bsr test_slot_fail
	mov.w test_slot_16, r0
	assert_exc_caught test_slot_str_k test_slot_15_pc

test_slot_16:	! MOV.L pcrel, Rn
	add #1, r12
	expect_exc 0x000001A0
test_slot_16_pc:
	bsr test_slot_fail
	mov.l test_slot_str_k, r0
	assert_exc_caught test_slot_str_k test_slot_16_pc

test_slot_17:	! "Undefined" 0xFFFD
	add #1, r12
	expect_exc 0x000001A0
test_slot_17_pc:
	bsr test_slot_fail
	.word 0xFFFD
	assert_exc_caught test_slot_str_k test_slot_17_pc

!
! Ok now the privilege tests. These should raise SLOT_ILLEGAL when executed
! in a delay slot (otherwise it's GENERAL_ILLEGAL)
! TODO:	need mode-switch code
	
test_slot_end:
	end_test test_slot_str_k

! Returns after the delay slot, which should hit the "no exception" test
test_slot_fail:
	rts
	nop

test_slot_str_k:
	.long test_slot_str
test_slot_str:
	.string "SLOT-ILLEGAL"

	