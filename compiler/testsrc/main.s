	.Ltext0:
	.cfi_sections	.debug_frame
	.file 0 "testsrc/main.ca"
	.attribute unaligned_access, 0
	.file 1 "testsrc/main.ca"
	.globl testAsmFunction
	.type testAsmFunction, @function
testAsmFunction:
	.loc 1 4 6
	.cfi_startproc
	addi sp, sp, -4
	sw ra, 0(sp)
	.cfi_offset 1, -4
	addi sp, sp, -4
	sw fp, 0(sp)
	.cfi_offset 8, -8
	mv fp, sp
	lb t0, 8(fp)
	mul t0, t0, t0
	mv a0, t0
	lwu fp, 0(sp)
	addi sp, sp, 4
	.cfi_restore 8
	lwu ra, 0(sp)
	addi sp, sp, 4
	.cfi_restore 1
	addi sp, sp, 1
	.cfi_def_cfa_offset 0
	jalr zero, 0(ra)
	.cfi_endproc
	.size testAsmFunction, .-testAsmFunction
	.globl _start
_start:
	li sp, 0x81000000
	call main
	pgm_done:
	wfi
	j pgm_done
	.globl main
	.type main, @function
main:
	.loc 1 11 15
	.cfi_startproc
	addi sp, sp, -4
	sw ra, 0(sp)
	.cfi_offset 1, -4
	addi sp, sp, -4
	sw fp, 0(sp)
	.cfi_offset 8, -8
	mv fp, sp
	addi sp, sp, -4
	sw a2, 0(sp)
	addi sp, sp, -4
	sw a1, 0(sp)
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
main_0:
	.loc 1 13
	call uart_init@plt
	.loc 1 14
	li t0, 65 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)
	.loc 1 15
	call uart_putc@plt
	.loc 1 16
	li t0, 0 # place literal
	# Write register variable main_n
	mv a1, t0
	j main_2
main_2:
	.loc 1 16
	.loc 1 18
	li t1, 20 # place literal
	bgeu a1, t1, main_1
	addi sp, sp, -1
	sb a1, 0(sp)
	.loc 1 19
	call testAsmFunction
	# Write register variable .t0
	mv a2, a0
	addi sp, sp, -4
	sw a2, 0(sp)
	call printNum@plt
	.loc 1 21
	li t1, 1 # place literal
	add a2, a1, t1
	# Write register variable main_n
	mv a1, a2
	j main_3
main_3:
	.loc 1 16
	j main_2
main_1:
main_done:
	lwu t0, 0(sp)
	addi sp, sp, 4
	lwu t1, 0(sp)
	addi sp, sp, 4
	lwu a1, 0(sp)
	addi sp, sp, 4
	lwu a2, 0(sp)
	addi sp, sp, 4
	lwu fp, 0(sp)
	addi sp, sp, 4
	.cfi_restore 8
	lwu ra, 0(sp)
	addi sp, sp, 4
	.cfi_restore 1
	.cfi_def_cfa_offset 0
	jalr zero, 0(ra)
	.cfi_endproc
	.size main, .-main
