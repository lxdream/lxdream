.section .text
.include "sh4/inc.s"
!
! Test for correct UTLB operation.
! 
! Note we don't test triggering a TLB multiple-hit exception - it's a reset
! rather than a regular exception.

.global _test_tlb
_test_tlb:
	start_test

! Turn on AT, and flush the current TLB (if any)
! Initialize to SV=0, SQMD=0, URB=URC=LRUI=0
	mov.l test_tlb_mmucr, r0
	mov #5, r1
	mov.l r1, @r0
	
! Privileged mode tests first (much easier)
	add #1, r12
	mov.l test_tlb1_pteh, r1
	mov.l test_tlb_pteh, r2
	mov.l r1, @r2
	mov.l test_tlb1_ptel, r1
	mov.l test_tlb_ptel, r2
	mov.l r1, @r2
	ldtlb

! Simple read
	mov.l test_tlb1_direct, r3
	mov #42, r2
	mov.l r2, @r3
	mov.l test_tlb1_mmu, r0
	mov.l @r0, r1
	cmp/eq r1, r2
	bt test_tlb_2
	fail test_tlb_str_k
	bra test_tlb_2
	nop
test_tlb1_pteh:
	.long 0x12345012
test_tlb1_ptel:
	.long 0x005F8120

test_tlb_2:		
	! Trigger an initial-page-write exception
	add #1, r12
	expect_exc 0x00000080
	mov.l test_tlb1_mmu, r0
test_tlb2_exc:	
	mov.l r0, @r0
	assert_tlb_exc_caught test_tlb_str_k test_tlb2_exc test_tlb1_mmu

test_tlb_3:
	! Trigger a missing page read exception by invalidation
	add #1, r12
	mov.l test_tlb3_addr, r1
	mov.l test_tlb3_data, r2
	mov.l r2, @r1

	expect_exc 0x00000040
	mov.l test_tlb1_mmu, r0
test_tlb3_exc:
	mov.l @r0, r2
	assert_tlb_exc_caught test_tlb_str_k, test_tlb3_exc, test_tlb1_mmu
	bra test_tlb_4
	nop
	
test_tlb3_addr:
	.long 0xF6000F80
test_tlb3_data:
	.long 0x12345212

test_tlb_4:
	! Test missing page write exception on the same page
	add #1, r12
	expect_exc 0x00000060
	mov.l test_tlb1_mmu, r0
test_tlb4_exc:
	mov.l r2, @r0
	assert_tlb_exc_caught test_tlb_str_k, test_tlb4_exc, test_tlb1_mmu
	
test_tlb_5: ! Test initial write exception
	add #1, r12

	mov.l test_tlb5_addr, r1
	mov.l test_tlb5_data, r2
	mov.l r2, @r1
	
	expect_exc 0x00000080
	mov.l test_tlb1_mmu, r0
	mov #63, r3
test_tlb5_exc:
	mov.l r3, @r0
	assert_tlb_exc_caught test_tlb_str_k, test_tlb5_exc, test_tlb1_mmu
	mov.l test_tlb1_direct, r3
	mov.l @r3, r4
	mov #42, r2
	cmp/eq r2, r4
	bf test_tlb5_fail
	mov.l test_tlb1_mmu, r0
	mov.l @r0, r3
	cmp/eq r2, r3
	bt test_tlb_6
test_tlb5_fail:
	fail test_tlb_str_k
        bra test_tlb_6
        nop

test_tlb5_addr:
	.long 0xF6000000
test_tlb5_data:
	.long 0x12345112

test_tlb_6:! Test successful write.
	add #1, r12

	mov.l test_tlb6_addr, r1
	mov.l test_tlb6_data, r2
	mov.l r2, @r1

	mov.l test_tlb1_mmu, r0
	mov #77, r3
	mov.l r3, @r0
	mov.l test_tlb1_direct, r1
	mov.l @r1, r2
	cmp/eq r2, r3
	bt test_tlb_7
	fail test_tlb_str_k
	bra test_tlb_7
	nop

test_tlb_7:	
	bra test_tlb_end
	nop

test_tlb6_addr:
	.long 0xF6000F80
test_tlb6_data:
	.long 0x12345312

			
test_tlb1_mmu:
	.long 0x12345040
test_tlb1_direct:
	.long 0xA05F8040  ! Display border colour

test_tlb_end:
	xor r0, r0
	mov.l test_tlb_mmucr, r1
	mov.l r0, @r1
	
	end_test test_tlb_str_k

test_tlb_mmucr:
	.long 0xFF000010
test_tlb_pteh:
	.long 0xFF000000
test_tlb_ptel:
	.long 0xFF000004
test_tlb_tea:
	.long 0xFF00000C
test_tlb_str:
	.string "TLB"
.align 4
test_tlb_str_k:
	.long test_tlb_str
