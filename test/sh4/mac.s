.section .text
.include "sh4/inc.s"
!
! Test MAC Rm,Rn operation
!
.global _test_mac
_test_mac:
	start_test

test_macl_1: ! Basic mac.l ops.
	add #1, r12
	clrmac
	clrs
	mov.l test_macl_1_inputs_k, r0
	mov.l test_macl_1_results_k, r6
	mov r0, r1
	mac.l @r0+, @r1+
	sts macl, r2
	sts mach, r3
	mov.l @r6+, r4
	cmp/eq r2, r4
	bf test_macl_1_fail
	xor r5, r5
	cmp/eq r3, r5
	bf test_macl_1_fail
	cmp/eq r0, r1
	bf test_macl_1_fail
	mov.l test_macl_1_inputs_k, r0
	cmp/eq r0, r1
	bt test_macl_1_fail
	add #-4, r1
	cmp/eq r0, r1
	bf test_macl_1_fail

	mac.l @r0+, @r0+
	sts macl, r2
	sts mach, r3
	mov.l @r6+, r4
	cmp/eq r2, r4
	bf test_macl_1_fail
	xor r5, r5
	cmp/eq r3, r5
	bf test_macl_1_fail
	add #8, r1
	cmp/eq r0, r1
	bf test_macl_1_fail

	mac.l @r0+, @r1+
	sts macl, r2
	sts mach, r3
	mov.l @r6+, r4
	cmp/eq r2, r4
	bf test_macl_1_fail
	mov.l @r6+, r5
	cmp/eq r3, r5
	bf test_macl_1_fail
	bra test_macl_2
	nop
test_macl_1_fail:
	fail test_mac_str_k
	bra test_macl_2
	nop
test_macl_1_inputs_k:
	.long test_macl_1_inputs
test_macl_1_inputs:
	.long 0x00000010
	.long 0x00000021
	.long 0xF0000002
test_macl_1_results_k:
	.long test_macl_1_results
test_macl_1_results:
	.long 0x00000100
	.long 0x00000310
	.long 0xC0000314
	.long 0x00FFFFFF

test_macl_2: ! Test saturation
	add #1, r12
	sets
	mova test_macl_2_results, r0
	mov r0, r3
	mova test_macl_2_inputs, r0
	mac.l @r0+, @r0+
	sts macl, r1
	mov.l @r3+, r2
	cmp/eq r1, r2
	bf test_macl_2_fail
	sts mach, r1
	mov.l @r3+, r2
	cmp/eq r1, r2
	bf test_macl_2_fail
	mov r0, r1
	mova test_macl_2_inputs, r0
	add #8, r0
	cmp/eq r0, r1
	bf test_macl_2_fail

	mac.l @r0+, @r0+	
	sts macl, r1
	mov.l @r3+, r2
	cmp/eq r1, r2
	bf test_macl_2_fail
	sts mach, r1
	mov.l @r3+, r2
	cmp/eq r1, r2
	bt test_macw_1

test_macl_2_fail:
	fail test_mac_str_k
	bra test_macw_1
	nop
test_macl_2_inputs:
	.long 0x00000000
	.long 0x00000010
	.long 0x7FFFFFDB
	.long 0x800000EC
	
test_macl_2_results:
	.long 0xFFFFFFFF
	.long 0x00007FFF
	.long 0x00000000
	.long 0xFFFF8000

test_macw_1:
	add #1, r12
	clrs
	clrmac

	mova test_macw_1_results, r0
	mov r0, r4
	mova test_macw_1_inputs, r0
	mov r0, r1
	mac.w @r0+, @r1+
	sts macl, r2
	mov.l @r4+, r3
	cmp/eq r2, r3
	bf test_macw_1_fail
	sts mach, r2
	tst r2,r2
	bf test_macw_1_fail
	cmp/eq r0, r1
	bf test_macw_1_fail
	mova test_macw_1_inputs, r0
	add #-2, r1
	cmp/eq r0, r1
	bf test_macw_1_fail
	
	mac.w @r0+, @r0+
	sts macl, r2
	mov.l @r4+, r3
	cmp/eq r2, r3
	bf test_macw_1_fail
	sts mach, r2
	tst r2, r2
	bf test_macw_1_fail
	add #4, r1
	cmp/eq r0, r1
	bf test_macw_1_fail

	add #2, r1
	mac.w @r0+, @r1+
	sts macl, r2
	mov.l @r4+, r3
	cmp/eq r2, r3
	bf test_macw_1_fail
	sts mach, r2
	tst r2, r2
	bf test_macw_1_fail
	bra test_macw_2
	nop
	
test_macw_1_fail:
	fail test_mac_str_k
	bra test_macw_2
	nop
test_macw_1_inputs:
	.long 0x00210014
	.long 0x0002FFFF
test_macw_1_results:
	.long 0x00000190
	.long 0x00000424
	.long 0x00000422
	
test_macw_2:
	add #1, r12
	sets
	clrmac
	xor r0, r0
	not r0, r0
	lds r0, mach

	mova test_macw_2_results, r0
	mov r0, r3
	mova test_macw_2_inputs, r0
	mov #3, r6
test_macw_2_loop:	
	mac.w @r0+, @r0+
	sts macl, r1
	mov.l @r3+, r2
	cmp/eq r1, r2
	bf test_macw_2_fail
	sts mach, r1
	mov.l @r3+, r2
	cmp/eq r1, r2
	bf test_macw_2_fail
	dt r6
	bf test_macw_2_loop
	
	clrmac
	mov #3, r6
test_macw_2_loop_2:	
	mac.w @r0+, @r0+
	sts macl, r1
	mov.l @r3+, r2
	cmp/eq r1, r2
	bf test_macw_2_fail
	sts mach, r1
	mov.l @r3+, r2
	cmp/eq r1, r2
	bf test_macw_2_fail
	dt r6
	bf test_macw_2_loop_2
	bra test_mac_end
	nop	
	
test_macw_2_fail:
	fail test_mac_str_k
	bra test_mac_end
	nop
test_macw_2_inputs:
	.long 0x7FFE7FFF
	.long 0x7FFF7FFD
	.long 0x7FFB7FFC
	.long 0x80007FF1
	.long 0x7FF28003
	.long 0x80047FF5
test_macw_2_results:
	.long 0x3FFE8002
	.long 0xFFFFFFFF
	.long 0x7FFC8005
	.long 0xFFFFFFFF
	.long 0x7FFFFFFF
	.long 0x00000001
	.long 0xC0078000
	.long 0x00000000
	.long 0x800FFFD6
	.long 0x00000000
	.long 0x80000000
	.long 0x00000001
	
test_mac_end:	
	end_test test_mac_str_k
			
test_mac_data_end:	
	.align 4
test_mac_data_end_k:
	.long test_mac_data_end	
test_mac_str_k:
	.long test_mac_str
test_mac_str:
	.string "MAC"
	