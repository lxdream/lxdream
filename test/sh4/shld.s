.section .text
.include "sh4/inc.s"
!
! Test SHAD/SHLD operation
!
.global _test_shld
_test_shld:	
	start_test
	mov.l r11, @-r15
	mov.l r10, @-r15

test_shld_1:
	mov #8, r11
	mova test_shld_data, r0
	mov r0, r10

test_shld_1_loop1:	
	add #1, r12
	mov.l @r10+, r2
	mov.l @r10+, r3
	mov.l @r10+, r4
	shld r3, r2
	cmp/eq r2, r4
	bt test_shld_1_ok
	fail test_shld_str_k
test_shld_1_ok:	
	dt r11
	bf test_shld_1_loop1

test_shad_1:	! Same again, but using shad 
	mov #8, r11
	mova test_shad_data, r0
	mov r0, r10

test_shad_1_loop1:	
	add #1, r12
	mov.l @r10+, r2
	mov.l @r10+, r3
	mov.l @r10+, r4
	shad r3, r2
	cmp/eq r2, r4
	bt test_shad_1_ok
	fail test_shld_str_k
test_shad_1_ok:	
	dt r11
	bf test_shad_1_loop1

test_shld_end:
	mov.l @r15+, r10
	mov.l @r15+, r11
	end_test test_shld_str_k

test_shld_data:
	.long 0x12345678
	.long 0
	.long 0x12345678
	
	.long 0xA8B9CADB
	.long 0x00000010
	.long 0xCADB0000

	.long 0x8A9BACBD
	.long 0xFFFFFFF0
	.long 0x00008A9B
		
	.long 0x7A9BACBD
	.long 0xFFFFFFF0
	.long 0x00007A9B

	.long 0x7891ACDC
	.long 0x80000000
	.long 0x00000000
	
	.long 0x8719C010
	.long 0x80000000
	.long 0x00000000
	
	.long 0x7891ACDF
	.long 0x7FFFFFFF
	.long 0x80000000
	
	.long 0x8719C01E
	.long 0x000000FF
	.long 0x00000000

test_shad_data:	
	.long 0x12345678
	.long 0
	.long 0x12345678
	
	.long 0xA8B9CADB
	.long 0x00000010
	.long 0xCADB0000
	
	.long 0x8A9BACBD
	.long 0xFFFFFFF0
	.long 0xFFFF8A9B

	.long 0x7A9BACBD
	.long 0xFFFFFFF0
	.long 0x00007A9B

	.long 0x7891ACDC
	.long 0x80000000
	.long 0x00000000
	
	.long 0x8719C010
	.long 0x80000000
	.long 0xFFFFFFFF
	
	.long 0x7891ACDF
	.long 0x7FFFFFFF
	.long 0x80000000
	
	.long 0x8719C01E
	.long 0x000000FF
	.long 0x00000000
				
test_shld_data_end:	
	.align 4
test_shld_data_end_k:
	.long test_shld_data_end	
test_shld_str_k:
	.long test_shld_str
test_shld_str:
	.string "SHLD"
	