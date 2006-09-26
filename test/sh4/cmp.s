.section .text
.include "sh4/inc.s"
!
! Test cmp/xx

.global _test_cmp
_test_cmp:	
	start_test

test_cmpeq_1:	! CMP/EQ 0, 0
	add #1, r12
	xor r0, r0
	xor r1, r1
	cmp/eq r0, r1
	bt test_cmpeq_2
	fail test_cmpeq_str_k

test_cmpeq_2:	! CMP/EQ !0, 0
	add #1, r12
	mov #50, r2
	cmp/eq r1, r2
	bf test_cmpeq_3
	fail test_cmpeq_str_k

test_cmpeq_3:	! CMP/EQ -50, 50
	add #1, r12
	mov #-50, r3
	cmp/eq r3, r2
	bf test_cmpeq_4
	fail test_cmpeq_str_k

test_cmpeq_4:	! CMP/EQ 50, 50
	add #1, r12
	mov #50, r6
	cmp/eq r6, r2
	bt test_cmpeq_5
	fail test_cmpeq_str_k

test_cmpeq_5:

test_cmpeq_6:

test_cmpge_1:	
	add #1, r12
	cmp/ge r2, r6
	bt test_cmpge_2
	fail test_cmpge_str_k

test_cmpge_2:
	add #1, r12
	cmp/ge r3, r2
	bt test_cmpge_3
	fail test_cmpge_str_k

test_cmpge_3:
	add #1, r12
	cmp/ge r2, r3
	bf test_cmpge_4
	fail test_cmpge_str_k

test_cmpge_4:
	add #1, r12
	mov #75, r5
	cmp/ge r2, r5
	bt test_cmpge_5
	fail test_cmpge_str_k

test_cmpge_5:	
	add #1, r12
	cmp/ge r5, r2
	bf test_cmpgt_1
	fail test_cmpge_str_k
	
test_cmpgt_1:	
	add #1, r12
	cmp/gt r2, r6
	bf test_cmpgt_2
	fail test_cmpgt_str_k

test_cmpgt_2:
	add #1, r12
	cmp/gt r3, r2
	bt test_cmpgt_3
	fail test_cmpgt_str_k

test_cmpgt_3:
	add #1, r12
	cmp/gt r2, r3
	bf test_cmpgt_4
	fail test_cmpgt_str_k

test_cmpgt_4:
	add #1, r12
	mov #75, r5
	cmp/gt r2, r5
	bt test_cmpgt_5
	fail test_cmpgt_str_k

test_cmpgt_5:	
	add #1, r12
	cmp/gt r5, r2
	bf test_cmphi_1
	fail test_cmpgt_str_k
	
	
test_cmphi_1:	
	add #1, r12
	cmp/hi r2, r6
	bf test_cmphi_2
	fail test_cmphi_str_k

test_cmphi_2:
	add #1, r12
	cmp/hi r3, r2
	bf test_cmphi_3
	fail test_cmphi_str_k

test_cmphi_3:
	add #1, r12
	cmp/hi r2, r3
	bt test_cmphi_4
	fail test_cmphi_str_k

test_cmphi_4:
	add #1, r12
	mov #75, r5
	cmp/hi r2, r5
	bt test_cmphi_5
	fail test_cmphi_str_k

test_cmphi_5:	
	add #1, r12
	cmp/hi r5, r2
	bf test_cmphs_1
	fail test_cmphi_str_k

test_cmphs_1:	
	add #1, r12
	cmp/hs r2, r6
	bt test_cmphs_2
	fail test_cmphs_str_k

test_cmphs_2:
	add #1, r12
	cmp/hs r3, r2
	bf test_cmphs_3
	fail test_cmphs_str_k

test_cmphs_3:
	add #1, r12
	cmp/hs r2, r3
	bt test_cmphs_4
	fail test_cmphs_str_k

test_cmphs_4:
	add #1, r12
	mov #75, r5
	cmp/hs r2, r5
	bt test_cmphs_5
	fail test_cmphs_str_k

test_cmphs_5:	
	add #1, r12
	cmp/hs r5, r2
	bf test_cmppl_1
	fail test_cmphs_str_k

test_cmppl_1:
	
test_cmp_end:
	end_test test_cmp_str_k
test_cmpeq_str:
	.string "CMP/EQ"
test_cmpge_str:
	.string "CMP/GE"
test_cmpgt_str:
	.string "CMP/GT"
test_cmphi_str:
	.string "CMP/HI"
test_cmphs_str:
	.string "CMP/HS"
test_cmp_str:
	.string "CMP"

.align 4	
test_cmp_str_k:	
	.long test_cmp_str
test_cmpeq_str_k:
	.long test_cmpeq_str
test_cmpge_str_k:
	.long test_cmpge_str
test_cmpgt_str_k:
	.long test_cmpgt_str
test_cmphi_str_k:
	.long test_cmphi_str
test_cmphs_str_k:
	.long test_cmphs_str
	