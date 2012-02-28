.section .text
.include "sh4/inc.s"

.global _test_subc
_test_subc:
	start_test
	
test_subc_1:	! test subing 0+0 = 0
	add #1, r12

	xor r3, r3
	clrt
	subc r3, r3
	bt test_subc_1_fail
	tst r3, r3
	bt test_subc_2
test_subc_1_fail:
	fail test_subc_str_k

test_subc_2:   ! test subing 0+0+T = -1
	add #1, r12

	xor r3, r3
	sett
	subc r3, r3
	bf test_subc_2_fail
	mov.l test_subc_2_result, r2
	cmp/eq r2, r3
	bt test_subc_3
test_subc_2_fail:
	fail test_subc_str_k
	bra test_subc_3
	nop
test_subc_2_result:
	.long 0xFFFFFFFF
test_subc_3:
	add #1, r12

	xor r3, r3
	mov.l test_subc_3_input, r2
	clrt
	subc r2, r3
	bf test_subc_3_fail
	mov.l test_subc_3_result, r1
	cmp/eq r1, r3
	bt test_subc_4
test_subc_3_fail:
	fail test_subc_str_k
	bra test_subc_4
	nop
test_subc_3_input:
	.long 0x00000001
test_subc_3_result:
	.long 0xFFFFFFFF

test_subc_4:
	add #1, r12

	xor r3, r3
	mov.l test_subc_4_input, r2
	sett
	subc r3, r2
	bt test_subc_4_fail
	tst r2, r2
	bt test_subc_5
test_subc_4_fail:
	fail test_subc_str_k
	bra test_subc_5
	nop
test_subc_4_input:
	.long 0x00000001

test_subc_5:	
test_subc_end:
	end_test test_subc_str_k

test_subc_str:
	.string "SUBC"

.align 4	
test_subc_str_k:	
	.long test_subc_str
	