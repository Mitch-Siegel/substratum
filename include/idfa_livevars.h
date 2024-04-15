#ifndef LIVEVARS_H
#define LIVEVARS_H

#include "idfa.h"

ssize_t compareTacOperand(void *dataA, void *dataB);

ssize_t compareTacOperandIgnoreSsaNumber(void *dataA, void *dataB);

void printTACOperand(void *operandData);

struct Idfa *analyzeLiveVars(struct IdfaContext *context);

#endif
