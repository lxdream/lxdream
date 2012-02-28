.section .text
.include "sh4/inc.s"

.global _test_ldc
_test_ldc:
	start_test
	mov.l r8, @-r15
	mov.l r9, @-r15
	
test_ldcsr_1:
	add #1, r12

	stc sr, r8
	mov #-1, r1
	ldc r1, sr
	stc sr, r9
	ldc r8, sr
	mov.l sr_mask, r3
	cmp/eq r9, r3
	bt test_ldsfpscr_1
	fail test_ldc_str_k

test_ldsfpscr_1:
	add #1,r12
	sts fpscr, r0
	mov #-1, r1
	lds r1, fpscr
	sts fpscr, r2
	lds r0, fpscr
	mov.l fpscr_mask, r3
	cmp/eq r2, r3
	bt test_ldc_end
	fail test_ldc_str_k

test_ldc_end:
	mov.l @r15+, r9
	mov.l @r15+, r8
	end_test test_ldc_str_k

test_ldc_str:
	.string "LDC/S"
.align 4
sr_mask:
	.long 0x700083F3
fpscr_mask:
	.long 0x003FFFFF
test_ldc_str_k:	
	.long test_ldc_str
