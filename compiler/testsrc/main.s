	.Ltext0:
	.cfi_sections	.debug_frame
	.file 0 "testsrc/main.ca"
	.attribute unaligned_access, 0
	.file 1 "testsrc/main.ca"
	.globl fib
	.type fib, @function
fib:
	.loc 1 4 6
	.cfi_startproc
	addi sp, sp, -4
	sw ra, 0(sp)
	.cfi_offset 1, -4
	addi sp, sp, -4
	sw fp, 0(sp)
	.cfi_offset 8, -8
	mv fp, sp
	addi sp, sp, -4
	sw a3, 0(sp)
	addi sp, sp, -4
	sw a2, 0(sp)
	addi sp, sp, -4
	sw a1, 0(sp)
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
	lbu a1, 8(fp) # place fib_n
fib_0:
	.loc 1 7
	li t1, 1 # place literal
	bleu a1, t1, fib_2
	j fib_3
fib_3:
	.loc 1 8
	li t1, 1 # place literal
	sub a2, a1, t1
	addi sp, sp, -1
	sb a2, 0(sp)
	call fib
	# Write register variable .t2
	mv a2, a0
	.loc 1 9
	li t1, 2 # place literal
	sub a3, a1, t1
	addi sp, sp, -1
	sb a3, 0(sp)
	call fib
	# Write register variable .t4
	mv a3, a0
	add a2, a2, a3
	mv a0, a2
	j fib_done
	j fib_1
fib_2:
	.loc 1 9
	mv a0, a1
	j fib_done
	.loc 1 16
	j fib_1
fib_1:
fib_done:
	lwu t0, 0(sp)
	addi sp, sp, 4
	lwu t1, 0(sp)
	addi sp, sp, 4
	lwu a1, 0(sp)
	addi sp, sp, 4
	lwu a2, 0(sp)
	addi sp, sp, 4
	lwu a3, 0(sp)
	addi sp, sp, 4
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
	.size fib, .-fib
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
	.loc 1 18 3
	.cfi_startproc
	addi sp, sp, -4
	sw ra, 0(sp)
	.cfi_offset 1, -4
	addi sp, sp, -4
	sw fp, 0(sp)
	.cfi_offset 8, -8
	mv fp, sp
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
main_0:
	.loc 1 19
	call uart_init@plt
	.loc 1 20
	li t0, 33 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)
	call uart_putc@plt
main_done:
	lwu t0, 0(sp)
	addi sp, sp, 4
	lwu t1, 0(sp)
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
