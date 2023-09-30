#include "CPU.asm"
entry START

#const MEMMAP_IDT = 0x100
#const MEMMAP_SCREEN = 0x500
#const MEMMAP_KEYBOARD = 0xC80

; leave room for memory mapped region (IDT, screen, keyboard)
#addr 4096
START:
    mov %r0, $MEMMAP_KEYBOARD
    mov %r1, $MEMMAP_SCREEN
    mov %r4, 0
    mov %r5, 32
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


