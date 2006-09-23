.section .text
.include "sh4/inc.s"
!
! Test AND #imm, R0 operation
! Test AND #imm, @(r0,GBR)

.global _test_andi
_test_andi:
	start_test
	
test_andi_1:	! test and ff, 0
	add #1, r12
	xor r0, r0
	xor r1, r1
	and #255, r0
	cmp/eq r0, r1
	bt test_andi_2
	fail test_andi_str_k

test_andi_2:	! test 0-extend
	add #1, r12
	mov.l test_andi_2_op1, r0
	and #255, r0
	mov.l test_andi_2_result, r4
	cmp/eq r0, r4
	bt test_andi_3
	fail test_andi_str_k
	bra test_andi_3
	nop

test_andi_2_op1:
	.long 0x98765432
test_andi_2_result:
	.long 0x00000032
	
test_andi_3:     ! Test single-bit AND
	add #1, r12
	mov.l test_andi_3_op1, r0
	and #128, r0
	mov.l test_andi_3_result, r3
	cmp/eq r0, r3
	bt test_andi_4
	fail test_andi_str_k
	bra test_andi_4
	nop
test_andi_3_op1:
	.long 0x123456AB
test_andi_3_result:
	.long 0x00000080

test_andi_4:	! Test GBR version
	add #1, r12
	stc gbr, r4
	mov.l test_andi_4_gbr, r0
	ldc r0, gbr
	mov.l test_andi_4_op1, r0
	and.b #254, @(r0,gbr)
	ldc r4, gbr
	mov.l test_andi_4_output, r1
	mov.l test_andi_4_result, r2
	cmp/eq r1, r2
	bt test_andi_5
	fail test_andi_str_k
	bra test_andi_5
	nop
test_andi_4_gbr:
	.long test_andi_4_gbr
test_andi_4_op1:
	.long 0x00000008
test_andi_4_output:
	.long 0x123456AB
test_andi_4_result:
	.long 0x123456AA
	
test_andi_5:	
test_andi_end:
	end_test test_andi_str_k

test_andi_str:
	.string "ANDi"

.align 4	
test_andi_str_k:	
	.long test_andi_str
	