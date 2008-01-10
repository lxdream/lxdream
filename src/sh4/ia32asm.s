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
	mov %ebp, %eax
	mov $0x8, %ecx
	mov $sh4r, %edx

frame_loop:
	test %eax, %eax
	je frame_not_found
	cmp (%eax), %edx
	je frame_found
	sub $0x1, %ecx
	je frame_not_found
	movl (%eax), %eax
	jmp frame_loop

frame_found:
	movl 0x4(%eax), %eax
	ret
frame_not_found:
	xor %eax, %eax
	ret
