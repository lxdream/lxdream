.altmacro
.macro	fail name
LOCAL LC1
LOCAL LC2
	add #1, r13
	mov.l LC1, r3
	mov r12, r5
	mov.l \name, r4
	xor r6, r6
	jsr @r3
	nop
	bra LC2
	nop
.align 4
LC1:
	.long _test_print_failure
LC2:	
.endm

.macro failm name msg
LOCAL LC1
LOCAL LC2
	add #1, r13
	mov.l LC1, r3
	mov r12, r5
	mov.l \name, r4
	mov.l \msg, r6
	jsr @r3
	nop
	bra LC2
	nop
.align 4
LC1:
	.long _test_print_failure
LC2:	
.endm


.macro start_test
	mov.l r14, @-r15
	sts.l pr, @-r15
	mov.l r12, @-r15
	mov.l r13, @-r15
	mov r15, r14
	xor r12,r12
	xor r13,r13
! r12 is the test counter
! r13 is the failed-test counter
.endm
	
.macro end_test name
LOCAL test_print_result_k
	mov.l \name, r4
	mov r13, r5
	mov r12, r6
	mov.l test_print_result_k, r3
	jsr @r3
	nop
	mov r14, r15
	mov.l @r15+, r13
	mov.l @r15+, r12
	lds.l @r15+, pr
	mov.l @r15+, r14	
	rts
	nop
.align 4
test_print_result_k:
	.long _test_print_result
.endm	


.macro assert_t_set testname
LOCAL LC1
LOCAL LC2
LOCAL LCM
	stc sr, r1
	mov.l r1, @-r15
	xor r0, r0
	add #1, r0
	and r0, r1
	cmp/eq r0, r1
	bt LC2
	add #1, r13
	mov.l LC1, r3
	mov r12, r5
	mov.l \testname, r4
	mov.l LCM, r6
	jsr @r3
	nop
	bra LC2
	nop
.align 4
LC1:
	.long _test_print_failure
LCM:	.long assert_t_clear_message
LC2:
	mov.l @r15+, r1
	ldc r1, sr
.endm

.macro assert_t_clear testname
LOCAL LC1
LOCAL LC2
LOCAL LCM
	stc sr, r1
	mov.l r1, @-r15
	xor r0, r0
	add #1, r0
	and r0, r1
	cmp/eq r0, r1
	bf LC2
	add #1, r13
	mov.l LC1, r3
	mov r12, r5
	mov.l \testname, r4
	mov.l LCM, r6
	jsr @r3
	nop
	bra LC2
	nop
.align 4
LC1:
	.long _test_print_failure
LCM:	.long assert_t_clear_message
LC2:
	mov.l @r15+, r1
	ldc r1, sr
.endm

! Note that yes there is a perfectly good clrt instruction, but we try to
! minimize the number of instructions we depend on here.
	
.macro clc
	xor r0, r0
	addc r0, r0
.endm
.macro setc
	xor r0, r0
	not r0, r0
	addc r0, r0
.endm


	.align 2
assert_t_set_message:
	.string "Expected T=1 but was 0"

assert_t_clear_message:
	.string "Expected T=0 but was 1"
	