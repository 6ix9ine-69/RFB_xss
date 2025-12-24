#ifndef SYSLIB_OSENV_H_INCLUDED
#define SYSLIB_OSENV_H_INCLUDED

#include "syslib_exp.h"

SYSLIBEXP(LPWSTR) SysExpandEnvironmentStringsExW(LPCWSTR lpEnvStr);
SYSLIBEXP(LPSTR) SysExpandEnvironmentStringsExA(LPCSTR lpEnvStr);

#ifdef UNICODE
    #define SysExpandEnvironmentStringsEx SysExpandEnvironmentStringsExW
#else
    #define SysExpandEnvironmentStringsEx SysExpandEnvironmentStringsExA
#endif

#endif // SYSLIB_OSENV_H_INCLUDED
