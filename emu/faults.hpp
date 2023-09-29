#ifndef _FAULTS_HPP_
#define _FAULTS_HPP_

enum class Fault
{
    NO_FAULT,
    // program counter not aligned to a multiple of 4
    PC_ALIGNMENT,
    // not a real instruction
    INVALID_OPCODE,
    // attempt to run a pop instruction that would overflow sp
    STACK_UNDERFLOW,
    // ret instruction was called when sp != bp
    RETURN_STACK_CORRUPT,
    // the physical address of the page table base exceeds the physical memory size of the system
    PTB_PHYS,
    // attempt to access a virtual address not mapped
    INVALID_PAGE,
    // the physical address decoded from the page table exceeds the physical memory size of the system
    PTD_INVALID,
    // the core ran a hlt instruction
    HALTED,
};

#endif
