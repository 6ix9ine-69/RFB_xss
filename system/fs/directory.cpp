#include "sys_includes.h"
#include <shlwapi.h>

#include <syslib\str.h>
#include <syslib\mem.h>
#include <syslib\files.h>

#include "findfiles.h"

SYSLIBFUNC(BOOL) CreateDirectoryTreeW(LPCWSTR lpPath)
{
    BOOL bRet=false;
    LPWSTR p=PathSkipRootW(lpPath);
    if (!p)
        p=(LPWSTR)lpPath;

    for (;; p++)
    {
        if ((*p == L'\\') || (*p == L'/') || (!*p))
        {
            WCHAR wOld=*p;
            *p=0;

            DWORD dwAttr=GetFileAttributesW(lpPath);
            if (dwAttr == INVALID_FILE_ATTRIBUTES)
            {
                if (CreateDirectoryW(lpPath,0) == FALSE)
                    break;
            }
            else if (!(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
                break;

            if (!wOld)
            {
                bRet=true;
                break;
            }

            *p=wOld;
        }
    }
    return bRet;
}

SYSLIBFUNC(BOOL) CreateDirectoryTreeA(LPCSTR lpPath)
{
    LPWSTR lpPathW=StrAnsiToUnicodeEx(lpPath,0,NULL);

    BOOL bRet=CreateDirectoryTreeW(lpPathW);

    MemFree(lpPathW);
    return bRet;
}

SYSLIBFUNC(BOOL) RemoveDirectoryTreeW(LPCWSTR lpDir)
{
    BOOL bRet=false;

    WIN32_FIND_DATAW fd;

    WCHAR stPath[MAX_PATH];
    wsprintfW(stPath,L"%s\\*",lpDir);

    HANDLE hFind=FindFirstFileW(stPath,&fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        bRet=true;
        do
        {
            if (SYSLIB::IsDotsNameW(fd.cFileName))
                continue;

            wsprintfW(stPath,L"%s\\%s",lpDir,fd.cFileName);

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                bRet=RemoveDirectoryTreeW(stPath);
            else
                bRet=RemoveFileW(stPath);
        }
        while (FindNextFileW(hFind,&fd));
        FindClose(hFind);
    }
    if (bRet)
        bRet=(RemoveDirectoryW(lpDir) != FALSE);

    return bRet;
}

SYSLIBFUNC(BOOL) RemoveDirectoryTreeA(LPCSTR lpDir)
{
    LPWSTR lpDirW=StrAnsiToUnicodeEx(lpDir,0,NULL);

    BOOL bRet=RemoveDirectoryTreeW(lpDirW);

    MemFree(lpDirW);
    return bRet;
}

