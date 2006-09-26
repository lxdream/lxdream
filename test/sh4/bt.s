.section .text
.include "sh4/inc.s"
!
! Test bt pcrel
! Test bt/s pcrel

.global _test_bt
_test_bt:
	start_test
	
test_bt_1:   ! Test branch not taken
	add #1, r12
	clc
	bt test_bt_1_b
	bra test_bt_2
	nop
test_bt_1_b:
	fail test_bt_str_k
	
test_bt_2:	! Test branch taken
	add #1, r12
	setc
	bt test_bt_3
	fail test_bt_str_k

test_bt_3:	! Test branch taken (backwards)
	add #1, r12
	setc
	bra test_bt_3_b
	nop
	fail test_bt_str_k
	bra test_bt_4
test_bt_3_c:	
	nop
	bra test_bt_4
	nop
	fail test_bt_str_k
	bra test_bt_4
test_bt_3_b:
	nop
	bt test_bt_3_c
	fail test_bt_str_k
	bra test_bt_4	
	nop
test_bt_4:	! Test branch not taken w/ delay
	add #1, r12
	clc
	xor r0, r0
	bt/s test_bt_4_b
	add #1, r0
	bra test_bt_4_c
	nop
test_bt_4_b:
	fail test_bt_str_k
	bra test_bt_5
	nop
test_bt_4_c:
	xor r1,r1
	add #1, r1
	cmp/eq r0, r1

test_bt_5:	! Test branch taken w/ delay
	add #1, r12
	setc
	xor r0,r0
	bt/s test_bt_5_b
	add #1, r0
	fail test_bt_str_k
	bra test_bt_6
test_bt_5_b:	
	xor r1,r1
	add #1, r1
	cmp/eq r0,r1
	bt test_bt_6
	fail test_bt_str_k

test_bt_6:	! Test back-branch taken w/ delay
	add #1, r12
	setc
	xor r0, r0
	bra test_bt_6_b
	nop
	fail test_bt_str_k
	bra test_bt_7
	nop
	add #1, r13
test_bt_6_c:
	mov #1, r1
	cmp/eq r0, r1
	bt test_bt_7
	fail test_bt_str_k
	bra test_bt_7
	nop
	fail test_bt_str_k
	bra test_bt_7
test_bt_6_b:
	nop
	bt/s test_bt_6_c
	add #1, r0
	fail test_bt_str_k
	bra test_bt_7	
	nop
	
test_bt_7:
	add #1, r12
	expect_exc 0x000001A0 ! BT is slot illegal
test_bt_7_exc:	
	bra test_bt_7_b
	bt test_bt_7_b
	assert_exc_caught test_bt_str_k test_bt_7_exc
	bra test_bt_8
	nop
test_bt_7_b:
test_bt_7_c:	
	fail test_bt_str_k

test_bt_8:
	add #1, r12
	expect_exc 0x000001A0 ! BT/S is slot illegal
test_bt_8_exc:	
	bra test_bt_8_b
	bt/s test_bt_8_b
	nop
	assert_exc_caught test_bt_str_k test_bt_8_exc
	bra test_bt_end
	nop
test_bt_8_b:
test_bt_8_c:	
	fail test_bt_str_k
		
test_bt_end:
	end_test test_bt_str_k

test_bt_str:
	.string "BT"

.align 4	
test_bt_str_k:	
	.long test_bt_str
	