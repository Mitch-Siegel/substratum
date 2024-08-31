#ifndef TAC_H
#define TAC_H

#include "ast.h"
#include "tac_operand.h"
#include "type.h"

#include <mbcl/deque.h>

#define N_TAC_OPERANDS_IN_LINE 3

enum TAC_TYPE
{
    TT_ASM,
    TT_ASM_LOAD,
    TT_ASM_STORE,
    TT_ASSIGN,
    TT_ADD,
    TT_SUBTRACT,
    TT_MUL,
    TT_DIV,
    TT_MODULO,
    TT_BITWISE_AND,
    TT_BITWISE_OR,
    TT_BITWISE_XOR,
    TT_BITWISE_NOT,
    TT_LSHIFT,
    TT_RSHIFT,
    TT_LOAD,
    TT_STORE,
    TT_ADDROF,
    TT_ARRAY_LOAD,  // load an array element
    TT_ARRAY_LEA,   // load a pointer to an array element
    TT_ARRAY_STORE, // store an array element
    TT_FIELD_LOAD,  // load a field of a struct
    TT_FIELD_LEA,   // load a pointer to a field of a struct
    TT_FIELD_STORE, // store a field of a struct
    TT_SIZEOF,      // get the size of a type
    TT_BEQ,         // branch equal
    TT_BNE,         // branch not equal
    TT_BGEU,        // branch greater than or equal unsigned
    TT_BLTU,        // branch less than unsigned
    TT_BGTU,        // branch greater than unsigned
    TT_BLEU,        // branch less than or equal unsigned
    TT_BEQZ,        // branch equal zero
    TT_BNEZ,        // branch not equal zero
    TT_JMP,
    TT_FUNCTION_CALL,   // call a function
    TT_METHOD_CALL,     // call a method of a struct
    TT_ASSOCIATED_CALL, // call an associated function of a struct
    TT_LABEL,
    TT_RETURN,
    TT_DO,
    TT_ENDDO,
    TT_PHI,
};

// TT_ASM
struct TacAsm
{
    char *asmString;
};

// TT_ASM_LOAD
struct TacAsmLoad
{
    char *destRegisterName;
    struct TACOperand sourceOperand;
};

// TT_ASM_STORE
struct TacAsmStore
{
    struct TACOperand destinationOperand;
    char *sourceRegisterName;
};

// TT_ASSIGN
struct TacAssign
{
    struct TACOperand destination;
    struct TACOperand source;
};

// TT_ADD,
// TT_SUBTRACT,
// TT_MUL,
// TT_DIV,
// TT_MODULO,
// TT_BITWISE_AND,
// TT_BITWISE_OR,
// TT_BITWISE_XOR,
// TT_BITWISE_NOT,
// TT_LSHIFT,
// TT_RSHIFT,
struct TacArithmetic
{
    struct TACOperand destination;
    struct TACOperand sourceA;
    struct TACOperand sourceB;
};

// TT_LOAD,
struct TacLoad
{
    struct TACOperand address;
    struct TACOperand destination;
};

// TT_STORE,
struct TacStore
{
    struct TACOperand source;
    struct TACOperand address;
};

// TT_ADDROF,
struct TacAddrOf
{
    struct TACOperand destination;
    struct TACOperand source;
};

// TT_ARRAY_LOAD,  // load an array element
// TT_ARRAY_LEA,   // load a pointer to an array element
struct TacArrayLoad
{
    struct TACOperand destination;
    struct TACOperand array;
    struct TACOperand index;
};

// TT_ARRAY_STORE, // store an array element
struct TacArrayStore
{
    struct TACOperand array;
    struct TACOperand index;
    struct TACOperand source;
};
// TT_FIELD_LOAD,  // load a field of a struct
// TT_FIELD_LEA,   // load a pointer to a field of a struct
struct TacFieldLoad
{
    struct TACOperand destination;
    struct TACOperand source;
    char *fieldName;
};
// TT_FIELD_STORE, // store a field of a struct
struct TacFieldStore
{
    struct TACOperand source;
    struct TACOperand destination;
    char *fieldName;
};

struct TacSizeof
{
    struct TACOperand destination;
    struct Type type;
};

// TT_BEQ,         // branch equal
// TT_BNE,         // branch not equal
// TT_BGEU,        // branch greater than or equal unsigned
// TT_BLTU,        // branch less than unsigned
// TT_BGTU,        // branch greater than unsigned
// TT_BLEU,        // branch less than or equal unsigned
// TT_BEQZ,        // branch equal zero
// TT_BNEZ,        // branch not equal zero
struct TacConditionalBranch
{
    struct TACOperand sourceA;
    struct TACOperand sourceB;
    ssize_t label;
};

// TT_JMP,
struct TacJump
{
    ssize_t label;
};

// TT_ARG_STORE,       // store a value at a (positive) offset from the stack pointer
// TT_FUNCTION_CALL,   // call a function
struct TacFunctionCall
{
    struct TACOperand returnValue;
    char *functionName;
    Deque *arguments;
};
// TT_METHOD_CALL,     // call a method of a struct
struct TacMethodCall
{
    struct TACOperand returnValue;
    struct TACOperand calledOn;
    char *methodName;
    Deque *arguments;
};
// TT_ASSOCIATED_CALL, // call an associated function of a struct
struct TacAssociatedCall
{
    struct TACOperand returnValue;
    struct Type associatedWith;
    char *functionName;
    Deque *arguments;
};
// TT_LABEL,
struct TacLabel
{
    ssize_t labelNumber;
};
// TT_RETURN,
struct TacReturn
{
    struct TACOperand returnValue;
};
// TT_DO,
// TT_ENDDO,
// TT_PHI,
struct TacPhi
{
    struct TACOperand destination;
    Deque *sources;
};

struct TACLine
{
    char *allocFile;
    int allocLine;
    // store the actual tree because some trees are manually generated and do not exist in the true parse tree
    // such as the += operator (a += b is transformed into a tree corresponding to a = a + b)
    struct Ast correspondingTree;
    union
    {
        struct TacAsm asm_;
        struct TacAsmLoad asmLoad;
        struct TacAsmStore asmStore;
        struct TacAssign assign;
        struct TacArithmetic arithmetic;
        struct TacLoad load;
        struct TacStore store;
        struct TacAddrOf addrof;
        struct TacArrayLoad arrayLoad;
        struct TacArrayStore arrayStore;
        struct TacFieldLoad fieldLoad;
        struct TacFieldStore fieldStore;
        struct TacSizeof sizeof_;
        struct TacConditionalBranch conditionalBranch;
        struct TacJump jump;
        struct TacFunctionCall functionCall;
        struct TacMethodCall methodCall;
        struct TacAssociatedCall associatedCall;
        struct TacLabel label;
        struct TacReturn return_;
        struct TacPhi phi;
    } operands;
    enum TAC_TYPE operation;
    // numerical index relative to other TAC lines
    size_t index;
    // numerical index in terms of emitted instructions (from function entry point, populated during code generation)
    size_t asmIndex;
    u8 reorderable;
};

char *tac_operation_get_name(enum TAC_TYPE tacOperation);

void print_tac_line(struct TACLine *line);

char *sprint_tac_line(struct TACLine *line);

struct TACLine *new_tac_line_function(enum TAC_TYPE operation, struct Ast *correspondingTree, char *file, int line);
#define new_tac_line(operation, correspondingTree) new_tac_line_function((operation), (correspondingTree), __FILE__, __LINE__)

bool tac_line_is_jump(struct TACLine *line);

ssize_t tac_get_jump_target(struct TACLine *line);

void free_tac(struct TACLine *line);

// Enum denoting how a particular TAC operand is used
struct OperandUsages
{
    Deque *reads;
    Deque *writes;
};

struct OperandUsages get_operand_usages(struct TACLine *line);

struct LinearizationResult
{
    struct BasicBlock *block;
    int endingTACIndex;
};

#endif
