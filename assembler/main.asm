	#include "CPU.asm"
	entry code
code:
	push $20
	call firstnfibs
	hlt
Program_done:
	ret 0
firstnfibs:
	sub %sp, $6
	push %r1
	push %r2
	push %r3
	push %r4
	mov %r0, $1000
	mov %r1, $0
	mov (%r0), %r1
	mov %r1, $1
	mov %r2, %r0
	add %r2, $2
	mov (%r2), %r1
	mov %r1, $2
firstnfibs_1:
	cmp %r1, 4(%bp)
	jg firstnfibs_2
	mov %r2, %r1
	sub %r2, $2
	mov %r3, %r2
	mul %r3, $2
	add %r3, %r0
	mov %r2, (%r3)
	mov %r3, %r1
	sub %r3, $1
	mov %r4, %r3
	mul %r4, $2
	add %r4, %r0
	mov %r3, (%r4)
	add %r2, %r3
	out %r2
	mov %r3, %r2
	mov %r2, %r1
	mul %r2, $2
	add %r2, %r0
	mov (%r2), %r3
	add %r1, $1
	jmp firstnfibs_1
firstnfibs_2:
firstnfibs_done:
	pop %r4
	pop %r3
	pop %r2
	pop %r1
	add %sp, $6
	ret 2
