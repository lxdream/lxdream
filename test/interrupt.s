.section .text
.include "sh4/inc.s"

! expect_interrupt( int intevt )
.global _expect_interrupt
_expect_interrupt:	
	stc sr, r3  ! Mask off interrupts
	mov.l bl_mask, r0
	or r3, r0
	ldc r0, sr
	mova expected_intevt, r0
	mov.l r4, @r0
	xor r1, r1
	mova expected_expevt, r0
	mov.l r1, @r0
	mova _interrupt_count, r0
	mov.l r1, @r0
	mova _interrupt_pc, r0
	mov.l r1, @r0
	ldc r3, sr  ! Restore old SR state
	rts
	nop
	
	.global _expect_exception
_expect_exception:
	stc sr, r3  ! Mask off interrupts
	mov.l bl_mask, r0
	or r3, r0
	ldc r0, sr
	mova expected_expevt, r0
	mov.l r4, @r0
	xor r1, r1
	mova expected_intevt, r0
	mov.l r1, @r0
	mova _interrupt_count, r0
	mov.l r1, @r0
	mova _interrupt_pc, r0
	mov.l r1, @r0
	ldc r3, sr  ! Restore old SR state
	rts
	nop
	
	.align 4
.global _interrupt_count
_interrupt_count:
	.long 0x00000000
.global _interrupt_pc
_interrupt_pc:
	.long 0x00000000
bl_mask:
	.long 0x10000000

.global _install_interrupt_handler
_install_interrupt_handler:
	stc vbr, r1
	mova old_vbr, r0
	mov.l r1, @r0
	mova __interrupt_handler, r0
	ldc r0, vbr
	rts
	nop

.global _remove_interrupt_handler
_remove_interrupt_handler:
	mov.l old_vbr, r1
	ldc r1, vbr
	rts
	nop
.align 4
old_vbr:
	.long 0x00000000
expected_intevt:
	.long 0x00000000
expected_expevt:
	.long 0x00000000

	
__interrupt_handler:
	.skip 0x100 
general_exception:
	mov.l handler_stack_ptr_k, r15
	mov.l @r15, r15
	mov.l r0, @-r15
	mov.l r1, @-r15
	mov.l r2, @-r15

	mov.l expevt_k, r0
	mov.l @r0, r1
	mov.l expected_expevt_k, r2
	mov.l @r2, r2
	cmp/eq r1, r2
	bf general_not_expected
	bra ex_expected
	nop
general_not_expected:
	bra ex_dontcare
	nop
	nop
expevt_k:
	.long 0xFF000024
expected_expevt_k:
	.long expected_expevt
handler_stack_ptr_k:
	.long handler_stack_ptr
	.skip 0x2D4 ! Pad up to 0x400

tlb_exception:
	mov.l handler_stack_ptr, r15
	mov.l r0, @-r15
	mov.l r1, @-r15
	mov.l r2, @-r15

	mov.l expevt1_k, r0
	mov.l @r0, r1
	mov.l expected_expevt1_k, r2
	mov.l @r2, r2
	cmp/eq r1, r2
	bf tlb_not_expected
	bra ex_expected
	nop
tlb_not_expected:
	bra ex_dontcare
	nop
expevt1_k:
	.long 0xFF000024
expected_expevt1_k:
	.long expected_expevt

	.skip 0x1DC ! Pad up to 0x600

irq_raised:
	mov.l handler_stack_ptr, r15
	mov.l r0, @-r15
	mov.l r1, @-r15
	mov.l r2, @-r15

	mov.l intevt_k, r0
	mov.l @r0, r1
	mov.l expected_intevt_k, r2
	mov.l @r2, r2
	cmp/eq r1, r2
	bf ex_dontcare

ex_expected:
	mov.l interrupt_count_k, r0
	mov.l @r0, r2
	add #1, r2
	mov.l r2, @r0
	stc spc, r2
	mov.l interrupt_pc_k, r0
	mov.l r2, @r0

! For most instructions, spc = raising instruction, so add 2 to get the next
! instruction. Exceptions are the slot illegals (need pc+4), and trapa/
! user-break-after-instruction where the pc is already correct
	mov.l slot_illegal_k, r0
	cmp/eq r0, r1
	bt ex_slot_spc
	mov.l slot_fpu_disable_k, r0
	cmp/eq r0, r1
	bt ex_slot_spc
	mov.l trapa_exc_k, r0
	cmp/eq r0, r1
	bt ex_nochain
	mov.l break_after_k, r0
	cmp/eq r0, r1
	bt ex_nochain
! For everything else, spc += 2
	add #2, r2
	ldc r2, spc
	bra ex_nochain
	nop
ex_slot_spc:
	add #4, r2
	ldc r2, spc
	bra ex_nochain
	nop
	
ex_dontcare: ! Not the event we were waiting for.
	mov.l old_vbr_k, r2
	mov.l @r2, r2
	xor r0, r0
	cmp/eq r0, r2
	bt ex_nochain
	
	stc ssr, r0
	mov.l r0, @-r15
	stc spc, r0
	mov.l r0, @-r15
	stc sgr, r0
	mov.l r0, @-r15
	mov.l ex_chainreturn, r0
	ldc r0, spc
	mova handler_stack_ptr, r0
	mov.l r15, @r0
	braf r2 ! Chain on
	nop

ex_chainreturn:
	mov.l handler_stack_ptr, r15
	mov.l @r15+, r0
	ldc r0, sgr
	mov.l @r15+, r0
	ldc r0, spc
	mov.l @r15+, r0
	ldc r0, ssr
	
ex_nochain:	! No previous vbr to chain to
	mova handler_stack_ptr, r0
	mov r15, r1
	add #12, r1
	mov.l r1, @r0
	mov.l @r15+, r2
	mov.l @r15+, r1
	mov.l @r15+, r0
	rte
	stc sgr, r15
	
.align 4
expected_intevt_k:
	.long expected_intevt
interrupt_count_k:
	.long _interrupt_count
interrupt_pc_k:
	.long _interrupt_pc
old_vbr_k:
	.long old_vbr
trapa_k:
	.long 0xFF000020
intevt_k:	
	.long 0xFF000028
	
slot_illegal_k:
	.long 0x000001A0
slot_fpu_disable_k:
	.long 0x00000820
trapa_exc_k:
	.long 0x00000160
break_after_k:
	.long 0x000001E0
	
handler_stack_ptr:
	.long handler_stack_end

handler_stack:
	.skip 0x200
handler_stack_end:	
