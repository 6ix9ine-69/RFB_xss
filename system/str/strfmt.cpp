#include "sys_includes.h"
#include <shlwapi.h>

#include "strfmt.h"
#include <syslib\mem.h>

SYSLIBFUNC(LPWSTR) StrDuplicateW(LPCWSTR lpSource,DWORD dwSize)
{
    LPWSTR lpOut=NULL;
    do
    {
        if (!dwSize)
        {
            dwSize=lstrlenW(lpSource);
            if (!dwSize)
                break;
        }

        lpOut=WCHAR_QuickAlloc(dwSize+1);
        if (!lpOut)
            break;

        lstrcpynW(lpOut,lpSource,dwSize+1);
    }
    while (false);

    return lpOut;
}

SYSLIBFUNC(LPSTR) StrDuplicateA(LPCSTR lpSource,DWORD dwSize)
{
    LPSTR lpOut=NULL;
    do
    {
        if (!dwSize)
        {
            dwSize=lstrlenA(lpSource);
            if (!dwSize)
                break;
        }

        lpOut=(char*)MemQuickAlloc(dwSize+1);
        if (!lpOut)
            break;

        lstrcpynA(lpOut,lpSource,dwSize+1);
    }
    while (false);

    return lpOut;
}

namespace SYSLIB
{
    DWORD StrFmt_FormatStringW(LPWSTR lpDest,LPCWSTR lpFormat,va_list args)
    {
        DWORD dwSize=0;

        if (!lpDest)
        {
            int iRealSize;
            DWORD dwBufferSize=lstrlenW(lpFormat);
            do
            {
                dwBufferSize+=100;
                if (dwBufferSize > MAX_SPRINTF_STRING_SIZE)
                    break;

                lpDest=(LPWSTR)MemRealloc(lpDest,(dwBufferSize+1)*sizeof(WCHAR));
                if (!lpDest)
                    break;
            }
            while (((iRealSize=wvnsprintfW(lpDest,dwBufferSize,lpFormat,args)) < 0) || (iRealSize >= (int)(dwBufferSize-1)));

            if (iRealSize >= 0)
                dwSize=iRealSize;
        }
        else
            dwSize=wvsprintfW(lpDest,lpFormat,args);

        return dwSize;
    }

    DWORD StrFmt_FormatStringA(LPSTR lpDest,LPCSTR lpFormat,va_list args)
    {
        DWORD dwSize=0;

        if (!lpDest)
        {
            int iRealSize;
            DWORD dwBufferSize=lstrlenA(lpFormat);
            do
            {
                dwBufferSize+=100;
                if (dwBufferSize > MAX_SPRINTF_STRING_SIZE)
                    break;

                lpDest=(LPSTR)MemRealloc(lpDest,(dwBufferSize+1)*sizeof(char));
                if (!lpDest)
                    break;
            }
            while (((iRealSize=wvnsprintfA(lpDest,dwBufferSize,lpFormat,args)) < 0) || (iRealSize >= (int)(dwBufferSize-1)));

            if (iRealSize >= 0)
                dwSize=iRealSize;
        }
        else
            dwSize=wvsprintfA(lpDest,lpFormat,args);

        return dwSize;
    }

    DWORD wsprintfExW(LPWSTR *lppBuffer,DWORD dwOffset,LPCWSTR lpFormat,va_list args)
    {
        DWORD dwNewSize=0;
        do
        {
            DWORD dwBufferSize=StrFmt_FormatStringW(NULL,lpFormat,args);
            if (!dwBufferSize)
                break;

            LPWSTR lpBuffer=WCHAR_Realloc(*lppBuffer,dwBufferSize+dwOffset+1);
            if (!lpBuffer)
                break;

            dwNewSize=StrFmt_FormatStringW(lpBuffer+dwOffset,lpFormat,args);
            if (!dwNewSize)
                break;

            dwNewSize+=dwOffset;
            *lppBuffer=lpBuffer;
        }
        while (false);

        return dwNewSize;
    }

    DWORD wsprintfExA(LPSTR *lppBuffer,DWORD dwOffset,LPCSTR lpFormat,va_list args)
    {
        DWORD dwNewSize=0;
        do
        {
            DWORD dwBufferSize=StrFmt_FormatStringA(NULL,lpFormat,args);
            if (!dwBufferSize)
                break;

            LPSTR lpBuffer=(LPSTR)MemRealloc(*lppBuffer,dwBufferSize+dwOffset+1);
            if (!lpBuffer)
                break;

            dwNewSize=StrFmt_FormatStringA(lpBuffer+dwOffset,lpFormat,args);
            if (!dwNewSize)
                break;

            dwNewSize+=dwOffset;
            *lppBuffer=lpBuffer;
        }
        while (false);

        return dwNewSize;
    }
}

SYSLIBFUNC(DWORD) StrCatFormatExW(LPWSTR *lppDest,DWORD dwDestSize,LPCWSTR lpFormat,...)
{
    DWORD dwNewSize=0;
    __try
    {
        if (!dwDestSize)
            dwDestSize=lstrlenW(*lppDest);

        va_list list;
        va_start(list,lpFormat);
        dwNewSize=SYSLIB::wsprintfExW(lppDest,dwDestSize,lpFormat,list);
        va_end(list);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {}
    return dwNewSize;
}

SYSLIBFUNC(DWORD) StrCatFormatExA(LPSTR *lppDest,DWORD dwDestSize,LPCSTR lpFormat,...)
{
    DWORD dwNewSize=0;
    __try
    {
        if (!dwDestSize)
            dwDestSize=lstrlenA(*lppDest);

        va_list list;
        va_start(list,lpFormat);
        dwNewSize=SYSLIB::wsprintfExA(lppDest,dwDestSize,lpFormat,list);
        va_end(list);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {}
    return dwNewSize;
}

SYSLIBFUNC(DWORD) StrFormatExW(LPWSTR *lppDest,LPCWSTR lpFormat,...)
{
    DWORD dwNewSize=0;
    __try
    {
        LPWSTR lpNewBuf=NULL;

        va_list list;
        va_start(list,lpFormat);
        dwNewSize=SYSLIB::wsprintfExW(&lpNewBuf,0,lpFormat,list);
        va_end(list);

        if (dwNewSize)
            *lppDest=lpNewBuf;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {}
    return dwNewSize;
}

SYSLIBFUNC(DWORD) StrFormatExA(LPSTR *lppDest,LPCSTR lpFormat,...)
{
    DWORD dwNewSize=0;
    __try
    {
        LPSTR lpNewBuf=NULL;

        va_list list;
        va_start(list,lpFormat);
        dwNewSize=SYSLIB::wsprintfExA(&lpNewBuf,0,lpFormat,list);
        va_end(list);

        if (dwNewSize)
            *lppDest=lpNewBuf;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {}
    return dwNewSize;
}

SYSLIBFUNC(int) StrNumberFormatA(LONGLONG llNumber,LPSTR lpFormatted,DWORD dwFormattedLen,LPSTR lpSep)
{
    char szData[MAX_PATH];
    wsprintfA(szData,"%I64d",llNumber);

    NUMBERFMTA nfmt={0};
    nfmt.Grouping=3;
    nfmt.lpDecimalSep=lpSep;
    nfmt.lpThousandSep=lpSep;
    return GetNumberFormatA(0,0,szData,&nfmt,lpFormatted,dwFormattedLen);
}

SYSLIBFUNC(int) StrNumberFormatW(LONGLONG llNumber,LPWSTR lpFormatted,DWORD dwFormattedLen,LPWSTR lpSep)
{
    WCHAR szData[MAX_PATH];
    wsprintfW(szData,L"%I64d",llNumber);

    NUMBERFMTW nfmt={0};
    nfmt.Grouping=3;
    nfmt.lpDecimalSep=lpSep;
    nfmt.lpThousandSep=lpSep;
    return GetNumberFormatW(0,0,szData,&nfmt,lpFormatted,dwFormattedLen);
}

