.section .text
.include "sh4/inc.s"

.global _test_ldc
_test_ldc:
	start_test
	
test_ldcsr_1:
	add #1, r12

	stc sr, r0
	mov #-1, r1
	ldc r1, sr
	stc sr, r2
	ldc r0, sr
	mov.l sr_mask, r3
	cmp/eq r2, r3
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
