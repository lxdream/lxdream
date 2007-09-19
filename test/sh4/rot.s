.section .text
.include "sh4/inc.s"

.global _test_rot
_test_rot:
	start_test

test_rotl_1:
	add #1, r12

	mov.l test_rotl_1_input, r3
	mov.l test_rotl_1_result, r4
	clrt
	rotl r3
	bf test_rotl_1_fail
	cmp/eq r3, r4
	bt test_rotl_2
test_rotl_1_fail:	
	fail test_rot_str_k
	bra test_rotl_2
	nop
test_rotl_1_input:
	.long 0x94E12323
test_rotl_1_result:
	.long 0x29C24647

test_rotl_2:
	add #1, r12
	mov.l test_rotl_2_input, r3
	mov.l test_rotl_2_result, r4
	sett
	rotl r3
	bt test_rotl_2_fail
	cmp/eq r3, r4
	bt test_rotcl_1
test_rotl_2_fail:	
	fail test_rot_str_k
	bra test_rotcl_1
	nop
test_rotl_2_input:
	.long 0x29C24646
test_rotl_2_result:
	.long 0x53848C8C


test_rotcl_1:
	add #1, r12

	mov.l test_rotcl_1_input, r3
	mov.l test_rotcl_1_result, r4
	clrt
	rotcl r3
	bf test_rotcl_1_fail
	cmp/eq r3, r4
	bt test_rotcl_2
test_rotcl_1_fail:	
	fail test_rot_str_k
	bra test_rotcl_2
	nop
test_rotcl_1_input:
	.long 0x94E12323
test_rotcl_1_result:
	.long 0x29C24646

test_rotcl_2:
	add #1, r12
	mov.l test_rotcl_2_input, r3
	mov.l test_rotcl_2_result, r4
	sett
	rotcl r3
	bt test_rotcl_2_fail
	cmp/eq r3, r4
	bt test_rotr_1
test_rotcl_2_fail:	
	fail test_rot_str_k
	bra test_rotr_1
	nop
test_rotcl_2_input:
	.long 0x29C24646
test_rotcl_2_result:
	.long 0x53848C8D

test_rotr_1:
	add #1, r12

	mov.l test_rotr_1_input, r3
	mov.l test_rotr_1_result, r4
	clrt
	rotr r3
	bf test_rotr_1_fail
	cmp/eq r3, r4
	bt test_rotr_2
test_rotr_1_fail:	
	fail test_rot_str_k
	bra test_rotr_2
	nop
test_rotr_1_input:
	.long 0x94E12323
test_rotr_1_result:
	.long 0xCA709191

test_rotr_2:
	add #1, r12
	mov.l test_rotr_2_input, r3
	mov.l test_rotr_2_result, r4
	sett
	rotr r3
	bt test_rotr_2_fail
	cmp/eq r3, r4
	bt test_rotcr_1
test_rotr_2_fail:	
	fail test_rot_str_k
	bra test_rotcr_1
	nop
test_rotr_2_input:
	.long 0xC2709192
test_rotr_2_result:
	.long 0x613848C9


test_rotcr_1:
	add #1, r12

	mov.l test_rotcr_1_input, r3
	mov.l test_rotcr_1_result, r4
	clrt
	rotcr r3
	bf test_rotcr_1_fail
	cmp/eq r3, r4
	bt test_rotcr_2
test_rotcr_1_fail:	
	fail test_rot_str_k
	bra test_rotcr_2
	nop
test_rotcr_1_input:
	.long 0x94E12323
test_rotcr_1_result:
	.long 0x4A709191

test_rotcr_2:
	add #1, r12
	mov.l test_rotcr_2_input, r3
	mov.l test_rotcr_2_result, r4
	sett
	rotcr r3
	bt test_rotcr_2_fail
	cmp/eq r3, r4
	bt test_rot_end
test_rotcr_2_fail:	
	fail test_rot_str_k
	bra test_rot_end
	nop
test_rotcr_2_input:	
	.long 0xC2709192
test_rotcr_2_result:
	.long 0xE13848C9


		


test_rot_end:
	end_test test_rot_str_k

test_rot_str:
	.string "ROT"

.align 4	
test_rot_str_k:	
	.long test_rot_str
	