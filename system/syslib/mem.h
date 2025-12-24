#ifndef SYSLIB_MEM_H_INCLUDED
#define SYSLIB_MEM_H_INCLUDED

#include <intrin.h>
#include "syslib_exp.h"

SYSLIBEXP(LPVOID) MemAlloc(size_t dwSize);
SYSLIBEXP(LPVOID) MemQuickAlloc(size_t dwSize);
SYSLIBEXP(LPVOID) MemRealloc(LPVOID lpMem,size_t dwSize);
SYSLIBEXP(size_t) MemGetSize(LPVOID lpMem);

SYSLIBEXP(void) MemFree(LPVOID lpMem);
SYSLIBEXP(void) MemZeroAndFree(LPVOID lpMem);

SYSLIBEXP(LPVOID) MemCopyEx(LPCVOID lpMem,size_t dwSize);

#define CHAR_Alloc(size)       (PCHAR)MemAlloc((size)*sizeof(CHAR))
#define CHAR_QuickAlloc(size)  (PCHAR)MemQuickAlloc((size)*sizeof(CHAR))
#define CHAR_Realloc(ptr,size) (PCHAR)MemRealloc(ptr,(size)*sizeof(CHAR))

#define WCHAR_Alloc(size)       (PWCHAR)MemAlloc((size)*sizeof(WCHAR))
#define WCHAR_QuickAlloc(size)  (PWCHAR)MemQuickAlloc((size)*sizeof(WCHAR))
#define WCHAR_Realloc(ptr,size) (PWCHAR)MemRealloc(ptr,(size)*sizeof(WCHAR))

#define TCHAR_Alloc(size)       (PTCHAR)MemAlloc((size)*sizeof(TCHAR))
#define TCHAR_QuickAlloc(size)  (PTCHAR)MemQuickAlloc((size)*sizeof(TCHAR))
#define TCHAR_Realloc(ptr,size) (PTCHAR)MemRealloc(ptr,(size)*sizeof(TCHAR))

SYSLIBEXP(void) MemFreeArrayOfPointers(LPVOID *lppMem,DWORD dwCount);

SYSLIBEXP(LPVOID) MemMem(LPVOID lpMem,SIZE_T dwMemSize,LPCVOID lpData,SIZE_T dwDataSize);
SYSLIBEXP(LPVOID) MemStrA(LPVOID lpMem,SIZE_T dwMemSize,LPCSTR lpData);

#define i_memcpy(dest, src, count) __movsb((LPBYTE)(dest), (LPCBYTE)(src), (SIZE_T)(count))
#define i_wmemcpy(dest, src, count) __movsw((LPWORD)(dest), (const LPWORD)(src), (SIZE_T)(count))
#define i_memset(dest, value, count) __stosb((LPBYTE)(dest), (BYTE)(value), (SIZE_T)(count))
#define i_wmemset(dest, value, count) __stosw((LPWORD)(dest), (WORD)(value), (SIZE_T)(count))

#endif // SYSLIB_MEM_H_INCLUDED
