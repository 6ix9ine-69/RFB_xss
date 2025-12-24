#include "sys_includes.h"

#include "syslib\mem.h"
#include "syslib\str.h"

static DWORD UnicodeToX(DWORD codePage,LPCWSTR source,DWORD sourceSize,LPSTR dest,DWORD destSize,DWORD dwFlags=0)
{
    if (sourceSize == 0)
        sourceSize=lstrlenW(source);
    DWORD size=WideCharToMultiByte(codePage,dwFlags,source,sourceSize,dest,destSize,NULL,NULL);
    if (destSize != 0)
    {
        if (size > destSize)
            size=0;
        dest[size]=0;
    }
    return size;
}

static DWORD xToUnicode(DWORD codePage,LPCSTR source,DWORD sourceSize,LPWSTR dest,DWORD destSize,DWORD dwFlags=0)
{
    if (sourceSize == 0)
        sourceSize=lstrlenA(source);
    DWORD size=MultiByteToWideChar(codePage,0,source,sourceSize,dest,destSize);
    if (destSize != 0)
    {
        if (size > destSize)
            size=0;
        dest[size]=0;
    }
    return size;
}

SYSLIBFUNC(DWORD) StrUnicodeToAnsi(LPCWSTR lpSource,DWORD dwSourceSize,LPSTR lpDest,DWORD dwDestSize)
{
    DWORD dwRet=0;
    do
    {
        if (!dwDestSize)
            dwDestSize=lstrlenW(lpSource)+1;

        dwRet=UnicodeToX(1251,lpSource,dwSourceSize,lpDest,dwDestSize,WC_COMPOSITECHECK);
    }
    while (false);
    return dwRet;
}

SYSLIBFUNC(LPSTR) StrUnicodeToAnsiEx(LPCWSTR lpSource,DWORD dwSourceSize,LPDWORD lpOutSize)
{
    LPSTR lpOut=NULL;
    do
    {
        if (!dwSourceSize)
        {
            dwSourceSize=lstrlenW(lpSource);
            if (!dwSourceSize)
                break;
        }

        DWORD dwRequedSize=UnicodeToX(1251,lpSource,dwSourceSize,NULL,0,WC_COMPOSITECHECK);
        if (!dwRequedSize)
            break;

        lpOut=(LPSTR)MemQuickAlloc(dwRequedSize+1);
        if (!lpOut)
            break;

        DWORD dwOutSize=StrUnicodeToAnsi(lpSource,dwSourceSize,lpOut,dwRequedSize);
        if (dwOutSize)
        {
            if (lpOutSize)
                *lpOutSize=dwOutSize;

            lpOut[dwOutSize]=0;
            break;
        }

        MemFree(lpOut);
        lpOut=NULL;
    }
    while (false);
    return lpOut;
}

SYSLIBFUNC(DWORD) StrAnsiToUnicode(LPCSTR lpSource,DWORD dwSourceSize,LPWSTR lpDest,DWORD dwDestSize)
{
    DWORD dwRet=0;
    do
    {
        if (!dwDestSize)
            dwDestSize=lstrlenA(lpSource)+1;

        dwRet=xToUnicode(1251,lpSource,dwSourceSize,lpDest,dwDestSize);
    }
    while (false);
    return dwRet;
}

SYSLIBFUNC(LPWSTR) StrAnsiToUnicodeEx(LPCSTR lpSource,DWORD dwSourceSize,LPDWORD lpOutSize)
{
    LPWSTR lpOut=NULL;
    do
    {
        if (!dwSourceSize)
        {
            dwSourceSize=lstrlenA(lpSource);
            if (!dwSourceSize)
                break;
        }

        DWORD dwRequedSize=xToUnicode(1251,lpSource,dwSourceSize,NULL,0);
        if (!dwRequedSize)
            break;

        lpOut=WCHAR_QuickAlloc(dwRequedSize+1);
        if (!lpOut)
            break;

        DWORD dwOutSize=StrAnsiToUnicode(lpSource,dwSourceSize,lpOut,dwRequedSize);
        if (dwOutSize)
        {
            if (lpOutSize)
                *lpOutSize=dwOutSize;

            lpOut[dwOutSize]=0;
            break;
        }

        MemFree(lpOut);
        lpOut=NULL;
    }
    while (false);
    return lpOut;
}

SYSLIBFUNC(DWORD) StrUnicodeToOem(LPCWSTR lpSource,DWORD dwSourceSize,LPSTR lpDest,DWORD dwDestSize)
{
    DWORD dwRet=0;
    do
    {
        if (!dwDestSize)
            dwDestSize=lstrlenW(lpSource)+1;

        dwRet=UnicodeToX(CP_OEMCP,lpSource,dwSourceSize,lpDest,dwDestSize,WC_COMPOSITECHECK);
    }
    while (false);
    return dwRet;
}

SYSLIBFUNC(LPSTR) StrUnicodeToOemEx(LPCWSTR lpSource,DWORD dwSourceSize,LPDWORD lpOutSize)
{
    LPSTR lpOut=NULL;
    do
    {
        if (!dwSourceSize)
        {
            dwSourceSize=lstrlenW(lpSource);
            if (!dwSourceSize)
                break;
        }

        DWORD dwRequedSize=UnicodeToX(CP_OEMCP,lpSource,dwSourceSize,NULL,0,WC_COMPOSITECHECK);
        if (!dwRequedSize)
            break;

        lpOut=(LPSTR)MemQuickAlloc(dwRequedSize+1);
        if (!lpOut)
            break;

        DWORD dwOutSize=StrUnicodeToOem(lpSource,dwSourceSize,lpOut,dwRequedSize);
        if (dwOutSize)
        {
            if (lpOutSize)
                *lpOutSize=dwOutSize;

            lpOut[dwOutSize]=0;
            break;
        }

        MemFree(lpOut);
        lpOut=NULL;
    }
    while (false);
    return lpOut;
}

SYSLIBFUNC(DWORD) StrOemToUnicode(LPCSTR lpSource,DWORD dwSourceSize,LPWSTR lpDest,DWORD dwDestSize)
{
    DWORD dwRet=0;
    do
    {
        if (!dwDestSize)
            dwDestSize=lstrlenA(lpSource)+1;

        dwRet=xToUnicode(CP_OEMCP,lpSource,dwSourceSize,lpDest,dwDestSize);
    }
    while (false);
    return dwRet;
}

SYSLIBFUNC(LPWSTR) StrOemToUnicodeEx(LPCSTR lpSource,DWORD dwSourceSize,LPDWORD lpOutSize)
{
    LPWSTR lpOut=NULL;
    do
    {
        if (!dwSourceSize)
        {
            dwSourceSize=lstrlenA(lpSource);
            if (!dwSourceSize)
                break;
        }

        DWORD dwRequedSize=xToUnicode(CP_OEMCP,lpSource,dwSourceSize,NULL,0);
        if (!dwRequedSize)
            break;

        lpOut=WCHAR_QuickAlloc(dwRequedSize+1);
        if (!lpOut)
            break;

        DWORD dwOutSize=StrOemToUnicode(lpSource,dwSourceSize,lpOut,dwRequedSize);
        if (dwOutSize)
        {
            if (lpOutSize)
                *lpOutSize=dwOutSize;

            lpOut[dwOutSize]=0;
            break;
        }

        MemFree(lpOut);
        lpOut=NULL;
    }
    while (false);
    return lpOut;
}

static byte GetHexValueW(WCHAR wChr)
{
    byte bRet=0xFF;

    if ((wChr >= L'0') && (wChr <= L'9' ))
        bRet=(wChr-L'0');
    else if ((wChr >= L'a') && (wChr <= L'f'))
        bRet=(wChr-L'a')+0xA;
    else if ((wChr >= L'AA') && (wChr <= L'F'))
        bRet=(wChr-L'A')+0xA;

    return bRet;
}

SYSLIBFUNC(DWORD64) StrToHex64W(LPCWSTR lpStr)
{
	DWORD64 dwHex = 0;
	do
	{
		if ((lpStr[0] == L'0') && ((lpStr[1] == L'x') || (lpStr[1] == L'X')))
			lpStr += 2;

		for (DWORD i = 0; i < 16; i++)
		{
			byte bHex = GetHexValueW(tolower(*lpStr++));
			if (bHex == 0xFF)
				break;

			dwHex = dwHex * 16 + bHex;
		}
	} while (false);
	return dwHex;
}

SYSLIBFUNC(DWORD) StrToHexW(LPCWSTR lpStr)
{
    DWORD dwHex=0;
    do
    {
        if ((lpStr[0] == L'0') && ((lpStr[1] == L'x') || (lpStr[1] == L'X')))
            lpStr+=2;

        for (DWORD i=0; i < 8; i++)
        {
            byte bHex=GetHexValueW(tolower(*lpStr++));
            if (bHex == 0xFF)
                break;

            dwHex=dwHex*16+bHex;
        }
    }
    while (false);
    return dwHex;
}

static byte GetHexValueA(char cChr)
{
    byte bRet=0xFF;

    if ((cChr >= '0') && (cChr <= '9' ))
        bRet=(cChr-'0');
    else if ((cChr >= 'a') && (cChr <= 'f'))
        bRet=(cChr-'a')+0xA;
    else if ((cChr >= 'A') && (cChr <= 'F'))
        bRet=(cChr-'A')+0xA;

    return bRet;
}

SYSLIBFUNC(DWORD64) StrToHex64A(LPCSTR lpStr)
{
	DWORD64 dwHex = 0;
	do
	{
		if ((lpStr[0] == '0') && ((lpStr[1] == 'x') || (lpStr[1] == 'X')))
			lpStr += 2;

		for (DWORD i = 0; i < 16; i++)
		{
			byte bHex = GetHexValueA(tolower(*lpStr++));
			if (bHex == 0xFF)
				break;

			dwHex = dwHex * 16 + bHex;
		}
	} while (false);
	return dwHex;
}

SYSLIBFUNC(DWORD) StrToHexA(LPCSTR lpStr)
{
    DWORD dwHex=0;
    do
    {
        if ((lpStr[0] == '0') && ((lpStr[1] == 'x') || (lpStr[1] == 'X')))
            lpStr+=2;

        for (DWORD i=0; i < 8; i++)
        {
            byte bHex=GetHexValueA(tolower(*lpStr++));
            if (bHex == 0xFF)
                break;

            dwHex=dwHex*16+bHex;
        }
    }
    while (false);
    return dwHex;
}
