#ifndef SYSLIB_UTILS_H_INCLUDED
#define SYSLIB_UTILS_H_INCLUDED

#include "syslib_exp.h"

SYSLIBEXP(DWORD) xor128(int val);
SYSLIBEXP(DWORD) xor128_Between(int iMin,int iMax);

SYSLIBEXP(DWORD) GetRndDWORD();

#endif // SYSLIB_UTILS_H_INCLUDED
