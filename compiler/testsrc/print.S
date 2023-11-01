	.Ltext0:
	.cfi_sections	.debug_frame
	.file 0 "testsrc/print.ca"
	.attribute unaligned_access, 0
	.file 1 "testsrc/print.ca"
	.globl print
	.type print, @function
print:
	.loc 1 2 5
	.cfi_startproc
	addi sp, sp, -4
	sw ra, 0(sp)
	.cfi_offset 1, -4
	addi sp, sp, -4
	sw fp, 0(sp)
	.cfi_offset 8, -8
	mv fp, sp
	li t0, 0x10000000
	lbu t1, 8(fp)
	sb t1, 0(t0)
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
	.size print, .-print
	.globl printStr
	.type printStr, @function
printStr:
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
	sw a4, 0(sp)
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
	lwu a1, 8(fp) # place printStr_str
	lbu a2, 12(fp) # place printStr_doNewline
printStr_0:
	.loc 1 13
	li t0, 0 # place literal
	# Write register variable printStr_numPrinted
	mv a3, t0
	.loc 1 14
	j printStr_2
printStr_2:
	.loc 1 14
	lbu a4, 0(a1)
	li t1, 0 # place literal
	beq a4, t1, printStr_1
	.loc 1 16
	lbu a4, 0(a1)
	addi sp, sp, -1
	sb a4, 0(sp)
	call print
	.loc 1 17
	li t0, 1 # place literal
	li t1, 1 # place literal
	mul a4, t0, t1
	add a4, a1, a4
	# Write register variable printStr_str
	mv a1, a4
	.loc 1 18
	li t1, 1 # place literal
	add a4, a3, t1
	# Write register variable printStr_numPrinted
	mv a3, a4
	j printStr_3
printStr_3:
	.loc 1 14
	j printStr_2
printStr_1:
	.loc 1 21
	li t1, 0 # place literal
	beq a2, t1, printStr_4
	j printStr_5
printStr_5:
	.loc 1 23
	li t0, 10 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)
	call print
	.loc 1 24
	li t1, 1 # place literal
	add a1, a3, t1
	# Write register variable printStr_numPrinted
	mv a3, a1
	j printStr_4
printStr_4:
	.loc 1 27
	mv a0, a3
	j printStr_done
printStr_done:
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
	lwu a4, 0(sp)
	addi sp, sp, 4
	lwu fp, 0(sp)
	addi sp, sp, 4
	.cfi_restore 8
	lwu ra, 0(sp)
	addi sp, sp, 4
	.cfi_restore 1
	addi sp, sp, 5
	.cfi_def_cfa_offset 0
	jalr zero, 0(ra)
	.cfi_endproc
	.size printStr, .-printStr
	.globl printNum
	.type printNum, @function
printNum:
	.loc 1 6 5
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
	sw a4, 0(sp)
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
	lwu a1, 8(fp) # place printNum_value
printNum_0:
	.loc 1 53
	li t0, 0 # place literal
	# Write register variable printNum_nChars
	mv a2, t0
	.loc 1 54
	# Write register variable printNum_vc
	mv a3, a1
	.loc 1 55
	j printNum_2
printNum_2:
	.loc 1 55
	li t1, 0 # place literal
	bleu a1, t1, printNum_1
	.loc 1 57
	li t0, 10 # place literal
	addi sp, sp, -4
	sw t0, 0(sp)
	addi sp, sp, -4
	sw a1, 0(sp)
	call mod
	# Write register variable .t0
	mv a3, a0
	.loc 1 58
	li t1, 48 # place literal
	add a4, a3, t1
	addi t0, fp, -10 # place printNum_chars
	slli t1, a2, 0
	add t0, t0, t1
	sb a4, 0(t0)
	.loc 1 59
	li t0, 10 # place literal
	addi sp, sp, -4
	sw t0, 0(sp)
	addi sp, sp, -4
	sw a1, 0(sp)
	call div
	# Write register variable .t2
	mv a4, a0
	# Write register variable printNum_value
	mv a1, a4
	.loc 1 60
	li t1, 1 # place literal
	add a4, a2, t1
	# Write register variable printNum_nChars
	mv a2, a4
	.loc 1 61
	addi t0, fp, -10 # place printNum_chars
	li t2, 0 # place literal
	slli t1, a2, 0
	add t0, t0, t1
	sb t2, 0(t0)
	j printNum_3
printNum_3:
	.loc 1 55
	j printNum_2
printNum_1:
	.loc 1 64
	j printNum_5
printNum_5:
	.loc 1 64
	li t1, 0 # place literal
	bleu a2, t1, printNum_4
	.loc 1 66
	li t1, 1 # place literal
	sub a1, a2, t1
	addi t1, fp, -10 # place printNum_chars
	slli t2, a1, 0
	add t1, t1, t2
	lbu a1, 0(t1)
	addi sp, sp, -1
	sb a1, 0(sp)
	call print
	.loc 1 67
	li t1, 1 # place literal
	sub a1, a2, t1
	# Write register variable printNum_nChars
	mv a2, a1
	j printNum_6
printNum_6:
	.loc 1 64
	j printNum_5
printNum_4:
	.loc 1 69
	li t0, 10 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)
	call print
printNum_done:
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
	lwu a4, 0(sp)
	addi sp, sp, 4
	addi sp, sp, 10
	lwu fp, 0(sp)
	addi sp, sp, 4
	.cfi_restore 8
	lwu ra, 0(sp)
	addi sp, sp, 4
	.cfi_restore 1
	addi sp, sp, 4
	.cfi_def_cfa_offset 0
	jalr zero, 0(ra)
	.cfi_endproc
	.size printNum, .-printNum
	.globl mod
	.type mod, @function
mod:
	.loc 1 30 5
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
	lwu a1, 8(fp) # place mod_a
	lwu a2, 12(fp) # place mod_b
mod_0:
	.loc 1 32
	j mod_2
mod_2:
	.loc 1 32
	bltu a1, a2, mod_1
	.loc 1 34
	sub a3, a1, a2
	# Write register variable mod_a
	mv a1, a3
	j mod_3
mod_3:
	.loc 1 32
	j mod_2
mod_1:
	.loc 1 36
	mv a0, a1
	j mod_done
mod_done:
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
	addi sp, sp, 8
	.cfi_def_cfa_offset 0
	jalr zero, 0(ra)
	.cfi_endproc
	.size mod, .-mod
	.globl div
	.type div, @function
div:
	.loc 1 39 5
	.cfi_startproc
	addi sp, sp, -4
	sw ra, 0(sp)
	.cfi_offset 1, -4
	addi sp, sp, -4
	sw fp, 0(sp)
	.cfi_offset 8, -8
	mv fp, sp
	addi sp, sp, -4
	sw a4, 0(sp)
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
	lwu a1, 8(fp) # place div_a
	lwu a2, 12(fp) # place div_b
div_0:
	.loc 1 41
	li t0, 0 # place literal
	# Write register variable div_quotient
	mv a3, t0
	.loc 1 42
	j div_2
div_2:
	.loc 1 42
	bltu a1, a2, div_1
	.loc 1 44
	sub a4, a1, a2
	# Write register variable div_a
	mv a1, a4
	.loc 1 45
	li t1, 1 # place literal
	add a4, a3, t1
	# Write register variable div_quotient
	mv a3, a4
	j div_3
div_3:
	.loc 1 42
	j div_2
div_1:
	.loc 1 47
	mv a0, a3
	j div_done
div_done:
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
	lwu a4, 0(sp)
	addi sp, sp, 4
	lwu fp, 0(sp)
	addi sp, sp, 4
	.cfi_restore 8
	lwu ra, 0(sp)
	addi sp, sp, 4
	.cfi_restore 1
	addi sp, sp, 8
	.cfi_def_cfa_offset 0
	jalr zero, 0(ra)
	.cfi_endproc
	.size div, .-div
