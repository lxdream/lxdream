.section .text
.include "sh4/inc.s"

.global _test_xtrct
_test_xtrct:
	start_test
	
test_xtrct_1:
	add #1, r12

	mov.l test_xtrct_1_input_1, r3
	mov.l test_xtrct_1_input_2, r4
	mov r4, r5
	xtrct r3, r4
	xtrct r5, r3
	mov.l test_xtrct_1_result_1, r0
	mov.l test_xtrct_1_result_2, r1
	cmp/eq r0, r4
	bf test_xtrct_1_fail
	cmp/eq r1, r3
	bt test_xtrct_2
test_xtrct_1_fail:
	fail test_xtrct_str_k
	bra test_xtrct_2
	nop
test_xtrct_1_input_1:
	.long 0x12345678
test_xtrct_1_input_2:	
	.long 0x9ABCDEF0
test_xtrct_1_result_1:
	.long 0x56789ABC
test_xtrct_1_result_2:
	.long 0xDEF01234

test_xtrct_2:
	add #1, r12

	mov.l test_xtrct_2_input, r3
	xtrct r3, r3
	mov.l test_xtrct_2_result, r4
	cmp/eq r3, r3
	bt test_xtrct_end
	fail test_xtrct_str_k
	bra test_xtrct_end
	nop
test_xtrct_2_input:
	.long 0x2143546A
test_xtrct_2_result:	
	.long 0x546A2143

test_xtrct_end:
	end_test test_xtrct_str_k

test_xtrct_str:
	.string "XTRCT"

.align 4	
test_xtrct_str_k:	
	.long test_xtrct_str
	