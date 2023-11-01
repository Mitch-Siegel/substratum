	.Ltext0:
	.cfi_sections	.debug_frame
	.file 0 "testsrc/main.ca"
	.attribute unaligned_access, 0
	.file 1 "testsrc/main.ca"
.userstart:
	.loc 1 3
	li zero, 0 # place literal
	# Write (global) variable nRecursions
	li t0, nRecursions
	sw (t0), zero
	.section	.data
	.globl nRecursions
	.align 2
	.type	nRecursions, @object
	.size 	nRecursions, 4
nRecursions:
	.zero 4	.text
	.globl fibRec
	.type fibRec, @function
fibRec:
	.loc 1 4 5
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
	lbu a1, 8(fp) # place fibRec_n
fibRec_0:
	.loc 1 6
	la t0, nRecursions # place nRecursions
	wu t0, 0(t0) # place nRecursions
	li t1, 1 # place literal
	add a2, t0, t1
	# Write (global) variable nRecursions
	li t0, nRecursions
	sw (t0), a2
	.loc 1 7
	li t1, 2 # place literal
	bgeu a1, t1, fibRec_2
	j fibRec_3
fibRec_3:
	.loc 1 9
	mv a0, a1
	j fibRec_done
	j fibRec_1
fibRec_2:
	.loc 1 13
	li t1, 1 # place literal
	sub a2, a1, t1
	addi sp, sp, -1
	sb a2, 0(sp)
	call fibRec
	# Write register variable .t3
	mv a2, a0
	li t1, 2 # place literal
	sub a1, a1, t1
	addi sp, sp, -1
	sb a1, 0(sp)
	call fibRec
	# Write register variable .t5
	mv a1, a0
	add a1, a2, a1
	mv a0, a1
	j fibRec_done
	j fibRec_1
fibRec_1:
fibRec_done:
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
	.size fibRec, .-fibRec
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
	.loc 1 18 5
	.cfi_startproc
	addi sp, sp, -4
	sw ra, 0(sp)
	.cfi_offset 1, -4
	addi sp, sp, -4
	sw fp, 0(sp)
	.cfi_offset 8, -8
	mv fp, sp
	addi sp, sp, -4
	sw a1, 0(sp)
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
main_0:
	.loc 1 20
	li t0, 1 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)
	la t0, main_hello # place main_hello
	addi sp, sp, -4
	sw t0, 0(sp)
	call printStr@plt
	.loc 1 22
	li t0, 123456 # place literal
	li t1, 654321 # place literal
	add a1, t0, t1
	addi sp, sp, -4
	sw a1, 0(sp)
	call printNum@plt
main_done:
	lwu t0, 0(sp)
	addi sp, sp, 4
	lwu t1, 0(sp)
	addi sp, sp, 4
	lwu a1, 0(sp)
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
	.section	.sdata
	.globl main_hello
	.align 3
	.type	main_hello, @object
	.size 	main_hello, 6
main_hello:
