.section .text
.include "sh4/inc.s"

.global _test_shl
_test_shl:
	start_test

test_shll_1:
	add #1, r12

	mov.l test_shll_1_input, r3
	mov.l test_shll_1_result, r4
	clrt
	shll r3
	bf test_shll_1_fail
	cmp/eq r3, r4
	bt test_shll_2
test_shll_1_fail:	
	fail test_shl_str_k
	bra test_shll_2
	nop
test_shll_1_input:
	.long 0x94E12323
test_shll_1_result:
	.long 0x29C24646

test_shll_2:
	add #1, r12
	mov.l test_shll_2_input, r3
	mov.l test_shll_2_result, r4
	sett
	shll r3
	bt test_shll_2_fail
	cmp/eq r3, r4
	bt test_shal_1
test_shll_2_fail:	
	fail test_shl_str_k
	bra test_shal_1
	nop
test_shll_2_input:
	.long 0x29C24646
test_shll_2_result:
	.long 0x53848C8C

test_shal_1:
	add #1, r12

	mov.l test_shal_1_input, r3
	mov.l test_shal_1_result, r4
	clrt
	shal r3
	bf test_shal_1_fail
	cmp/eq r3, r4
	bt test_shal_2
test_shal_1_fail:	
	fail test_shl_str_k
	bra test_shal_2
	nop
test_shal_1_input:
	.long 0x94E12323
test_shal_1_result:
	.long 0x29C24646

test_shal_2:
	add #1, r12
	mov.l test_shal_2_input, r3
	mov.l test_shal_2_result, r4
	sett
	shal r3
	bt test_shal_2_fail
	cmp/eq r3, r4
	bt test_shlr_1
test_shal_2_fail:	
	fail test_shl_str_k
	bra test_shlr_1
	nop
test_shal_2_input:
	.long 0x29C24646
test_shal_2_result:
	.long 0x53848C8C

test_shlr_1:
	add #1, r12

	mov.l test_shlr_1_input, r3
	mov.l test_shlr_1_result, r4
	clrt
	shlr r3
	bf test_shlr_1_fail
	cmp/eq r3, r4
	bt test_shlr_2
test_shlr_1_fail:	
	fail test_shl_str_k
	bra test_shlr_2
	nop
test_shlr_1_input:
	.long 0x94E12323
test_shlr_1_result:
	.long 0x4A709191

test_shlr_2:
	add #1, r12
	mov.l test_shlr_2_input, r3
	mov.l test_shlr_2_result, r4
	sett
	shlr r3
	bt test_shlr_2_fail
	cmp/eq r3, r4
	bt test_shar_1
test_shlr_2_fail:	
	fail test_shl_str_k
	bra test_shar_1
	nop
test_shlr_2_input:
	.long 0x42709192
test_shlr_2_result:
	.long 0x213848C9


test_shar_1:
	add #1, r12

	mov.l test_shar_1_input, r3
	mov.l test_shar_1_result, r4
	clrt
	shar r3
	bf test_shar_1_fail
	cmp/eq r3, r4
	bt test_shar_2
test_shar_1_fail:	
	fail test_shl_str_k
	bra test_shar_2
	nop
test_shar_1_input:
	.long 0x94E12323
test_shar_1_result:
	.long 0xCA709191

test_shar_2:
	add #1, r12
	mov.l test_shar_2_input, r3
	mov.l test_shar_2_result, r4
	sett
	shar r3
	bt test_shar_2_fail
	cmp/eq r3, r4
	bt test_shl_end
test_shar_2_fail:	
	fail test_shl_str_k
	bra test_shl_end
	nop
test_shar_2_input:
	.long 0x42709192
test_shar_2_result:
	.long 0x213848C9


	
test_shl_end:
	end_test test_shl_str_k

test_shl_str:
	.string "SHL"

.align 4	
test_shl_str_k:	
	.long test_shl_str
	