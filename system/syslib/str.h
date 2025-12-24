#ifndef SYSLIB_STR_H_INCLUDED
#define SYSLIB_STR_H_INCLUDED

#include "syslib_exp.h"

SYSLIBEXP(DWORD) StrUnicodeToAnsi(LPCWSTR lpSource,DWORD dwSourceSize,LPSTR lpDest,DWORD dwDestSize);
SYSLIBEXP(LPSTR) StrUnicodeToAnsiEx(LPCWSTR lpSource,DWORD dwSourceSize,LPDWORD lpOutSize);
SYSLIBEXP(LPWSTR) StrAnsiToUnicodeEx(LPCSTR lpSource,DWORD dwSourceSize,LPDWORD lpOutSize);
SYSLIBEXP(LPSTR) StrUnicodeToOemEx(LPCWSTR lpSource,DWORD dwSourceSize,LPDWORD lpOutSize);
SYSLIBEXP(LPWSTR) StrOemToUnicodeEx(LPCSTR lpSource,DWORD dwSourceSize,LPDWORD lpOutSize);
SYSLIBEXP(LPSTR) StrDuplicateA(LPCSTR lpSource,DWORD dwLen);
SYSLIBEXP(LPWSTR) StrDuplicateW(LPCWSTR lpSource,DWORD dwLen);

#ifdef UNICODE
    #define StrDuplicate StrDuplicateW
#else
    #define StrDuplicate StrDuplicateA
#endif

SYSLIBEXP(DWORD) StrToHexW(LPCWSTR lpStr);
SYSLIBEXP(DWORD) StrToHexA(LPCSTR lpStr);

#ifdef UNICODE
    #define StrToHex StrToHexW
#else
    #define StrToHex StrToHexA
#endif

SYSLIBEXP(DWORD64) StrToHex64W(LPCWSTR lpStr);
SYSLIBEXP(DWORD64) StrToHex64A(LPCSTR lpStr);

#ifdef UNICODE
	#define StrToHex64 StrToHex64W
#else
	#define StrToHex64 StrToHex64A
#endif

SYSLIBEXP(DWORD) StrCatFormatExW(LPWSTR *lppDest,DWORD dwDestSize,LPCWSTR lpFormat,...);
SYSLIBEXP(DWORD) StrCatFormatExA(LPSTR *lppDest,DWORD dwDestSize,LPCSTR lpFormat,...);

#ifdef UNICODE
    #define StrCatFormatEx StrCatFormatExW
#else
    #define StrCatFormatEx StrCatFormatExA
#endif

SYSLIBEXP(DWORD) StrFormatExW(LPWSTR *lppDest,LPCWSTR lpFormat,...);
SYSLIBEXP(DWORD) StrFormatExA(LPSTR *lppDest,LPCSTR lpFormat,...);

#ifdef UNICODE
    #define StrFormatEx StrFormatExW
#else
    #define StrFormatEx StrFormatExA
#endif

SYSLIBEXP(int) StrNumberFormatA(LONGLONG llData,LPSTR lpFormatted,DWORD dwFormattedLen,LPSTR lpSep);
SYSLIBEXP(int) StrNumberFormatW(LONGLONG llData,LPWSTR lpFormatted,DWORD dwFormattedLen,LPWSTR lpSep);

#ifdef UNICODE
    #define StrNumberFormat StrNumberFormatW
#else
    #define StrNumberFormat StrNumberFormatA
#endif


#endif // SYSLIB_STR_H_INCLUDED
