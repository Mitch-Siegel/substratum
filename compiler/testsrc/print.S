.rawasm:
print:
	addi sp, sp, -4
	sw ra, 0(sp)
	addi sp, sp, -4
	sw fp, 0(sp)
	mv fp, sp
	li t0, 0x10000000
	lbu t1, 8(fp)
	sb t1, 0(t0)
	lwu fp, 0(sp)
	addi sp, sp, 4
	lwu ra, 0(sp)
	addi sp, sp, 4
	addi sp, sp, 1
	jalr zero, 0(ra)
