; customasm ruledef for the cpu
#bits 8

#ruledef reg{
    r0  => 0b0000
    r1  => 0b0001
    r2  => 0b0010
    r3  => 0b0011
    r4  => 0b0100
    r5  => 0b0101
    r6  => 0b0110
    r7  => 0b0111
    r8  => 0b1000
    r9  => 0b1001
    r10 => 0b1010
    r11 => 0b1011
    r12 => 0b1100
    rr  => 0b1101 ;return register
    sp  => 0b1110
    bp  => 0b1111
    ; 12 GP registers - plenty to give up a few for MMU and other stuff handling!
}


; 2 instruction lengths - full word (32bit) and halfword (16bit)
; lsb of opcode indicates length (0 for hword 1 for word)
#ruledef{

    nop             => 0x01 @ 0x000000
    
    jmp {dst:i32}  => 
    {
        reladdr = ((dst - $) - 4) >> 2
        assert(reladdr < 0x00ffffff)
        0x11 @ reladdr`24
    }

    je  {dst:i32}  => 
    {
        reladdr = ((dst - $) - 4) >> 2
        assert(reladdr < 0x00ffffff)
        0x13 @ reladdr`24
    }

    jz  {dst:i32}  => asm {je dst}

    jne {dst:i32}  => 
    {
        reladdr = ((dst - $) - 4) >> 2
        assert(reladdr < 0x00ffffff)
        0x15 @ reladdr`24
    }

    jnz {dst:i32}  => asm {jne dst}

    jg  {dst:i32}  => 
    {
        reladdr = ((dst - $) - 4) >> 2
        assert(reladdr < 0x00ffffff)
        0x17 @ reladdr`24
    }

    jl  {dst:i32}  => 
    {
        reladdr = ((dst - $) - 4) >> 2
        assert(reladdr < 0x00ffffff)
        0x19 @ reladdr`24
    }

    jge {dst:i32}  => 
    {
        reladdr = ((dst - $) - 4) >> 2
        assert(reladdr < 0x00ffffff)
        0x1b @ reladdr`24
    }

    jle {dst:i32}  => 
    {
        reladdr = ((dst - $) - 4) >> 2
        assert(reladdr < 0x00ffffff)
        0x1d @ reladdr`24
    }



    ; unsigned arithmetic
    add %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x41 @ rd @ rs1 @ rs2 @ 0x000
    sub %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x43 @ rd @ rs1 @ rs2 @ 0x000
    mul %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x45 @ rd @ rs1 @ rs2 @ 0x000
    div %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x47 @ rd @ rs1 @ rs2 @ 0x000
    shr %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x49 @ rd @ rs1 @ rs2 @ 0x000
    shl %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x4b @ rd @ rs1 @ rs2 @ 0x000
    and %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x4d @ rd @ rs1 @ rs2 @ 0x000
    or  %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x4f @ rd @ rs1 @ rs2 @ 0x000
    xor %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x51 @ rd @ rs1 @ rs2 @ 0x000
    cmp %{rs1: reg}, %{rs2: reg}                    => 0x53 @ 0x0 @ rs1 @ rs2 @ 0x000
    inc %{rd: reg}                                  => 0x54 @ rd @ 0x0 @ 0x0000
    dec %{rd: reg}                                  => 0x56 @ rd @ 0x0 @ 0x0000
    not %{rd: reg}, %{rs1: reg}                     => 0x58 @ rd @ rs1 @ 0x0000


        ; immediate
    addi %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x61 @ rd @ rs1 @ imm
    subi %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x63 @ rd @ rs1 @ imm
    muli %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x65 @ rd @ rs1 @ imm
    divi %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x67 @ rd @ rs1 @ imm
    shri %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x69 @ rd @ rs1 @ imm
    shli %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x6b @ rd @ rs1 @ imm
    andi %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x6d @ rd @ rs1 @ imm
    ori  %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x6f @ rd @ rs1 @ imm
    xori %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x71 @ rd @ rs1 @ imm
    cmpi %{rs1: reg}, ${imm: i16}                   => 0x73 @ 0x0 @ rs1 @ imm

    ; data movement
    ; Limitations of this scaling setup:
    ; array size at 2^12 (4096) elements since that's the maximum offset
    ; element size at 2^(2^4 - 1) (32768 bytes) since that's the maximum scale
    ; this is very silly and the way I now think it should be done is:
    ; shuffle around the operands so sclpow is just a scale multiplier using an unsigned 9 bit
    ; encode a few different instructions that bit shift the multiplier for larger sized array elements

    ; data movement (byte)
    movb %{rd: reg}, %{rs: reg}                                  => 0xa0 @ rs @ rd @ 0x0000

    movb %{rd: reg}, (%{rbase: reg})                             => 0xa2 @ rd @ rbase @ 0x0000
    movb (%{rbase: reg}), %{rs: reg}                             => 0xa4 @ rbase @ rs @ 0x0000
    
    movb %{rd: reg}, (%{rbase: reg}+{offset: i16})              => 0xa5 @ rd @ rbase @ offset
    movb (%{rbase: reg}+{offset: i16}), %{rs: reg}               => 0xa7 @ rs @ rbase @ offset

    movb %{rd: reg}, (%{rbase: reg}+%{roffset: reg},{sclpow: i5}) => 0xa9 @ rd @ rbase @ 0x0 @ roffset @ 0b000 @ sclpow
    movb (%{rbase: reg}+%{roffset: reg},{sclpow:i5}), %{rs: reg}  => 0xab @ rs @ rbase @ 0x0 @ roffset @ 0b000 @ sclpow

    movb %{rd: reg}, ${imm: i8}                                  => 0xaf @ rd @ 0x000 @ imm

    ; data movement (half word)
    movh %{rd: reg}, %{rs: reg}                                  => 0xb0 @ rs @ rd @ 0x0000

    movh %{rd: reg}, (%{rbase: reg})                             => 0xb2 @ rd @ rbase @ 0x0000
    movh (%{rbase: reg}), %{rs: reg}                             => 0xb4 @ rbase @ rs @ 0x0000
    
    movh %{rd: reg}, (%{rbase: reg}+%{offset: i16})              => 0xb5 @ rd @ rbase @ offset
    movh (%{rbase: reg}+{offset: i16}), %{rs: reg}              => 0xb7 @ rs @ rbase @ offset

    movh %{rd: reg}, (%{rbase: reg}+%{roffset: reg},{sclpow: i5})=> 0xb9 @ rd @ rbase @ 0x0 @ roffset @ 0b000 @ sclpow
    movh (%{rbase: reg}+%{roffset: reg},{sclpow:i5}), %{rs: reg} => 0xbb @ rs @ rbase @ 0x0 @ roffset @ 0b000 @ sclpow

    movh %{rd: reg}, ${imm: i16}                                 => 0xbf @ rd @ 0x0 @ imm
    mov %{rd: reg}, ${imm: i16}                                 => 0xbf @ rd @ 0x0 @ imm


    ; data movement (full word)
    mov %{rd: reg}, %{rs: reg}                                   => 0xc0 @ rs @ rd @ 0x0000

    mov %{rd: reg}, (%{rbase: reg})                              => 0xc2 @ rd @ rbase @ 0x0000
    mov (%{rbase: reg}), %{rs: reg}                              => 0xc4 @ rbase @ rs @ 0x0000
    
    mov %{rd: reg}, (%{rbase: reg}+{offset: i16})                => 0xc5 @ rd @ rbase @ offset
    mov (%{rbase: reg}+{offset: i16}), %{rs: reg}                => 0xc7 @ rs @ rbase @ offset

    mov %{rd: reg}, (%{rbase: reg}+%{roffset: reg},{sclpow: i5}) => 0xc9 @ rd @ rbase @ 0x0 @ roffset @ 0b000 @ sclpow
    mov (%{rbase: reg}+%{roffset: reg},{sclpow:i5}), %{rs: reg}  => 0xcb @ rs @ rbase @ 0x0 @ roffset @ 0b000 @ sclpow

    push %{rs: reg}                         => 0xd0 @ 0x0 @ rs @ 0x0000
    pushi ${imm: i24}                       => 0xd1 @ imm
    pop %{rd: reg}                          => 0xd2 @ 0x0 @ rd @ 0x0000

    ; call the function at address << 8 (256-byte alignment)
    call {address: i32}                     => 0xd3 @ (address >> 8)`24

    ; wipe 'argw' number of bytes off the stack from arguments
    ret {argw: i24}                         => 0xd4 @ argw
    ret                                     => asm{ret 0}
    
    int {code: i8}                          => 0xd5 @ code @ 0x0000
    reti                                    => 0xd6 @ 0x000000

    out {port: i8}, %{rs: reg}                         => 0xe2 @ port @ 0x0 @ rs @ 0x00

    hlt                                    => 0xfe000000

    entry {address: i32}    => address ; specify the entry point to the code

    ; data@ {address: i16}      => address
}
