#
# Assembler functions to invoke the non-standard FP operations
#
.global _clear_fpscr
_clear_fpscr:
	xor r0, r0
	lds r0, fpscr
	rts
	nop

.global _get_fpscr
_get_fpscr:
	rts
	sts fpscr, r0

.global _set_fpscr
_set_fpscr:
	rts
	lds r4, fpscr

.global _do_fsca
_do_fsca:
	sts fpscr, r0
	mov #0x08, r1
	shll16 r1
	not r1, r1
	and r0, r1
	lds r1, fpscr
	lds r4, fpul
	.word 0xF0FD
	fmov fr0, @r5
	fmov fr1, @r6
	xor r0,r0
	rts
	lds r1, fpscr

.global _do_fsrra
_do_fsrra:
	sts fpscr, r0
	mov #0x08, r1
	shll16 r1
	not r1, r1
	and r0, r1
	lds r1, fpscr
	fmov fr4, fr0
	.word 0xF07D
	rts
	lds r0, fpscr

.global _do_fipr
_do_fipr:
	sts fpscr, r0
	mov #0x08, r1
	shll16 r1
	not r1, r1
	and r0, r1
	lds r1, fpscr
	fmov.s @r4+, fr0
	fmov.s @r4+, fr1
	fmov.s @r4+, fr2
	fmov.s @r4+, fr3
	fmov.s @r5+, fr4
	fmov.s @r5+, fr5
	fmov.s @r5+, fr6
	fmov.s @r5+, fr7
	fipr fv0, fv4
	lds r0, fpscr
	rts
	fmov fr7, fr0

.global _do_fipr2
_do_fipr2:
	sts fpscr, r0
	mov #0x08, r1
	shll16 r1
	not r1, r1
	and r0, r1
	lds r1, fpscr
	fschg
	fmov @r4+, dr0
	fmov @r4+, dr2
	fmov @r5+, dr4
	fmov @r5+, dr6
	fschg
	fipr fv0, fv4
	lds r0, fpscr
	rts
	fmov fr7, fr0

.global _do_ftrv
_do_ftrv:
	sts fpscr, r0
	mov #0x08, r1
	shll16 r1
	not r1, r1
	and r0, r1
	lds r1, fpscr
	fschg
	fmov @r4+, xd0
	fmov @r4+, xd2
	fmov @r4+, xd4
	fmov @r4+, xd6
	fmov @r4+, xd8
	fmov @r4+, xd10
	fmov @r4+, xd12
	fmov @r4, xd14
	fmov @r5+, dr0
	fmov @r5, dr2
	ftrv xmtrx, fv0
	fmov dr2, @r5
	fmov dr0, @-r5
	fschg
	rts
	lds r0, fpscr

.global _do_ftrv2
_do_ftrv2:
	sts fpscr, r0
	mov #0x08, r1
	shll16 r1
	not r1, r1
	and r0, r1
	lds r1, fpscr
	fschg
	frchg
	fmov @r4+, dr0
	fmov @r4+, dr2
	fmov @r4+, dr4
	fmov @r4+, dr6
	fmov @r4+, dr8
	fmov @r4+, dr10
	fmov @r4+, dr12
	fmov @r4+, dr14
	frchg
	fmov @r5+, dr0
	fmov @r5, dr2
	ftrv xmtrx, fv0
	fmov dr2, @-r5
	fmov dr0, @-r5
	fschg
	rts
	lds r0, fpscr
