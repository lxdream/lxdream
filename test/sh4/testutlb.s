

.global _install_utlb_test_handler
_install_utlb_test_handler:
	mov.l utlb_old_vbr_k, r0
	stc vbr, r1
	mov.l r1, @r0
	
	mov.l utlb_vbr_k, r0
	ldc r0, vbr
	rts
	nop

.global _uninstall_utlb_test_handler
_uninstall_utlb_test_handler:
	mov.l utlb_old_vbr_k, r0
	mov.l @r0, r0
	ldc r0, vbr
	rts
	nop

utlb_exc_handler:
	mov.l utlb_exc_stack_k, r15
	mov.l r0, @-r15
	mov.l r1, @-r15
	mov.l r2, @-r15
	sts pr, r0
	mov.l r0, @-r15

	mov.l mmu_expevt_k, r2
	mov.l @r2, r2
	mov r2, r0
	shlr2 r0
	cmp/eq #0x58, r0
	bt utlb_trap
	
	mov.l utlb_exc_k_1, r0
	mov.l @(16,r0), r1
	tst r1, r1
	bt utlb_exc_unexpected
	
	mov.l @r0, r1
	tst r1, r1
	bf utlb_exc_set
	mov.l r2, @r0
	stc spc, r2
	mov.l r2, @(8,r0)
	
utlb_exc_set:
	mov.l @(4,r0), r2
	add #1, r2
	mov.l r2, @(4,r0)
	
	mov.l @(12,r0), r2
	stc spc, r1
	add r2, r1
	ldc r1, spc
	
utlb_exc_done:
	mov.l @r15+, r0
	lds r0, pr
	mov.l @r15+, r2
	mov.l @r15+, r1
	mov.l @r15+, r0
	rte
	stc sgr, r15

utlb_trap: 
	mov.l mmu_tra_k, r2
	mov.l @r2, r0
	shlr2 r0
	cmp/eq #42, r0
	bt utlb_trap_priv
	cmp/eq #41, r0
	bt utlb_trap_printf
	bra utlb_exc_done
	nop
utlb_trap_priv:
	stc ssr, r0
	mov.l mmu_sr_md, r1
	or r1, r0
	ldc r0, ssr
	bra utlb_exc_done
	nop
utlb_trap_printf:
	stc sr, r0
	mov.l mmu_sr_rb_mask, r1
	and r1, r0
	ldc r0, sr
	mov.l utlb_printf_k, r0
	jsr @r0
	nop
	bra utlb_exc_done
	nop
utlb_exc_unexpected: /* Report an unexpected exception. Save everything in case printf clobbers it */
	mov.l r3, @-r15
	mov.l r4, @-r15
	mov.l r5, @-r15
	mov.l r6, @-r15
	mov.l r7, @-r15
	mov.l r8, @-r15
	mov.l r9, @-r15
	mov.l r10, @-r15
	mov.l r11, @-r15
	mov.l r12, @-r15
	mov.l r13, @-r15
	mov.l r14, @-r15
	sts fpscr, r0
	mov.l r0, @-r15
	
	stc spc, r6
	mov r2, r5
	mov.l utlb_unexpected_msg_k, r4
	mov.l utlb_printf_k, r3
	jsr @r3
	nop
	
	mov.l @r15+, r0
	lds r0, fpscr
	mov.l @r15+, r14
	mov.l @r15+, r13
	mov.l @r15+, r12
	mov.l @r15+, r11
	mov.l @r15+, r10
	mov.l @r15+, r9
	mov.l @r15+, r8
	mov.l @r15+, r7
	mov.l @r15+, r6
	mov.l @r15+, r5
	mov.l @r15+, r4
	mov.l @r15+, r3
	mov.l utlb_exc_k_1, r0
	bra utlb_exc_set
	nop
	
	
.align 4
utlb_vbr_k:
	.long utlb_vbr
utlb_old_vbr_k:
	.long utlb_old_vbr
utlb_exc_k_1:
	.long utlb_exc
mmu_expevt_k:
	.long 0xFF000024
mmu_tra_k:
	.long 0xFF000020
mmu_sr_md:
	.long 0x40000000
mmu_sr_rb_mask:
	.long 0x50000000
utlb_exc_stack_k:
	.long utlb_exc_stack
utlb_printf_k:
	.long _printf
utlb_unexpected_msg_k:
	.long utlb_unexpected_msg
	
	.skip 0x1F00 /* 8K stack */
utlb_vbr:
	.skip 0x100
utlb_exc_stack:
	mov.l utlb_exc_handler_k1, r15
	jmp @r15
	nop
	nop
utlb_exc_handler_k1:
	.long utlb_exc_handler
	
	.skip 0x2F4
	mov.l utlb_exc_handler_k2, r15
	jmp @r15
	nop
	nop
utlb_exc_handler_k2:
	.long utlb_exc_handler
	.skip 0x1F4
	rte
	stc sgr, r15


utlb_expect_exc:
	mova utlb_exc, r0
	xor r1, r1
	mov.l r1, @r0
	mov.l r1, @(4,r0)
	mov.l r1, @(8,r0)
	mov #1, r1
	mov.l r1, @(16,r0)
	mov #2, r1
	mov.l r1, @(12,r0)
	rts
	nop

utlb_noexpect_exc:
	mova utlb_exc, r0
	xor r1, r1
	mov.l r1, @r0
	mov.l r1, @(4,r0)
	mov.l r1, @(8,r0)
	mov.l r1, @(16,r0)
	mov #2, r1
	mov.l r1, @(12,r0)
	rts
	nop
	
/* Check the result of a read test. Call with:
 *   r0 = expected spc 
 *   r1 = value read (if any)
 *   r9 = (char *) testname
 *   r10 = test VMA
 *   r11 = test PMA
 *   r12 = expected exc
 *
 * Trashes r0..r7
 */
utlb_check_read_exc:
	mov.l utlb_exc_k, r3
	mov.l addr_mask, r2
	and r2, r3
	
	mov.l @r3, r2
	cmp/eq r2, r12
	bf test_read_exc_bad
	tst r12, r12
	bt test_read_ok /* Expected no exception, and got none */
	mov.l @(4,r3), r2
	dt r2
	bf test_read_count_bad
	mov.l @(8,r3), r2
	cmp/eq r0, r2
	bt test_read_ok
test_read_pc_bad:
	add #1, r14
	mov r0, r6
	mov.l err_read_pc_msg_k, r4
	mov r2, r7
	mov r9, r5
	trapa #41
	bra test_read_ok
	nop
test_read_count_bad:
	add #1, r14
	add #2, r2
	mov.l err_read_count_msg_k, r4
	mov r2, r6
	mov r9, r5
	trapa #41
	bra test_read_ok
	nop	
test_read_exc_bad:
	add #1, r14
	mov.l err_read_exc_msg_k, r4
	mov r12, r6
	mov r2, r7
	mov r9, r5
	trapa #41 
	bra test_read_ok
	nop
test_read_ok:
	bra utlb_expect_exc
	nop

/* Check the result of a write test (and clears the exception). Call with:
 *   r0 = expected spc 
 *   r1 = written value
 *   r9 = (char *) testname
 *   r10 = test VMA
 *   r11 = test PMA
 *   r13 = expected exc
 *   r14 = fail count (updated)
 *
 * Trashes r0..r7
 */
utlb_check_write_exc:
	mov.l utlb_exc_k, r3
	mov.l addr_mask, r2
	and r2, r3

	mov.l @r3, r2
	cmp/eq r2, r13
	bf test_write_exc_bad
	tst r13, r13
	bt test_write_ok /* Expected no exception, and got none */
	mov.l @(4,r3), r2
	dt r2
	bf test_write_count_bad
	mov.l @(8,r3), r2
	cmp/eq r0, r2
	bt test_write_ok
test_write_pc_bad:
	add #1, r14
	mov r0, r6
	mov.l err_write_pc_msg_k, r4
	mov r2, r7
	mov r9, r5
	trapa #41
	bra test_write_ok
	nop
test_write_count_bad:
	add #1, r14
	add #1, r2
	mov.l err_write_count_msg_k, r4
	mov r2, r6
	mov r9, r5
	trapa #41
	bra test_write_ok
	nop	
test_write_exc_bad:
	add #1, r14
	mov.l err_write_exc_msg_k, r4
	mov r13, r6
	mov r2, r7
	mov r9, r5 
	trapa #41
	bra test_write_ok
	nop
test_write_ok:
	bra utlb_expect_exc
	nop
.align 4
utlb_exc_k:
	.long utlb_exc
err_read_exc_msg_k:
	.long err_read_exc_msg
err_read_count_msg_k:
	.long err_read_count_msg
err_read_pc_msg_k:
	.long err_read_pc_msg
err_write_exc_msg_k:
	.long err_write_exc_msg
err_write_count_msg_k:
	.long err_write_count_msg
err_write_pc_msg_k:
	.long err_write_pc_msg

.align 4
utlb_old_vbr:
	.long 0
utlb_exc:
	.long 0
utlb_exc_count:
	.long 0
utlb_exc_spc:
	.long 0
utlb_rte_offset:
	.long 2
utlb_expected:
	.long 0

.global _run_utlb_priv_test
_run_utlb_priv_test:
	mov.l r14, @-r15
	sts pr, r0
	mov.l r0, @-r15
	mov.l r13, @-r15
	mov.l r12, @-r15
	mov.l r11, @-r15
	mov.l r10, @-r15
	mov.l r9, @-r15
	mov.l r8, @-r15
	
	mov.l @(0,r4), r9  /* Test name */
	mov.l @(4,r4), r10 /* Test VMA */
	mov.l @(8,r4), r11 /* Test PMA */
	mov.l @(12,r4), r12 /* Read exception */
	mov.l @(16,r4), r13 /* Write exception */
	xor r14, r14 /* Fail count */

	mov.l @r11, r0 /* Save original memory value */
	ocbp @r11
	mov.l r0, @-r15
	
	tst r12, r12
	bt utlb_read_test_noexc

/* Exception test cases - all should fail with the same exception */	
utlb_read_test_exc:
	mov r10, r8
	bsr utlb_expect_exc
	nop

/* Test mov.l Rm, Rn */
	mova test_readl_1, r0
.align 4		
test_readl_1:
	mov.l @r10, r1
	bsr utlb_check_read_exc
	nop

/* Test mov.l @Rm+, Rn */
	mova test_readl_2, r0
.align 4
test_readl_2:
	mov.l @r8+, r1 
	bsr utlb_check_read_exc
	nop
	cmp/eq r8,r10
	bt test_readl_2_ok
	
	add #1, r14
	mov.l err_readlp_bad_msg_k, r4
	mov r9, r5
	trapa #41

test_readl_2_ok:
/* Test mov.w @Rm, Rn */
	mova test_readw_1, r0
.align 4
test_readw_1:
	mov.w @r10, r1
	bsr utlb_check_read_exc
	nop
	
/* Test mov.w @Rm+, Rn */
	mova test_readw_2, r0
.align 4
test_readw_2:
	mov.w @r8+, r1
	bsr utlb_check_read_exc
	nop
	cmp/eq r8, r10
	bt test_readw_2_ok
	
	add #1, r14
	mov.l err_readwp_bad_msg_k, r4
	mov r9, r5
	trapa #41

test_readw_2_ok:

/* Test mov.b @Rm, Rn */
	mova test_readb_1, r0
.align 4
test_readb_1:
	mov.b @r10, r1
	bsr utlb_check_read_exc
	nop
	
/* Test mov.b @Rm+, Rn */
	mova test_readb_2, r0
.align 4
test_readb_2:
	mov.b @r8+, r1
	bsr utlb_check_read_exc
	nop
	cmp/eq r8, r10
	bt test_readb_2_ok
	
	add #1, r14
	mov.l err_readbp_bad_msg_k, r4
	mov r9, r5
	trapa #41

test_readb_2_ok:
	
	bra utlb_read_test_done
	nop

/* Non-exception read tests */
utlb_read_test_noexc:
	mov.l utlb_exc_k_2, r8
	mov.l addr_mask, r0
	and r0, r8
	bsr utlb_noexpect_exc
	nop
	
	mov.l @r10, r1
	not r1, r2
	mov.l r2, @r11
	ocbp @r11
	ocbi @r10
	mov.l @r10, r1
	ocbi @r10
	cmp/eq r1, r2
	bt noexc_readl_ok
	

	add #1, r14
	mov.l err_read_mismatch_msg_k, r4
	mov r9, r5
	trapa #41
	
noexc_readl_ok:

	mov.l @(4,r8), r0
	add r0, r14
utlb_read_test_done:

/*********************** Begin write tests *****************************/
	tst r13, r13
	bt utlb_write_test_noexc

/* Exception write tests */
utlb_write_test_exc:
	bsr utlb_expect_exc
	nop
	
	mova test_writel_pc, r0
.align 4
test_writel_pc:
	mov.l r1, @r10
	bsr utlb_check_write_exc
	nop

	mova test_writelp_pc, r0
	mov r10, r8
.align 4
test_writelp_pc:
	mov.l r1, @-r8
	bsr utlb_check_write_exc
	nop
	cmp/eq r8, r10
	bt test_writelp_ok
	add #1, r14
	mov.l err_writelp_bad_msg_k, r4
	mov r9, r5
	trapa #41
test_writelp_ok:

	mova test_writew_pc, r0
.align 4
test_writew_pc:
	mov.w r1, @r10
	bsr utlb_check_write_exc
	nop

	mova test_writewp_pc, r0
	mov r10, r8
.align 4
test_writewp_pc:
	mov.w r1, @-r8
	bsr utlb_check_write_exc
	nop
	cmp/eq r8, r10
	bt test_writewp_ok
	add #1, r14
	mov.l err_writewp_bad_msg_k, r4
	mov r9, r5
	trapa #41
test_writewp_ok:

	mova test_writeb_pc, r0
.align 4
test_writeb_pc:
	mov.b r1, @r10
	bsr utlb_check_write_exc
	nop

	mova test_writebp_pc, r0
	mov r10, r8
.align 4
test_writebp_pc:
	mov.b r1, @-r8
	bsr utlb_check_write_exc
	nop
	cmp/eq r8, r10
	bt test_writebp_ok
	add #1, r14
	mov.l err_writelp_bad_msg_k, r4
	mov r9, r5
	trapa #41
test_writebp_ok:

	bra utlb_write_test_done
	nop
	
/* Non-exception write tests */
utlb_write_test_noexc:
	mov.l utlb_exc_k_2, r8
	mov.l addr_mask, r0
	and r0, r8
	bsr utlb_noexpect_exc
	nop
	
	mov.l @r11, r7
	ocbp @r11
	not r7, r1

	mov.l r1, @r10
	ocbp @r10
	mov.l @r11, r6
	cmp/eq r6, r1
	bt test_writel_1_ok
	
	add #1, r14
	mov.l err_write_ignored_msg_k, r4
	mov r9, r5
	trapa #41
	
test_writel_1_ok:
	
	mov.l @(4,r8), r0
	add r0, r14
utlb_write_test_done:
	
	xor r0, r0
	mov.l r0, @(16,r8)
		
	mov.l @r15+, r0
	mov.l r0, @r11
	mov.l @r15+, r8
	mov.l @r15+, r9
	mov.l @r15+, r10
	mov.l @r15+, r11
	mov.l @r15+, r12
	mov.l @r15+, r13
	mov.l @r15+, r1
	lds r1, pr
	mov r14, r0
	mov.l @r15+, r14
	rts
	nop

.align 4
err_read_mismatch_msg_k:
	.long err_read_mismatch_msg
err_write_ignored_msg_k:
	.long err_write_ignored_msg
err_readlp_bad_msg_k:
	.long err_readlp_bad_msg
err_readwp_bad_msg_k:
	.long err_readwp_bad_msg
err_readbp_bad_msg_k:
	.long err_readbp_bad_msg
err_writelp_bad_msg_k:
	.long err_readlp_bad_msg
err_writewp_bad_msg_k:
	.long err_readwp_bad_msg
err_writebp_bad_msg_k:
	.long err_readbp_bad_msg



.global _run_utlb_user_test
_run_utlb_user_test:
	sts pr, r0
	mov.l r0, @-r15
	stc sr, r2
	mov.l sr_mask, r1
	and r1, r2

	mova user_entry_point, r0
	mov.l addr_mask, r1
	and r1, r0

	mov.l r15, @-r15
	
	jmp @r0
	nop

	nop
user_entry_point:
	ldc r2, sr 	 

	/* In user mode */
	
	mov.l main_test_k, r0
	and r1, r0
	and r1, r4
	and r1, r15
	jsr @r0
	nop

	/* Done, return to priv mode */
	trapa #42
	mov.l user_retaddr, r1
	jmp @r1
	nop
		
user_exit_point:
	/* Back to priv mode */
	mov.l @r15+, r15
	mov.l @r15+, r1
	lds r1, pr
	rts
	nop
.align 4

utlb_exc_k_2: 
	.long utlb_exc
main_test_k:
	.long _run_utlb_priv_test
addr_mask:
	.long 0x1FFFFFFF
sr_mask:
	.long 0x3FFFFFFF
user_retaddr:
	.long user_exit_point

err_read_exc_msg:
	.string "%s: Read failed: Expected Exc %04X but got %04X\n"
err_read_count_msg:
	.string "%s: Read bad exception: Exception 1 exception, but got %d\n"
err_read_pc_msg:
	.string "%s: Read bad exception: Expected PC=%08X but was %08X\n"
err_read_mismatch_msg:
	.string "%s: Read failed: Data mismatch!\n"
err_readlp_bad_msg:
	.string "%s: Mov.l @Rm+, Rn failed: Rm changed!\n"
err_readwp_bad_msg:
	.string "%s: Mov.w @Rm+, Rn failed: Rm changed!\n"
err_readbp_bad_msg:
	.string "%s: Mov.b @Rm+, Rn failed: Rm changed!\n"
err_writelp_bad_msg:
	.string "%s: Mov.l Rm, @-Rn failed: Rm changed!\n"
err_writewp_bad_msg:
	.string "%s: Mov.w Rm, @-Rn failed: Rm changed!\n"
err_writebp_bad_msg:
	.string "%s: Mov.b Rm, @-Rn failed: Rm changed!\n"
err_write_exc_msg:
	.string "%s: Write failed: Expected Exc %04X but got %04X\n"
err_write_count_msg:
	.string "%s: Write bad exception: Expected 1 exception, but got %d\n"
err_write_pc_msg:
	.string "%s: Write bad exception: Expected PC=%08X but was %08X\n"
err_write_ignored_msg:
	.string "%s: Write failed: write didn't happen!\n"
utlb_unexpected_msg:
	.string "*** Unexpected exception %04X at %08X!\n"
