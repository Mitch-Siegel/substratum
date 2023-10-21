.global _start
_start:
	jal ra, userstart
	dummy:
#res 1
main_123:
#d8 0x31 
#d8 0x32 
#d8 0x33 
#d8 0x00 
print:
	addi sp, sp, -4
	sw ra, 0(sp)
	li t0, 0x10000000
	lbu t1, 4(sp)
	sb t1, 0(t0)
	lwu ra, 0(sp)
	addi sp, sp, 4
	jalr zero, 0(ra)
	addi sp, sp, 1
printStr:
	addi sp, sp, -4
	sw ra, 0(sp)
	addi sp, sp, -4
	sw a2, 0(sp)
	addi sp, sp, -4
	sw a1, 0(sp)
	addi sp, sp, -4
	sw a0, 0(sp)
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
	lwu a0, 8(fp) # place printStr_str
printStr_0:

	 # printStr_numPrinted = 0
	li t0, 0 # place literal
	# Write register variable printStr_numPrinted
	mv a1, t0

	 # jmp basicblock 2
	j printStr_2
printStr_2:

	 # do

	 # .t0 = *printStr_str
	li t2, 0 # place literal
	add a0, a0, t2
	lbu a2, 0(a0)

	 # beq .t0, 0, basicblock 1
	li t1, 0 # place literal
	beq a2, t1, printStr_1

	 # .t1 = *printStr_str
	li t2, 0 # place literal
	add a0, a0, t2
	lbu a2, 0(a0)

	 # push .t1
	addi sp, sp, -1
	sb a2, 0(sp)

	 # call print
	jal ra, print

	 # .t3 = 1 * 1
	li t0, 1 # place literal
	li t1, 1 # place literal
	mul a2, t0, t1

	 # .t2 = printStr_str + .t3
	add a2, a0, a2

	 # printStr_str = .t2
	# Write register variable printStr_str
	mv a0, a2

	 # .t4 = printStr_numPrinted + 1
	li t1, 1 # place literal
	add a2, a1, t1

	 # printStr_numPrinted = .t4
	# Write register variable printStr_numPrinted
	mv a1, a2

	 # jmp basicblock 3
	j printStr_3
printStr_3:

	 # jmp basicblock 2
	j printStr_2

	 # end do
printStr_1:

	 # ret printStr_numPrinted
	mv a0, a1
	j printStr_done
printStr_done:
	lwu t0, 0(sp)
	addi sp, sp, 4
	lwu t1, 0(sp)
	addi sp, sp, 4
	lwu a0, 0(sp)
	addi sp, sp, 4
	lwu a1, 0(sp)
	addi sp, sp, 4
	lwu a2, 0(sp)
	addi sp, sp, 4
	lwu ra, 0(sp)
	addi sp, sp, 4
	addi sp, sp, 4
	jalr zero, 0(ra)
main:
	addi sp, sp, -4
	sw ra, 0(sp)
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
main_0:

	 # push main_123
	la t0, main_123 # place main_123
	addi sp, sp, -4
	sw t0, 0(sp)

	 # call printStr
	jal ra, printStr

	 # push 10
	li t0, 10 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)

	 # call print
	jal ra, print
main_done:
	lwu t0, 0(sp)
	addi sp, sp, 4
	lwu t1, 0(sp)
	addi sp, sp, 4
	lwu ra, 0(sp)
	addi sp, sp, 4
	jalr zero, 0(ra)
userstart:
	li t0, 0x10000000
	andi t1, t1, 0
	addi t1, t1, 'h'
	sw t1, 0(t0)
	andi t1, t1, 0
	addi t1, t1, 'e'
	sw t1, 0(t0)
	andi t1, t1, 0
	addi t1, t1, 'l'
	sw t1, 0(t0)
	sw t1, 0(t0)
	andi t1, t1, 0
	addi t1, t1, 'o'
	sw t1, 0(t0)
	andi t1, t1, 0
	addi t1, t1, '!'
	sw t1, 0(t0)
	andi t1, t1, 0
	addi t1, t1, 10
	sw t1, 0(t0)
	li sp, 0x81000000
	jal ra, main
pgm_done:
	wfi
	beq t1, t1, pgm_done
