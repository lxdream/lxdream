.section .text
.include "sh4/inc.s"

.global _test_ldcsr
_test_ldcsr:
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
	bt test_ldcsr_end
	fail test_ldcsr_str_k

test_ldcsr_end:
	end_test test_ldcsr_str_k

test_ldcsr_str:
	.string "LDC Rn, SR"
.align 4
sr_mask:
	.long 0x700083F3
test_ldcsr_str_k:	
	.long test_ldcsr_str
	