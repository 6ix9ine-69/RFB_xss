#include "sys_includes.h"

#include <syslib\str.h>
#include <syslib\mem.h>

static bool NtDeleteFileW(PCWSTR lpFileName)
{
    bool bRet=false;

    WCHAR szFullPath[MAX_PATH*2];
    if (SearchPath(NULL,lpFileName,NULL,ARRAYSIZE(szFullPath),szFullPath,NULL))
    {
        UNICODE_STRING usNtPath;
        if (RtlDosPathNameToNtPathName_U(szFullPath,&usNtPath,NULL,NULL))
        {
            OBJECT_ATTRIBUTES ObjectAttributes;
            InitializeObjectAttributes(&ObjectAttributes,&usNtPath,OBJ_CASE_INSENSITIVE,NULL,NULL);
            bRet=(SUCCEEDED(NtDeleteFile(&ObjectAttributes)));
            RtlFreeUnicodeString(&usNtPath);
        }
    }
    return bRet;
}

SYSLIBFUNC(BOOL) RemoveFileW(LPCWSTR lpFile)
{
    BOOL bRet=false;
    int dwCount=0;
    SetFileAttributesW(lpFile,FILE_ATTRIBUTE_NORMAL);
    while (true)
    {
        bRet=(NtDeleteFileW(lpFile) != false);
        if ((bRet) || (dwCount++ > 20))
            break;

        bRet=(GetLastError() == ERROR_FILE_NOT_FOUND);
        if (bRet)
            break;

        Sleep(10);
    }
    return bRet;
}

SYSLIBFUNC(BOOL) RemoveFileA(LPCSTR lpFile)
{
    LPWSTR lpFileNameW=StrAnsiToUnicodeEx(lpFile,0,NULL);

    BOOL bRet=RemoveFileW(lpFileNameW);

    MemFree(lpFileNameW);
    return bRet;
}

