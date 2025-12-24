#ifndef SYSLIB_NET_H_INCLUDED
#define SYSLIB_NET_H_INCLUDED

#include "syslib_exp.h"

SYSLIBEXP(UINT) NetResolveAddress(LPCSTR lpHost);
SYSLIBEXP(LPCSTR) NetNtoA(int iAddr);
SYSLIBEXP(void) NetCloseSocket(SOCKET hSock);

SYSLIBEXP(DWORD) NetUrlCalcEncodedSizeW(LPCWSTR lpIn,DWORD dwSize);
SYSLIBEXP(DWORD) NetUrlCalcEncodedSizeA(LPCSTR lpIn,DWORD dwSize);

#ifdef UNICODE
    #define NetUrlCalcEncodedSize NetUrlCalcEncodedSizeW
#else
    #define NetUrlCalcEncodedSize NetUrlCalcEncodedSizeA
#endif

SYSLIBEXP(BOOL) NetUrlEncodeBufferW(LPCWSTR lpIn,DWORD dwSize,LPWSTR lpOut,DWORD dwOutSize);
SYSLIBEXP(BOOL) NetUrlEncodeBufferA(LPCSTR lpIn,DWORD dwSize,LPSTR lpOut,DWORD dwOutSize);

#ifdef UNICODE
    #define NetUrlEncodeBuffer NetUrlEncodeBufferW
#else
    #define NetUrlEncodeBuffer NetUrlEncodeBufferA
#endif

#endif // SYSLIB_NET_H_INCLUDED
