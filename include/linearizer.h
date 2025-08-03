#ifndef LINEARIZER_H
#define LINEARIZER_H

#include "substratum_defs.h"
#include "symtab_scope.h"

#include "mbcl/deque.h"

struct Ast;
struct SymbolTable;
struct Scope;
struct Type;
struct FunctionEntry;
struct StructDesc;
struct BasicBlock;
struct TACOperand;

// TODO: move to linearizer_generic
void reserve_and_store_stack_args(struct Ast *callTree,
                                  struct FunctionEntry *calledFunction,
                                  Deque *argumentPushes,
                                  struct BasicBlock *block,
                                  size_t *tacIndex);

struct SymbolTable *walk_program(struct Ast *program);

void walk_type_name(struct Ast *tree, struct Scope *scope,
                    struct Type *populateTypeTo,
                    struct TypeEntry *fieldOf);

void walk_extern(struct Ast *tree, struct Scope *scope, size_t *tacIndex);

struct VariableEntry *walk_variable_declaration(struct Ast *tree,
                                                struct Scope *scope,
                                                const size_t *tacIndex,
                                                const u8 isArgument);

struct VariableEntry *walk_field_declaration(struct Ast *tree,
                                             struct Scope *scope,
                                             const size_t *tacIndex,
                                             const enum ACCESS accessibility,
                                             struct TypeEntry *fieldOf);

void walk_argument_declaration(struct Ast *tree,
                               size_t *tacIndex,
                               struct FunctionEntry *fun);

struct FunctionEntry *walk_function_declaration(struct Ast *tree,
                                                struct Scope *scope,
                                                struct TypeEntry *implementedFor,
                                                enum ACCESS accessibility,
                                                bool forTrait);

void walk_function_definition(struct Ast *tree,
                              struct FunctionEntry *fun);

struct FunctionEntry *walk_implemented_function(struct Ast *tree,
                                                struct TypeEntry *implementedFor,
                                                enum ACCESS accessibility);

void walk_implementation_block(struct Ast *tree, struct Scope *scope);

struct StructDesc *walk_struct_declaration(struct Ast *tree,
                                           struct Scope *scope,
                                           List *genericParams);

void walk_generic(struct Ast *tree,
                  struct Scope *scope);

void walk_trait_declaration(struct Ast *tree, struct Scope *scope);

void walk_enum_declaration(struct Ast *tree,
                           struct BasicBlock *block,
                           struct Scope *scope,
                           List *genericParams);

void walk_statement(struct Ast *tree,
                    struct BasicBlock **blockP,
                    struct Scope *scope,
                    size_t *tacIndex,
                    ssize_t *labelNum,
                    ssize_t controlConvergesToLabel);

void walk_scope(struct Ast *tree,
                struct BasicBlock *block,
                struct Scope *scope,
                size_t *tacIndex,
                ssize_t *labelNum,
                ssize_t controlConvergesToLabel);

// walk the logical operator pointed to by the AST
// returns the basic block which will be executed if the condition is met
// jumps to falseJumpLabelNum if the condition is not met
struct BasicBlock *walk_logical_operator(struct Ast *tree,
                                         struct BasicBlock *block,
                                         struct Scope *scope,
                                         size_t *tacIndex,
                                         ssize_t *labelNum,
                                         ssize_t falseJumpLabelNum);

// walk the condition check pointed to by the AST
// returns the basic block which will be executed if the condition is met
// jumps to falseJumpLabelNum if the condition is not met
struct BasicBlock *walk_condition_check(struct Ast *tree,
                                        struct BasicBlock *block,
                                        struct Scope *scope,
                                        size_t *tacIndex,
                                        ssize_t *labelNum,
                                        ssize_t falseJumpLabelNum);

void walk_while_loop(struct Ast *tree,
                     struct BasicBlock *block,
                     struct Scope *scope,
                     size_t *tacIndex,
                     ssize_t *labelNum,
                     ssize_t controlConvergesToLabel);

void walk_if_statement(struct Ast *tree,
                       struct BasicBlock *block,
                       struct Scope *scope,
                       size_t *tacIndex,
                       ssize_t *labelNum,
                       ssize_t controlConvergesToLabel);

void walk_for_loop(struct Ast *tree,
                   struct BasicBlock *block,
                   struct Scope *scope,
                   size_t *tacIndex,
                   ssize_t *labelNum,
                   ssize_t controlConvergesToLabel);

void walk_match_statement(struct Ast *tree,
                          struct BasicBlock *block,
                          struct Scope *scope,
                          size_t *tacIndex,
                          ssize_t *labelNum,
                          ssize_t controlConvergesToLabel);

void walk_assignment(struct Ast *tree,
                     struct BasicBlock *block,
                     struct Scope *scope,
                     size_t *tacIndex);

void walk_arithmetic_assignment(struct Ast *tree,
                                struct BasicBlock *block,
                                struct Scope *scope,
                                size_t *tacIndex);

void walk_initializer(struct Ast *tree,
                      struct BasicBlock *block,
                      struct Scope *scope,
                      size_t *tacIndex,
                      struct TACOperand *initialized);

void walk_sub_expression(struct Ast *tree,
                         struct BasicBlock *block,
                         struct Scope *scope,
                         size_t *tacIndex,
                         struct TACOperand *destinationOperand);

void walk_method_call(struct Ast *tree,
                      struct BasicBlock *block,
                      struct Scope *scope,
                      size_t *tacIndex,
                      struct TACOperand *destinationOperand);

struct TypeEntry *walk_type_name_or_generic_instantiation(struct Scope *scope,
                                                          struct Ast *tree,
                                                          struct TypeEntry *fieldOf);

void walk_associated_call(struct Ast *tree,
                          struct BasicBlock *block,
                          struct Scope *scope,
                          size_t *tacIndex,
                          struct TACOperand *destinationOperand);

void walk_function_call(struct Ast *tree,
                        struct BasicBlock *block,
                        struct Scope *scope,
                        size_t *tacIndex,
                        struct TACOperand *destinationOperand);

struct TACLine *walk_field_access(struct Ast *tree,
                                  struct BasicBlock *block,
                                  struct Scope *scope,
                                  size_t *tacIndex,
                                  struct TACOperand *destinationOperand,
                                  size_t depth);

void walk_non_pointer_arithmetic(struct Ast *tree,
                                 struct BasicBlock *block,
                                 struct Scope *scope,
                                 size_t *tacIndex,
                                 struct TACLine *expression);

struct TACOperand *walk_expression(struct Ast *tree,
                                   struct BasicBlock *block,
                                   struct Scope *scope,
                                   size_t *tacIndex);

struct TACLine *walk_array_read(struct Ast *tree,
                                struct BasicBlock *block,
                                struct Scope *scope,
                                size_t *tacIndex);

struct TACOperand *walk_dereference(struct Ast *tree,
                                    struct BasicBlock *block,
                                    struct Scope *scope,
                                    size_t *tacIndex);

struct TACOperand *walk_addr_of(struct Ast *tree,
                                struct BasicBlock *block,
                                struct Scope *scope,
                                size_t *tacIndex);

void walk_pointer_arithmetic(struct Ast *tree,
                             struct BasicBlock *block,
                             struct Scope *scope,
                             size_t *tacIndex,
                             struct TACOperand *destinationOperand);

void walk_asm_block(struct Ast *tree,
                    struct BasicBlock *block,
                    struct Scope *scope,
                    size_t *tacIndex);

void walk_string_literal(struct Ast *tree,
                         struct BasicBlock *block,
                         struct Scope *scope,
                         struct TACOperand *destinationOperand);

void walk_sizeof(struct Ast *tree,
                 struct BasicBlock *block,
                 struct Scope *scope,
                 size_t *tacIndex,
                 struct TACOperand *destinationOperand);

#endif
