.section .text
.include "sh4/inc.s"
!
! Test DIV0 operation
!
.global _test_div0
_test_div0:
	start_test
	mov.l r11, @-r15
	mov.l r10, @-r15

test_div0u:
	add #1, r12
	stc sr, r2
	mov.l test_div0_sr_mask, r0
	or r0, r2
	ldc r2, sr

	div0u
	stc sr, r3
	mov r3, r4
	and r0, r4
	tst r4, r4
	bf test_div0u_fail
	not r0, r0
	and r0, r3
	and r0, r2
	cmp/eq r2, r3
	bt test_div0s
test_div0u_fail:
	fail test_div0_str_k
	
test_div0s:	
	mova test_div0s_data, r0
	mov r0, r11
	mov #4, r10

test_div0s_loop:
	add #1, r12
	mov.l @r11+, r1
	mov.l @r11+, r2
	mov.l @r11+, r3
	div0s r1,r2
	stc sr, r4
	mov.l test_div0_sr_mask, r0
	and r0,r4
	cmp/eq r3,r4
	bt test_div0s_ok
	fail test_div0_str_k
test_div0s_ok:	
	dt r10
	bf test_div0s_loop
	
test_div0_end:
	mov.l @r15+, r10
	mov.l @r15+, r11
	end_test test_div0_str_k

test_div0s_data:
	.long 0x01234567
	.long 0x12345678
	.long 0x00000000
	.long 0xFFFF8912
	.long 0x7ABC1526
	.long 0x00000201
	.long 0x55443322
	.long 0x80000234
	.long 0x00000101
	.long 0xFFFFFFFF
	.long 0x9CD39495
	.long 0x00000300
	
test_div0_sr_mask:
	.long 0x00000301
	
test_div0_str_k:
	.long test_div0_str
test_div0_str:
	.string "DIV0"
	