#ifndef _CBUFFER_H
#define _CBUFFER_H

#include "MShadowLow.h"
void CBufferInit(int size);
void CBufferDeinit();

int  CBufferAdd(LTable* table);
void CBufferAccess(LTable* table);



#endif