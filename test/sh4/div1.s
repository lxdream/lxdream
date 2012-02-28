.section .text
.include "sh4/inc.s"
!
! Test DIV1 operation
!
.global _test_div1
_test_div1:
	start_test
	mov.l r11, @-r15
	mov.l r10, @-r15

	mova test_div1_data, r0
	mov r0, r11
	mov #12, r10	
test_div1_loop:	
	add #1, r12
	
	mov.l @r11+, r4
	mov.l @r11+, r5
	mov.l @r11+, r6

	stc sr, r2
	mov.l test_div1_sr_mask, r0
	not r0, r1
	and r1, r2
	or r6, r2
	ldc r2, sr
	mov r4, r3
	
	div1 r4, r5

	stc sr, r2
	and r0, r2
	cmp/eq r3, r4
	bf test_div1_fail
	mov.l @r11+, r3
	cmp/eq r3, r5
	bf test_div1_fail
	mov.l @r11+, r3
	cmp/eq r3, r2
	bt test_div1_next
test_div1_fail:
	fail test_div1_str_k

test_div1_next:	
	dt r10
	bf test_div1_loop	

test_div1_end:
	mov.l @r15+, r10
	mov.l @r15+, r11
	end_test test_div1_str_k

test_div1_data:
	.long 0x12345678
	.long 0x01234123
	.long 0x00000001
	.long 0xF0122BCF
	.long 0x00000100

	.long 0x11223344
	.long 0xF0122BCF
	.long 0x00000100
	.long 0xF1468AE2
	.long 0x00000100

	.long 0x20103040
	.long 0xF1468AE2
	.long 0x00000101
	.long 0x029D4605
	.long 0x00000001

	.long 0x01231231
	.long 0x029D4605
	.long 0x00000000
	.long 0x041779D9
	.long 0x00000001

	.long 0xF1234123
	.long 0x13434454
	.long 0x00000100
	.long 0x17A9C9CB
	.long 0x00000100

	.long 0x65432123
	.long 0x12312312
	.long 0x00000101
	.long 0x89A56748
	.long 0x00000001

! and now the m=1 cases
	.long 0x12345678
	.long 0x01234123
	.long 0x00000301
	.long 0xF0122BCF
	.long 0x00000200

	.long 0x11223344
	.long 0xF0122BCF
	.long 0x00000200
	.long 0xF1468AE2
	.long 0x00000200

	.long 0x20103040
	.long 0xF1468AE2
	.long 0x00000201
	.long 0x029D4605
	.long 0x00000301

	.long 0x01231231
	.long 0x029D4605
	.long 0x00000300
	.long 0x041779D9
	.long 0x00000301

	.long 0xF1234123
	.long 0x13434454
	.long 0x00000200
	.long 0x17A9C9CB
	.long 0x00000200

	.long 0x65432123
	.long 0x12312312
	.long 0x00000201
	.long 0x89A56748
	.long 0x00000301
	
test_div1_data_end:
	
test_div1_sr_mask:
	.long 0x00000301

test_div1_str_k:
	.long test_div1_str
test_div1_str:
	.string "DIV1"
	