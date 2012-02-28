.section .text
.include "sh4/inc.s"

.global _test_tas
_test_tas:
	start_test
	
test_tas_1:
	add #1, r12

	mova test_tas_1_data, r0  ! test with byte == 0
	mov r0, r4
	mov r4, r5
	tas.b @r4
	bf test_tas_1_fail
	cmp/eq r4, r5
	bf test_tas_1_fail
	mov #0x40, r1
	shll r1
	mov.l test_tas_1_data, r3
	cmp/eq r1, r3
	bt test_tas_2

test_tas_1_fail:
	fail test_tas_str_k
	bra test_tas_2
	nop
test_tas_1_data:
	.long 0x00000000
	
test_tas_2:	
	add #1, r12

	mova test_tas_2_data, r0  ! test with byte == 0x80
	mov r0, r4
	mov r4, r5
	tas.b @r4                  ! follow up test with byte == 0x00000080
	bt test_tas_2_fail
	cmp/eq r4, r5
	bf test_tas_2_fail
	mov.l @r4, r3
	mov #0x40, r1
	shll r1
	cmp/eq r1, r3
	bt test_tas_3
test_tas_2_fail:
	fail test_tas_str_k
	bra test_tas_3
	nop
test_tas_2_data:
	.long 0x00000080

test_tas_3:	 ! Test with byte = 0x45 (ensure existing bits aren't changed)
	add #1, r12
	mova test_tas_3_data, r0
	mov r0, r4
	mov.l test_tas_3_result, r1
	mov r4, r5
	tas.b @r4
	bt test_tas_3_fail
	cmp/eq r4,r5
	bf test_tas_3_fail
	mov.l test_tas_3_data, r3
	cmp/eq r1, r3
	bt test_tas_4
	
test_tas_3_fail:
	fail test_tas_str_k
	bra test_tas_4
	nop
test_tas_3_data:
	.long 0x00000045
test_tas_3_result:
	.long 0x000000C5

test_tas_4:	 ! Test that it's really a byte op
	add #1, r12
	mova test_tas_4_data, r0
	mov r0, r4
	mov.l test_tas_4_result, r1
	add #1, r4
	mov r4, r5
	tas.b @r4
	bf test_tas_4_fail
	cmp/eq r4, r5
	mov.l test_tas_4_data, r3
	cmp/eq r1, r3
	bt test_tas_end
test_tas_4_fail:
	fail test_tas_str_k
	bra test_tas_end
	nop
test_tas_4_data:
	.long 0xAB9C00ED
test_tas_4_result:
	.long 0xAB9C80ED
		
test_tas_end:
	end_test test_tas_str_k

test_tas_str:
	.string "TAS"

.align 4	
test_tas_str_k:	
	.long test_tas_str
	