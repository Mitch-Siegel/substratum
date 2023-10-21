.global _start
_start:
	jal ra, userstart
	dummy:
#res 1
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
dumdum:
	addi sp, sp, -4
	sw ra, 0(sp)
	addi sp, sp, -4
	sw a0, 0(sp)
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
dumdum_0:
dumdum_done:
	lwu t0, 0(sp)
	addi sp, sp, 4
	lwu t1, 0(sp)
	addi sp, sp, 4
	lwu a0, 0(sp)
	addi sp, sp, 4
	lwu ra, 0(sp)
	addi sp, sp, 4
	addi sp, sp, 1
	jalr zero, 0(ra)
main:
	addi sp, sp, -4
	sw ra, 0(sp)
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
main_0:

	 # push 123
	li t0, 123 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)

	 # call dumdum
	jal ra, dumdum
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
