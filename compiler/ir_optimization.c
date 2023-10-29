#include "ir_optimization.h"

#include "symtab.h"

void optimizeIRForBlock(struct Scope *scope, struct BasicBlock *block)
{
    
}

void optimizeIRForScope(struct Scope *scope)
{
    // second pass: rename basic block operands relevant to the current scope
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *thisMember = scope->entries->data[i];
		switch (thisMember->type)
		{
		case e_function:
        {
            struct FunctionEntry *theFunction = thisMember->entry;
            optimizeIRForScope(theFunction->mainScope);
        }
        break;

		case e_scope:
        {
            struct Scope *theScope = thisMember->entry;
            optimizeIRForScope(theScope);
        }
        break;

		case e_basicblock:
        {
            struct BasicBlock *theBlock = thisMember->entry;
            optimizeIRForBasicBlock(scope, theBlock);
        }
        break;

        case e_variable:
		case e_argument:
		case e_class:
        break;
        }
    }
}

void optimizeIR(struct SymbolTable *table)
{
    optimizeIRForScope(table->globalScope);
}