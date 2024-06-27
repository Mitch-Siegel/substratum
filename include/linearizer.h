#ifndef LINEARIZER_H
#define LINEARIZER_H

#include "substratum_defs.h"
#include "symtab_scope.h"

struct AST;
struct SymbolTable;
struct Scope;
struct Type;
struct FunctionEntry;
struct StructEntry;
struct BasicBlock;
struct TACOperand;

// TODO: move to linearizer_generic
void reserve_and_store_stack_args(struct AST *callTree,
                              struct FunctionEntry *calledFunction,
                              struct Stack *argumentPushes,
                              struct BasicBlock *block,
                              size_t *TACIndex);

// TODO: move to linearizer_generic
struct TACLine *generate_call_tac(struct AST *callTree,
                                struct FunctionEntry *calledFunction,
                                struct BasicBlock *block,
                                size_t *TACIndex,
                                size_t *tempNum,
                                struct TACOperand *destinationOperand);

struct SymbolTable *walk_program(struct AST *program);

void walk_type_name(struct AST *tree, struct Scope *scope, struct Type *populateTypeTo);

struct VariableEntry *walk_variable_declaration(struct AST *tree,
                                              struct BasicBlock *block,
                                              struct Scope *scope,
                                              const size_t *tacIndex,
                                              const size_t *tempNum,
                                              u8 isArgument,
                                              enum ACCESS accessibility);

void walk_argument_declaration(struct AST *tree,
                             struct BasicBlock *block,
                             size_t *tacIndex,
                             size_t *tempNum,
                             struct FunctionEntry *fun);

struct FunctionEntry *walk_function_declaration(struct AST *tree,
                                              struct Scope *scope,
                                              struct StructEntry *methodOf,
                                              enum ACCESS accessibility);

void walk_function_definition(struct AST *tree,
                            struct FunctionEntry *fun);

void walk_method(struct AST *tree,
                struct StructEntry *methodOf,
                enum ACCESS accessibility);

void walk_implementation_block(struct AST *tree, struct Scope *scope);

void walk_struct_declaration(struct AST *tree,
                           struct BasicBlock *block,
                           struct Scope *scope);

void walk_enum_declaration(struct AST *tree,
                         struct BasicBlock *block,
                         struct Scope *scope);

void walk_statement(struct AST *tree,
                   struct BasicBlock **blockP,
                   struct Scope *scope,
                   size_t *tacIndex,
                   size_t *tempNum,
                   ssize_t *labelNum,
                   ssize_t controlConvergesToLabel);

void walk_scope(struct AST *tree,
               struct BasicBlock *block,
               struct Scope *scope,
               size_t *tacIndex,
               size_t *tempNum,
               ssize_t *labelNum,
               ssize_t controlConvergesToLabel);

// walk the logical operator pointed to by the AST
// returns the basic block which will be executed if the condition is met
// jumps to falseJumpLabelNum if the condition is not met
struct BasicBlock *walk_logical_operator(struct AST *tree,
                                       struct BasicBlock *block,
                                       struct Scope *scope,
                                       size_t *tacIndex,
                                       size_t *tempNum,
                                       ssize_t *labelNum,
                                       ssize_t falseJumpLabelNum);

// walk the condition check pointed to by the AST
// returns the basic block which will be executed if the condition is met
// jumps to falseJumpLabelNum if the condition is not met
struct BasicBlock *walk_condition_check(struct AST *tree,
                                      struct BasicBlock *block,
                                      struct Scope *scope,
                                      size_t *tacIndex,
                                      size_t *tempNum,
                                      ssize_t *labelNum,
                                      ssize_t falseJumpLabelNum);

void walk_while_loop(struct AST *tree,
                   struct BasicBlock *block,
                   struct Scope *scope,
                   size_t *tacIndex,
                   size_t *tempNum,
                   ssize_t *labelNum,
                   ssize_t controlConvergesToLabel);

void walk_if_statement(struct AST *tree,
                     struct BasicBlock *block,
                     struct Scope *scope,
                     size_t *tacIndex,
                     size_t *tempNum,
                     ssize_t *labelNum,
                     ssize_t controlConvergesToLabel);

void walk_for_loop(struct AST *tree,
                 struct BasicBlock *block,
                 struct Scope *scope,
                 size_t *tacIndex,
                 size_t *tempNum,
                 ssize_t *labelNum,
                 ssize_t controlConvergesToLabel);

void walk_match_statement(struct AST *tree,
                        struct BasicBlock *block,
                        struct Scope *scope,
                        size_t *tacIndex,
                        size_t *tempNum,
                        ssize_t *labelNum,
                        ssize_t controlConvergesToLabel);

void walk_assignment(struct AST *tree,
                    struct BasicBlock *block,
                    struct Scope *scope,
                    size_t *tacIndex,
                    size_t *tempNum);

void walk_arithmetic_assignment(struct AST *tree,
                              struct BasicBlock *block,
                              struct Scope *scope,
                              size_t *tacIndex,
                              size_t *tempNum);

void walk_struct_initializer(struct AST *tree,
                           struct BasicBlock *block,
                           struct Scope *scope,
                           size_t *TACIndex,
                           size_t *tempNum,
                           struct TACOperand *initialized);

void walk_sub_expression(struct AST *tree,
                       struct BasicBlock *block,
                       struct Scope *scope,
                       size_t *tacIndex,
                       size_t *tempNum,
                       struct TACOperand *destinationOperand);

void walk_method_call(struct AST *tree,
                    struct BasicBlock *block,
                    struct Scope *scope,
                    size_t *TACIndex,
                    size_t *tempNum,
                    struct TACOperand *destinationOperand);

void walk_associated_call(struct AST *tree,
                        struct BasicBlock *block,
                        struct Scope *scope,
                        size_t *TACIndex,
                        size_t *tempNum,
                        struct TACOperand *destinationOperand);

void walk_function_call(struct AST *tree,
                      struct BasicBlock *block,
                      struct Scope *scope,
                      size_t *tacIndex,
                      size_t *tempNum,
                      struct TACOperand *destinationOperand);

struct TACLine *walk_member_access(struct AST *tree,
                                 struct BasicBlock *block,
                                 struct Scope *scope,
                                 size_t *tacIndex,
                                 size_t *tempNum,
                                 struct TACOperand *srcDestOperand,
                                 size_t depth);

void walk_non_pointer_arithmetic(struct AST *tree,
                              struct BasicBlock *block,
                              struct Scope *scope,
                              size_t *TACIndex,
                              size_t *tempNum,
                              struct TACLine *expression);

struct TACOperand *walk_expression(struct AST *tree,
                                  struct BasicBlock *block,
                                  struct Scope *scope,
                                  size_t *tacIndex,
                                  size_t *tempNum);

struct TACLine *walk_array_ref(struct AST *tree,
                             struct BasicBlock *block,
                             struct Scope *scope,
                             size_t *tacIndex,
                             size_t *tempNum);

struct TACOperand *walk_dereference(struct AST *tree,
                                   struct BasicBlock *block,
                                   struct Scope *scope,
                                   size_t *tacIndex,
                                   size_t *tempNum);

struct TACOperand *walk_addr_of(struct AST *tree,
                              struct BasicBlock *block,
                              struct Scope *scope,
                              size_t *tacIndex,
                              size_t *tempNum);

void walk_pointer_arithmetic(struct AST *tree,
                           struct BasicBlock *block,
                           struct Scope *scope,
                           size_t *tacIndex,
                           size_t *tempNum,
                           struct TACOperand *destinationOperand);

void walk_asm_block(struct AST *tree,
                  struct BasicBlock *block,
                  struct Scope *scope,
                  size_t *tacIndex,
                  size_t *tempNum);

void walk_string_literal(struct AST *tree,
                       struct BasicBlock *block,
                       struct Scope *scope,
                       struct TACOperand *destinationOperand);

void walk_sizeof(struct AST *tree,
                struct BasicBlock *block,
                struct Scope *scope,
                struct TACOperand *destinationOperand);

#endif
