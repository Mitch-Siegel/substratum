#ifndef LIVEVARS_H
#define LIVEVARS_H

#include "idfa.h"

int compareTacOperand(void *dataA, void *dataB);

int compareTacOperandIgnoreSsaNumber(void *dataA, void *dataB);

void printTACOperand(void *operandData);

struct Idfa *analyzeLiveVars(struct IdfaContext *context);

#endif
