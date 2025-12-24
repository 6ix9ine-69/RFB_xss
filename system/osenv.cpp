#include "sys_includes.h"

#include <syslib\mem.h>

SYSLIBFUNC(LPWSTR) SysExpandEnvironmentStringsExW(LPCWSTR lpEnvStr)
{
    LPWSTR lpRes=NULL;
    do
    {
        if (!lpEnvStr[0])
            break;

        DWORD dwNeededBytes=ExpandEnvironmentStringsW(lpEnvStr,NULL,0);
        if (!dwNeededBytes)
            break;

        lpRes=WCHAR_QuickAlloc(dwNeededBytes);
        if (!lpRes)
            break;

        ExpandEnvironmentStringsW(lpEnvStr,lpRes,dwNeededBytes);
    }
    while (false);
    return lpRes;
}

SYSLIBFUNC(LPSTR) SysExpandEnvironmentStringsExA(LPCSTR lpEnvStr)
{
    LPSTR lpRes=NULL;
    do
    {
        DWORD dwNeededBytes=ExpandEnvironmentStringsA(lpEnvStr,NULL,0);
        if (!dwNeededBytes)
            break;

        lpRes=(LPSTR)MemQuickAlloc(dwNeededBytes);
        if (!lpRes)
            break;

        ExpandEnvironmentStringsA(lpEnvStr,lpRes,dwNeededBytes);
    }
    while (false);
    return lpRes;
}

