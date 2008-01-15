# 
# Scan back through the stack until we hit the currently executing
# translation block, and find the call return address to that block.
# 
# Implementation: iterate back through each stack frame until we find
# a frame that has a saved %ebp == sh4r (setup by the xlat blocks).
# The return address is then the stack value immediately before the
# saved %ebp.
#
# At most 8 stack frames are checked, to prevent infinite looping on a
# corrupt stack.

.global xlat_get_native_pc
xlat_get_native_pc:
	mov %rbp, %rax
	mov $0x8, %ecx
	mov $sh4r, %rdx

frame_loop:
	test %rax, %rax
	je frame_not_found
	cmpq (%rax), %rdx
	je frame_found
	sub $0x1, %ecx
	je frame_not_found
	movq (%rax), %rax
	jmp frame_loop

frame_found:
	movl 0x4(%rax), %rax
	ret
frame_not_found:
	xor %rax, %rax
	ret
