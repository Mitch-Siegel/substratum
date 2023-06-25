#include "CPU.asm"
entry main
print:
	push %r1
	push %r0
	print_0:

		movb %r0, (%bp+8)
		out 0, %r0
print_done:
	pop %r0
	pop %r1
	ret 1
mod:
	push %r3
	push %r2
	push %r1
	push %r0
	mov %r1, (%bp+8) ;place a
	mov %r2, (%bp+12) ;place divisor
	mod_0:
	;jmp basicblock 2
	jmp mod_2
mod_2:
	;do
	;cmp a divisor
	cmp %r1, %r2
	;jl basicblock 1
	jl mod_1
mod_3:
	;.t0 = a - divisor
	sub %r3, %r1, %r2
	;a = .t0
	mov %r1, %r3
	;jmp basicblock 2
	jmp mod_2
	;jmp basicblock 2
	jmp mod_2
	;end do
mod_1:
	;ret a
	mov %rr, %r1
	jmp mod_done
mod_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	ret 8
div:
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
	mov %r1, (%bp+8) ;place a
	mov %r2, (%bp+12) ;place b
	div_0:
	;quotient = 0
	movb %r3, $0
	;jmp basicblock 2
	jmp div_2
div_2:
	;do
	;cmp a b
	cmp %r1, %r2
	;jl basicblock 1
	jl div_1
div_3:
	;.t0 = a - b
	sub %r4, %r1, %r2
	;a = .t0
	mov %r1, %r4
	;.t1 = quotient + 1
	addi %r4, %r3, $1
	;quotient = .t1
	mov %r3, %r4
	;jmp basicblock 2
	jmp div_2
	;jmp basicblock 2
	jmp div_2
	;end do
div_1:
	;ret quotient
	mov %rr, %r3
	jmp div_done
div_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	ret 8
printNum:
	subi %sp, %sp, $16
	push %r6
	push %r5
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
	mov %r1, (%bp+8) ;place num
	mov %r2, (%bp+12) ;place newLine
	printNum_0:
	;declare outStr
	subi %r3, %bp, $16
	;digits = 0
	movb %r4, $0
	;cmp num 0
	cmpi %r1, $0
	;jne basicblock 2
	jne printNum_2
printNum_3:
	;.t0 = 48
	movb %r5, $48
	;(outStr + 0) = .t0
	movb (%r3+0), %r5
	;digits = 1
	movb %r4, $1
	;jmp basicblock 1
	jmp printNum_1
printNum_2:
printNum_4:
	;jmp basicblock 6
	jmp printNum_6
printNum_6:
	;do
	;cmp num 0
	cmpi %r1, $0
	;jle basicblock 5
	jle printNum_5
printNum_7:
	;.t2 CAST= 10
	movb %r5, $10
	;push .t2
	push %r5
	;push num
	push %r1
	;.t1 = call mod
	call mod
	mov %r5, %rr
	;remainder.00.01 = .t1
	mov %r5, %r5
	;.t3 = remainder.00.01 + 48
	addi %r6, %r5, $48
	;.t4 = .t3
	mov %r6, %r6
	;(outStr + digits * 2^0) = .t4
	movb (%r3+%r4,0), %r6
	;.t6 CAST= 10
	movb %r6, $10
	;push .t6
	push %r6
	;push num
	push %r1
	;.t5 = call div
	call div
	mov %r6, %rr
	;num = .t5
	mov %r1, %r6
	;.t7 = digits + 1
	addi %r6, %r4, $1
	;digits = .t7
	mov %r4, %r6
	;jmp basicblock 6
	jmp printNum_6
	;jmp basicblock 6
	jmp printNum_6
	;end do
printNum_5:
printNum_1:
	;i = 0
	movb %r1, $0
	;jmp basicblock 9
	jmp printNum_9
printNum_9:
	;do
	;cmp i digits
	cmp %r1, %r4
	;jg basicblock 8
	jg printNum_8
printNum_10:
	;.t9 = digits - i
	sub %r5, %r4, %r1
	;.t8 = (outStr + .t9 * 2^0)
	movb %r5, (%r3+%r5,0)
	;push .t8
	pushb %r5
	;call print
	call print
	;.t10 = i + 1
	addi %r5, %r1, $1
	;i = .t10
	mov %r1, %r5
	;jmp basicblock 9
	jmp printNum_9
	;jmp basicblock 9
	jmp printNum_9
	;end do
printNum_8:
	;cmp newLine 0
	cmpi %r2, $0
	;jle basicblock 11
	jle printNum_11
printNum_12:
	;.t11 = digits + 1
	addi %r1, %r4, $1
	;digits = .t11
	mov %r4, %r1
	;push 10
	movb %r0, $10
	pushb %r0
	;call print
	call print
	;jmp basicblock 11
	jmp printNum_11
printNum_11:
	;ret digits
	mov %rr, %r4
	jmp printNum_done
printNum_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	pop %r5
	pop %r6
	addi %sp, %sp, $16
	ret 8
mul:
	subi %sp, %sp, $4
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
	mov %r1, (%bp+8) ;place a
	mov %r2, (%bp+12) ;place b
	mul_0:
	;result = 0
	movb %r3, $0
	;jmp basicblock 2
	jmp mul_2
mul_2:
	;do
	;cmp b 0
	cmpi %r2, $0
	;jle basicblock 1
	jle mul_1
mul_3:
	;.t0 = result + a
	add %r4, %r3, %r1
	;result = .t0
	mov %r3, %r4
	;.t1 = b - 1
	subi %r4, %r2, $1
	;b = .t1
	mov %r2, %r4
	;jmp basicblock 2
	jmp mul_2
	;jmp basicblock 2
	jmp mul_2
	;end do
mul_1:
	;.t2 = nMultiplications + 1
	mov %r0, (%bp-4)
	addi %r4, %r0, $1
	;nMultiplications = .t2
	mov (%bp-4), %r4
	;ret result
	mov %rr, %r3
	jmp mul_done
mul_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	addi %sp, %sp, $4
	ret 8
test:
	subi %sp, %sp, $4
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
	test_0:
	;i = 1
	movb %r1, $1
	;jmp basicblock 2
	jmp test_2
test_2:
	;do
	;cmp i 10
	cmpi %r1, $10
	;jge basicblock 1
	jge test_1
test_3:
	;j.00 = 1
	movb %r2, $1
	;jmp basicblock 5
	jmp test_5
test_5:
	;do
	;cmp j.00 10
	cmpi %r2, $10
	;jge basicblock 4
	jge test_4
test_6:
	;.t2 CAST= j.00
	mov %r3, %r2
	;push .t2
	push %r3
	;.t3 CAST= i
	mov %r3, %r1
	;push .t3
	push %r3
	;.t1 = call mul
	call mul
	mov %r3, %rr
	;.t4 CAST= 0
	movb %r4, $0
	;push .t4
	push %r4
	;push .t1
	push %r3
	;.t0 = call printNum
	call printNum
	movb %r3, %rr
	;printedDigits.00.00 = .t0
	movb %r3, %r3
	;jmp basicblock 8
	jmp test_8
test_8:
	;do
	;cmp printedDigits.00.00 4
	cmpi %r3, $4
	;jge basicblock 7
	jge test_7
test_9:
	;push 32
	movb %r0, $32
	pushb %r0
	;call print
	call print
	;.t5 = printedDigits.00.00 + 1
	addi %r4, %r3, $1
	;printedDigits.00.00 = .t5
	movb %r3, %r4
	;jmp basicblock 8
	jmp test_8
	;jmp basicblock 8
	jmp test_8
	;end do
test_7:
	;.t6 = j.00 + 1
	addi %r4, %r2, $1
	;j.00 = .t6
	movb %r2, %r4
	;jmp basicblock 5
	jmp test_5
	;jmp basicblock 5
	jmp test_5
	;end do
test_4:
	;push 10
	movb %r0, $10
	pushb %r0
	;call print
	call print
	;.t7 = i + 1
	addi %r4, %r1, $1
	;i = .t7
	movb %r1, %r4
	;jmp basicblock 2
	jmp test_2
	;jmp basicblock 2
	jmp test_2
	;end do
test_1:
	;push 10
	movb %r0, $10
	pushb %r0
	;call print
	call print
	;.t9 CAST= 1
	movb %r1, $1
	;push .t9
	push %r1
	;push nMultiplications
	mov %r0, (%bp-4)
	push %r0
	;call printNum
	call printNum
test_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	addi %sp, %sp, $4
	ret
main:
	push %r0
	main_0:
	;call test
	call test
main_done:
	pop %r0
	ret
START:
	subi %sp, %sp, $4
	push %r0
	START_0:
	;nMultiplications = 0
	movb %r0, $0
	mov (%bp-4), %r0

	code:
	call main
	hlt
START_done:
	pop %r0
	addi %sp, %sp, $4
	ret
