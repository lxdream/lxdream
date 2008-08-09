.section .text
.include "sh4/inc.s"
!
! Test for correct performance of the continuation-type core exit - that is,
! the memory write instruction finishes completely, and the following 
! instruction is only executed once. Note that we assume the exit actually
! takes place, but the mmu tests are unlikely to pass if it doesn't.
!
! Reserved:
!   r11 Original value of MMUCR
!   r10 Address of MMUCR
!   r9  Current value of MMUCR

.global _test_vmexit
_test_vmexit:	
	start_test

	mov.l r11, @-r15
	mov.l r10, @-r15
	mov.l r9, @-r15

	mov.l test_vmexit_mmucr, r10
	mov.l @r10, r11
	mov r11, r9

test_vmexit_1:
	add #1, r12
	mov r10, r0
	mov r10, r2
	mov #1, r1
    add #4, r0
	xor r1, r9
	mov.l r9, @-r0
	add #1, r1
	cmp/eq r0, r2
	bt test_vmexit_1a
	fail test_vmexit_str_k
	bra test_vmexit_2
	nop
test_vmexit_1a:
	mov #2, r0
	cmp/eq r1, r0
	bt test_vmexit_2
	fail test_vmexit_str_k	

test_vmexit_2:
	add #1, r12
	mov #1, r1
	xor r1, r9
	bra test_vmexit_2_ok
	mov.l r9, @r10
	fail test_vmexit_str_k
	bra test_vmexit_end
	nop

test_vmexit_2_ok:

test_vmexit_end:
	mov.l r11, @r10
	mov.l @r15+, r9
	mov.l @r15+, r10
	mov.l @r15+, r11
	end_test test_vmexit_str_k

test_vmexit_mmucr:
	.long 0xFF000010
test_vmexit_str_k:
	.long test_vmexit_str
test_vmexit_str:
	.string "VM-EXIT"
