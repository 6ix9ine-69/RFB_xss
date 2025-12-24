#ifndef INCLUDES_H_INCLUDED
#define INCLUDES_H_INCLUDED

#include <winsock2.h>
#include <windows.h>
#include <ntdll.h>
#include <tchar.h>

#define SYSLIBFUNC(x) __declspec(noinline) extern "C" x __cdecl

#endif // INCLUDES_H_INCLUDED
