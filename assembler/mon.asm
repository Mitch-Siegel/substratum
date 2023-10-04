#include "CPU.asm"
entry START

#const MEMMAP_IDT = 0x100
#const MEMMAP_SCREEN = 0x500
#const MEMMAP_KEYBOARD = 0xC80

; leave room for memory mapped region (IDT, screen, keyboard)
#addr 4096
buflen:
#res 1
buf:
#res 256
#align 32
READKEY:
    push %r0
    push %r1
    push %r2
    push %r3
    mov %r0, $MEMMAP_KEYBOARD
    mov %r1, buflen
    mov %r1, (%r1)
    mov %r2, $MEMMAP_SCREEN
READKEY_POLL:
    movb %r3, (%r0)
    cmpi %r3, $127
    je READKEY_BACKSPACE
    cmpi %r3, $10
    je READKEY_ENTER
    movb (%r2+%r1,$0), %r3
    addi %r1, %r1, $1
    jmp READKEY_WRITEBUFLEN
READKEY_BACKSPACE:
    subi %r1, %r1, $1
    ; fall through to write buflen
READKEY_WRITEBUFLEN:
    mov %r2, buflen
    mov (%r2), %r1
READKEY_ENTER:
READKEY_DONE:
    pop %r3
    pop %r2
    pop %r1
    pop %r0
    reti


scrollScreen:
	push %r7
	push %r6
	push %r5
	push %r4
	push %r3
	push %r2
	mov %r2, (%bp+8) ;place scrollScreen_screen
scrollScreen_0:

	;scrollScreen_y = 1
	movb %r0, $1 ; place literal
	;Write register variable scrollScreen_y
	movb %r3, %r0

	;jmp basicblock 2
	jmp scrollScreen_2
scrollScreen_2:

	;do

	;cmp scrollScreen_y 24
	movb %rr, $24 ; place literal
	cmp %r3, %rr

	;jge basicblock 1
	jge scrollScreen_1

	;scrollScreen_00_x = 0
	movb %r0, $0 ; place literal
	;Write register variable scrollScreen_00_x
	movb %r4, %r0

	;.t1 = scrollScreen_y - 1
	movb %rr, $1 ; place literal
	sub %r5, %r3, %rr

	;.t0 = (scrollScreen_screen + .t1 * 2^2)
	mov %r5, (%r2+%r5,2)

	;scrollScreen_00_upperLine = .t0

	;.t2 = (scrollScreen_screen + scrollScreen_y * 2^2)
	mov %r6, (%r2+%r3,2)

	;scrollScreen_00_lowerLine = .t2

	;jmp basicblock 5
	jmp scrollScreen_5
scrollScreen_5:

	;do

	;cmp scrollScreen_00_x 80
	movb %rr, $80 ; place literal
	cmp %r4, %rr

	;jge basicblock 4
	jge scrollScreen_4

	;.t3 = (scrollScreen_00_lowerLine + scrollScreen_00_x * 2^0)
	movb %r7, (%r6+%r4,0)

	;(scrollScreen_00_upperLine + scrollScreen_00_x * 2^0) = .t3
	movb (%r5+%r4,0), %r7

	;.t4 = scrollScreen_00_x + 1
	movb %rr, $1 ; place literal
	add %r7, %r4, %rr

	;scrollScreen_00_x = .t4
	;Write register variable scrollScreen_00_x
	movb %r4, %r7

	;jmp basicblock 6
	jmp scrollScreen_6
scrollScreen_6:

	;jmp basicblock 5
	jmp scrollScreen_5

	;end do
scrollScreen_4:

	;.t5 = scrollScreen_y + 1
	movb %rr, $1 ; place literal
	add %r7, %r3, %rr

	;scrollScreen_y = .t5
	;Write register variable scrollScreen_y
	movb %r3, %r7

	;jmp basicblock 3
	jmp scrollScreen_3
scrollScreen_3:

	;jmp basicblock 2
	jmp scrollScreen_2

	;end do
scrollScreen_1:
scrollScreen_done:
	pop %r2
	pop %r3
	pop %r4
	pop %r5
	pop %r6
	pop %r7
	ret 4

copyLineToScreen:
    push %r0
    push %r1
    push %r2
    push %r3
    push %r4
    movb %r0, $0
    mov %r1, buflen
    mov %r2, buf
    movb %r1, (%r1)
    mov %r4, $MEMMAP_SCREEN
copyLineToScreen_loop:
    movb %r3, (%r2+%r1,0)
	cmpi %r3, $10
	je copyLineToScreen_done
	movb (%r4+%r1,0), %r3 
	addi %r0, %r0, $1
	jmp copyLineToScreen_loop

copyLineToScreen_done:
    pop %r4
    pop %r3
    pop %r2
    pop %r1
    pop %r0
	ret

#align 32
START:
    mov %r0, $MEMMAP_IDT
    mov %r1, READKEY
    mov (%r0), %r1
    mov %r0, $MEMMAP_SCREEN
    mov %r1, buflen
    mov %r2, buf
pollLine:
    mov %r3, (%r1)
    movb %r5, (%r2+%r3,$0)
	cmpi %r5, $10
	jne pollLine
	call copyLineToScreen
	jmp pollLine

