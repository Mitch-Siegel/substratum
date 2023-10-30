.rawasm:
main:
	addi sp, sp, -4
	sw ra, 0(sp)
	addi sp, sp, -4
	sw fp, 0(sp)
	mv fp, sp
	addi sp, sp, -4
	sw t1, 0(sp)
	addi sp, sp, -4
	sw t0, 0(sp)
main_0:

	#  0:push 104                             	[L u8(u8)]   -      -      -   
	li t0, 104 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)

	#  1:call print                           	   -      -      -      -   
	jal ra, print

	#  2:push 101                             	[L u8(u8)]   -      -      -   
	li t0, 101 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)

	#  3:call print                           	   -      -      -      -   
	jal ra, print

	#  4:push 108                             	[L u8(u8)]   -      -      -   
	li t0, 108 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)

	#  5:call print                           	   -      -      -      -   
	jal ra, print

	#  6:push 108                             	[L u8(u8)]   -      -      -   
	li t0, 108 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)

	#  7:call print                           	   -      -      -      -   
	jal ra, print

	#  8:push 111                             	[L u8(u8)]   -      -      -   
	li t0, 111 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)

	#  9:call print                           	   -      -      -      -   
	jal ra, print

	#  a:push 10                              	[L u8(u8)]   -      -      -   
	li t0, 10 # place literal
	addi sp, sp, -1
	sb t0, 0(sp)

	#  b:call print                           	   -      -      -      -   
	jal ra, print
main_done:
	lwu t0, 0(sp)
	addi sp, sp, 4
	lwu t1, 0(sp)
	addi sp, sp, 4
	lwu fp, 0(sp)
	addi sp, sp, 4
	lwu ra, 0(sp)
	addi sp, sp, 4
	jalr zero, 0(ra)
