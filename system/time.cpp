#include "sys_includes.h"

SYSLIBFUNC(DWORD) Now()
{
    LARGE_INTEGER liTime;
    NtQuerySystemTime(&liTime);

    DWORD dwNow;
    RtlTimeToSecondsSince1980(&liTime,&dwNow);
    return dwNow;
}

