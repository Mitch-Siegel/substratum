	.Ltext0:
	.cfi_sections	.debug_frame
	.file 0 "arrays.ca"
	.attribute unaligned_access, 0
	.file 1 "arrays.ca"
	.globl uart_putc
	.type uart_putc, @function
uart_putc:
	.loc 1 1 4
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
	lbu a1, 8(fp) # place uart_putc_c
uart_putc_0:
	.loc 1 3
	li t0, 268435456 # place literal
	# Write register variable uart_putc_uart0
	mv a2, t0
	.loc 1 4
	sb a1, 0(a2)
uart_putc_done:
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
	addi sp, sp, 1
	.cfi_def_cfa_offset 0
	jalr zero, 0(ra)
	.cfi_endproc
	.size uart_putc, .-uart_putc
	.globl exit
	.type exit, @function
exit:
	.loc 1 12 15
	.cfi_startproc
	addi sp, sp, -4
	sw ra, 0(sp)
	.cfi_offset 1, -4
	addi sp, sp, -4
	sw fp, 0(sp)
	.cfi_offset 8, -8
	mv fp, sp
	li a0, 0x100000
	li a1, 0x5555
	sw a1,0(a0)
	lwu fp, 0(sp)
	addi sp, sp, 4
	.cfi_restore 8
	lwu ra, 0(sp)
	addi sp, sp, 4
	.cfi_restore 1
	addi sp, sp, 0
	.cfi_def_cfa_offset 0
	jalr zero, 0(ra)
	.cfi_endproc
	.size exit, .-exit
	.globl testu8Array
	.type testu8Array, @function
testu8Array:
	.loc 1 19 22
	.cfi_startproc
	addi sp, sp, -4
	sw ra, 0(sp)
	.cfi_offset 1, -4
	addi sp, sp, -4
	sw fp, 0(sp)
	.cfi_offset 8, -8
	mv fp, sp
	addi sp, sp, -10
	.cfi_def_cfa_offset 18
	addi sp, sp, -4
	sw a2, 0(sp)
	addi sp, sp, -4
	sw a1, 0(sp)
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
testu8Array_0:
	.loc 1 23
	li t0, 0 # place literal
	# Write register variable testu8Array_i
	mv a1, t0
	j testu8Array_2
testu8Array_2:
	.loc 1 23
	.loc 1 25
	li t1, 10 # place literal
	bgeu a1, t1, testu8Array_1
	.loc 1 26
	addi t0, fp, -10 # place testu8Array_array
	li t2, 255 # place literal
	slli t1, a1, 0
	add t0, t0, t1
	sb t2, 0(t0)
	.loc 1 27
	addi t0, fp, -10 # place testu8Array_array
	slli t1, a1, 0
	add t0, t0, t1
	sb a1, 0(t0)
	.loc 1 29
	addi t1, fp, -10 # place testu8Array_array
	slli t2, a1, 0
	add t1, t1, t2
	lbu a2, 0(t1)
	li t0, 48 # place literal
	add a2, t0, a2
	addi sp, sp, -1
	sb a2, 0(sp)
	call uart_putc
	.loc 1 30
	li t1, 1 # place literal
	add a2, a1, t1
	# Write register variable testu8Array_i
	mv a1, a2
	j testu8Array_3
testu8Array_3:
	.loc 1 23
	j testu8Array_2
testu8Array_1:
	.loc 1 33
	li t0, 10 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)
	call uart_putc
testu8Array_done:
	lwu t0, 0(sp)
	addi sp, sp, 4
	lwu t1, 0(sp)
	addi sp, sp, 4
	lwu a1, 0(sp)
	addi sp, sp, 4
	lwu a2, 0(sp)
	addi sp, sp, 4
	addi sp, sp, 10
	lwu fp, 0(sp)
	addi sp, sp, 4
	.cfi_restore 8
	lwu ra, 0(sp)
	addi sp, sp, 4
	.cfi_restore 1
	.cfi_def_cfa_offset 0
	jalr zero, 0(ra)
	.cfi_endproc
	.size testu8Array, .-testu8Array
	.globl testu16Array
	.type testu16Array, @function
testu16Array:
	.loc 1 35 12
	.cfi_startproc
	addi sp, sp, -4
	sw ra, 0(sp)
	.cfi_offset 1, -4
	addi sp, sp, -4
	sw fp, 0(sp)
	.cfi_offset 8, -8
	mv fp, sp
	addi sp, sp, -20
	.cfi_def_cfa_offset 28
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
testu16Array_0:
	.loc 1 39
	li t0, 0 # place literal
	# Write register variable testu16Array_i
	mv a1, t0
	j testu16Array_2
testu16Array_2:
	.loc 1 39
	.loc 1 40
	li t1, 10 # place literal
	bgeu a1, t1, testu16Array_1
	addi t0, fp, -20 # place testu16Array_array
	li t2, 65535 # place literal
	slli t1, a1, 1
	add t0, t0, t1
	sh t2, 0(t0)
	.loc 1 42
	li t0, 256 # place literal
	add a2, t0, a1
	addi t0, fp, -20 # place testu16Array_array
	slli t1, a1, 1
	add t0, t0, t1
	sh a2, 0(t0)
	.loc 1 43
	addi t1, fp, -20 # place testu16Array_array
	slli t2, a1, 1
	add t1, t1, t2
	lhu a2, 0(t1)
	.loc 1 45
	li t0, 48 # place literal
	add a3, t0, a2
	addi sp, sp, -1
	sb a3, 0(sp)
	call uart_putc
	.loc 1 48
	li t1, 1 # place literal
	add a3, a1, t1
	# Write register variable testu16Array_i
	mv a1, a3
	j testu16Array_3
testu16Array_3:
	.loc 1 39
	j testu16Array_2
testu16Array_1:
	.loc 1 50
	li t0, 10 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)
	call uart_putc
testu16Array_done:
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
	addi sp, sp, 20
	lwu fp, 0(sp)
	addi sp, sp, 4
	.cfi_restore 8
	lwu ra, 0(sp)
	addi sp, sp, 4
	.cfi_restore 1
	.cfi_def_cfa_offset 0
	jalr zero, 0(ra)
	.cfi_endproc
	.size testu16Array, .-testu16Array
	.globl testu32Array
	.type testu32Array, @function
testu32Array:
	.loc 1 51 9
	.cfi_startproc
	addi sp, sp, -4
	sw ra, 0(sp)
	.cfi_offset 1, -4
	addi sp, sp, -4
	sw fp, 0(sp)
	.cfi_offset 8, -8
	mv fp, sp
	addi sp, sp, -40
	.cfi_def_cfa_offset 48
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
testu32Array_0:
	.loc 1 54
	li t0, 0 # place literal
	# Write register variable testu32Array_i
	mv a1, t0
	j testu32Array_2
testu32Array_2:
	.loc 1 54
	.loc 1 55
	li t1, 10 # place literal
	bgeu a1, t1, testu32Array_1
	.loc 1 56
	addi t0, fp, -40 # place testu32Array_array
	li t2, 4294967295 # place literal
	slli t1, a1, 2
	add t0, t0, t1
	sw t2, 0(t0)
	.loc 1 57
	li t0, 65536 # place literal
	add a2, t0, a1
	addi t0, fp, -40 # place testu32Array_array
	slli t1, a1, 2
	add t0, t0, t1
	sw a2, 0(t0)
	.loc 1 58
	addi t1, fp, -40 # place testu32Array_array
	slli t2, a1, 2
	add t1, t1, t2
	lwu a2, 0(t1)
	.loc 1 63
	li t0, 48 # place literal
	add a3, t0, a2
	addi sp, sp, -1
	sb a3, 0(sp)
	call uart_putc
	.loc 1 65
	li t1, 1 # place literal
	add a3, a1, t1
	# Write register variable testu32Array_i
	mv a1, a3
	j testu32Array_3
testu32Array_3:
	.loc 1 54
	j testu32Array_2
testu32Array_1:
	.loc 1 66
	li t0, 10 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)
	call uart_putc
testu32Array_done:
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
	addi sp, sp, 40
	lwu fp, 0(sp)
	addi sp, sp, 4
	.cfi_restore 8
	lwu ra, 0(sp)
	addi sp, sp, 4
	.cfi_restore 1
	.cfi_def_cfa_offset 0
	jalr zero, 0(ra)
	.cfi_endproc
	.size testu32Array, .-testu32Array
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
	.loc 1 67 13
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
	.loc 1 68
	call testu8Array
	.loc 1 69
	call testu16Array
	call testu32Array
	call exit
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
