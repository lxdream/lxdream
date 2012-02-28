.section .text
.include "sh4/inc.s"

.global _test_trapa
_test_trapa:	
	start_test

test_trapa_1:
	add #1, r12
	expect_exc 0x00000160
	trapa #42

test_trapa_1_pc:
	assert_exc_caught test_trapa_str_k test_trapa_1_pc

	mov.l test_trapa_tra, r1
	mov.l @r1, r2
	mov #42, r0
	shll r0
	shll r0
	cmp/eq r0, r2
	bt test_trapa_end
	fail test_trapa_str_k
	bra test_trapa_end
	nop
		
test_trapa_tra:
	.long 0xFF000020
	
test_trapa_end:
	end_test test_trapa_str_k

test_trapa_str_k:
	.long test_trapa_str
test_trapa_str:
	.string "TRAPA"
