.section .text
.code 32
.align 	0
.global	start
.global ___exit
.global _atexit
start:
	b real_entry
	b exception_entry
	b exception_entry /* SWI - not used so jump to general exception */
	b exception_entry
	b exception_entry
	b exception_entry /* Not a vector, but if we ever get here... */
	b irq_entry
	b fiq_entry

/* Start by setting up a stack */
real_entry:
	/*  Set up the stack pointer to a fixed value */
	ldr	r3, .LC0
	mov 	sp, r3
	/* Setup a default stack-limit in-case the code has been
	   compiled with "-mapcs-stack-check".  Hard-wiring this value
	   is not ideal, since there is currently no support for
	   checking that the heap and stack have not collided, or that
	   this default 64k is enough for the program being executed.
	   However, it ensures that this simple crt0 world will not
	   immediately cause an overflow event:  */
	sub	sl, sp, #64 << 10	/* Still assumes 256bytes below sl */
	b .LB0

.syscall:
	.word   -1              /* Syscall # */
	.word   0               /* Arguments */
	.word   0
	.word   0

irq_counter:
	.word 0
fiq_counter:
	.word 0

.LB0:
	/* Zero-out the BSS segment */
	mov 	a2, #0			/* Second arg: fill value */
	mov	fp, a2			/* Null frame pointer */
	mov	r7, a2			/* Null frame pointer for Thumb */
	
	ldr	a1, .LC1		/* First arg: start of memory block */
	ldr	a3, .LC2	
	sub	a3, a3, a1		/* Third arg: length of block */
	bl	memset

	/* Enter main with no arguments for now */
	mov	r0, #0		/*  no arguments  */
	mov	r1, #0		/*  no argv either */
	bl	main

	bl	exit		/* Should not return */

	/* For Thumb, constants must be after the code since only 
	positive offsets are supported for PC relative addresses. */

exception_entry:
	mov r13, #-2
	str r13, .syscall
.die:
	b .die
	
irq_entry:
	/* Increment IRQ counter and return */
	ldr r13, irq_counter
	add r13, r13, #1
	str r13, irq_counter
	subs r15, r14, #4
fiq_entry:
	/* Increment FIQ counter and return */
	ldr r13, fiq_counter
	add r13, r13, #1
	str r13, fiq_counter
	subs r15, r14, #4

	.align 0
.LC0:
	.word   _stack
.LC1:
	.word	__bss_start
.LC2:
	.word	__bss_end


