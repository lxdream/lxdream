.section .text
.include "sh4/inc.s"
!
! Test cmp/xx

.global _test_cmpstr
_test_cmpstr:
	start_test
	mov.l r11, @-r15
	mov.l r10, @-r15

test_cmpstr_1:	! CMP/STR r2,r2
	add #1, r12
	mova test_cmpstr_str, r0
	cmp/str r0, r0
	bt test_cmpstr_2
	fail test_cmpstr_str_k

test_cmpstr_2:
	mova test_cmpstr_data, r0
	mov r0, r11
	mov #6, r10

test_cmpstr_2_loop:
	add #1, r12
	mov.l @r11+, r2
	mov.l @r11+, r3
	mov.l @r11+, r4
	cmp/str r2, r3
	movt r0
	cmp/eq r0, r4
	bt test_cmpstr_2_ok
	fail test_cmpstr_str_k
test_cmpstr_2_ok:
	dt r10
	bf test_cmpstr_2_loop

test_cmpstr_end:
	mov.l @r15+, r10
	mov.l @r15+, r11
	end_test test_cmpstr_str_k

test_cmpstr_data:
	.long 0x81828384
	.long 0x82838485
	.long 0x00000000

	.long 0x01040302
	.long 0x02010304
	.long 0x00000001

	.long 0xAA55AA55
	.long 0x55AA55AA
	.long 0x00000000

	.long 0x12345678
	.long 0x12345678
	.long 0x00000001

	.long 0xABCD01DC
	.long 0xABCD01DD
	.long 0x00000001

	.long 0x12003423
	.long 0x12342300
	.long 0x00000001

test_cmpstr_str_k:
	.long test_cmpstr_str
test_cmpstr_str:
	.string "CMP/STR"
	