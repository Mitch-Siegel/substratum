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
    mov %r2, buf
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


#align 32
START:
    mov %r0, $MEMMAP_IDT
    mov %r1, READKEY
    mov (%r0), %r1
    mov %r0, $MEMMAP_SCREEN
    mov %r1, buflen
    mov %r2, buf
printBuf:
    mov %r3, (%r1)
    mov %r4, $0
printBufLoop:
    cmp %r3, %r4
    jge printBuf
    movb %r5, (%r2+%r4,$0)
    movb (%r0+%r4,$0), %r5
    addi %r4, %r4, $1
    jmp printBufLoop
    


poll:
    movb %r3, %r2
    movb %r2, (%r0)
    cmp %r2, %r3
    je poll
    cmpi %r2, $0
    je poll
    cmpi %r2, $127
    je backspace
    movb (%r1+%r4,$0), %r2
    addi %r4, %r4, $1
    andi %r4, %r4, $255
    jmp poll
backspace:
    movb (%r1+%r4,$0), %r5
    cmpi %r4, $0
    je poll
    subi %r4, %r4, $1
    jmp poll


