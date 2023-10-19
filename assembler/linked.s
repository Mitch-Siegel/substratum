.global _start#align 2048
print:
	pushw a1
	pushw a0
	pushw t1
	pushw t0
	lw %r10, (%bp+8) ;place print_n
print_0:

	;.t0 = print_n + 1
	li t1, 1 ; place literal
	add a1, a0, t1

	;print_n = .t0
	;Write register variable print_n
	mv a0, a1
print_done:
	popw t0
	popw t1
	popw a0
	popw a1
	ret 4
#align 2048
main:
	subi %sp, %sp, $32
	pushw a0
	pushw t1
	pushw t0
main_0:

	;.t0 = &main_s
	subi a0, %fp, 32

	;(.t0 + 4) = 0
	li t1, 0 ; place literal
	sw t1, 4(a0)
	;main_i = 2
	li t0, 2 ; place literal
	;Write register variable main_i
	mv a0, t0

	;(main_testArray + main_i*2^1) = 4
	subi t0, %fp, 24 ; place main_testArray
	li t2, 4 ; place literal
	slli t1, a0, 1
	add t0, t0, t1
	sh t2, 0(t0)main_done:
	popw t0
	popw t1
	popw a0
	addi %sp, %sp, $32
	ret
