.section .text
.include "sh4/inc.s"

.global _test_fsrra
_test_fsrra:
	start_test
	mov.l r11, @-r15
	mov.l test_fsrra_data_k, r11
	
test_fsrra_loop:
	mov.l test_fsrra_data_end_k, r4
	cmp/eq r11, r4
	bt test_fsrra_end
	add #1, r12

	fmov @r11+, fr5
	fsrra fr5
        flds fr5, fpul
        sts fpul, r5
	mov.l @r11+, r4
	cmp/eq r4, r5
	bt test_fsrra_loop
	
	mov.l test_fsrra_error_k, r3
	jsr @r3
	fail test_fsrra_str_k
	bra test_fsrra_loop
	nop

test_fsrra_end:
	mov.l @r15+, r11
	end_test test_fsrra_str_k

	.align 4	
test_fsrra_data_k:
	.long test_fsrra_data
test_fsrra_data:
test_fsrra_data_1:
	.long 0x3F800000
	.long 0x3F800000

	.long 0x43214321
	.long 0x3da14613

	.long 0x3B0D693E
	.long 0x41ac38ca

	.long 0x3B5D87B7
	.long 0x41899934
	
        .long 0x41899934
	.long 0x3e76e8e3

#	.long 0xFFFFFFF0
#	.long 0x7fbfffff

	.long 0x00000040
	.long 0x633504f3

	.long 0xFFFFFF80
	.long 0xffffff80

	.long 0x00000001
	.long 0x64b504f3

	.long 0x98765432
	.long 0xffc00000

	.long 0x64b504f3
	.long 0x2cd744fd

	.long 0x2cd744fd
	.long 0x48c5672a
	
test_fsrra_data_end:	
	.align 4
test_fsrra_data_end_k:
	.long test_fsrra_data_end	
test_fsrra_str_k:
	.long test_fsrra_str
test_fsrra_error_k:
	.long _test_print_float_error
test_fsrra_str:
	.string "FSRRA"
	