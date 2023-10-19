.global _start
_start:
	jal ra, userstart
	dummy:
#res 1
print:
	addi sp, sp, -4
	sw a1, 0(sp)
	addi sp, sp, -4
	sw a0, 0(sp)
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
	lw a0, 8(fp) # place print_n
print_0:

	 # .t0 = print_n + 1
	li t1, 1 # place literal
	add a1, a0, t1

	 # print_n = .t0
	# Write register variable print_n
	mv a0, a1
print_done:
	lw t0, 0(sp)
	addi sp, sp, 4
	lw t1, 0(sp)
	addi sp, sp, 4
	lw a0, 0(sp)
	addi sp, sp, 4
	lw a1, 0(sp)
	addi sp, sp, 4
	addi sp, sp, 4
	ret
main:
	addi sp, sp, -32
	addi sp, sp, -4
	sw a0, 0(sp)
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
main_0:

	 # .t0 = &main_s
	addi a0, fp, -32

	 # (.t0 + 4) = 0
	li t1, 0 # place literal
	sw t1, 4(a0)
	 # main_i = 2
	li t0, 2 # place literal
	# Write register variable main_i
	mv a0, t0

	 # (main_testArray + main_i*2^1) = 4
	addi t0, fp, -24 # place main_testArray
	li t2, 4 # place literal
	slli t1, a0, 1
	add t0, t0, t1
	sh t2, 0(t0)
main_done:
	lw t0, 0(sp)
	addi sp, sp, 4
	lw t1, 0(sp)
	addi sp, sp, 4
	lw a0, 0(sp)
	addi sp, sp, 4
	addi sp, sp, 32
	ret
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
pgm_done:
	wfi
	beq t1, t1, pgm_done
