#include "sys_includes.h"

#include <syslib\net.h>

static bool UrlEncoderIsGoodCharW(WCHAR wChr)
{
    bool bRet=true;
    if (((wChr < L'0') && (wChr != L'-') && (wChr != L'.')) || ((wChr < L'A') && (wChr > L'9')) || ((wChr > L'Z') && (wChr < L'a') && (wChr != L'_')) || (wChr > L'z'))
        bRet=false;
    return bRet;
}

static void ByteToWChar(byte bByte,LPWSTR lpStr)
{
    lpStr[0]=(BYTE)(bByte >> 4);
    lpStr[1]=(BYTE)(bByte & 0xF);

    lpStr[0]+=(lpStr[0] > 0x9 ? (L'A' - 0xA) : L'0');
    lpStr[1]+=(lpStr[1] > 0x9 ? (L'A' - 0xA) : L'0');
    return;
}

SYSLIBFUNC(BOOL) NetUrlEncodeBufferW(LPCWSTR lpIn,DWORD dwSize,LPWSTR lpOut,DWORD dwOutSize)
{
    BOOL bRet=false;
    do
    {
        if ((!lpIn) || (!lpOut))
            break;

        if (!dwSize)
        {
            dwSize=lstrlenW(lpIn);
            if (!dwSize)
                break;
        }

        DWORD dwRequedSize=NetUrlCalcEncodedSizeW(lpIn,dwSize);
        if (!dwRequedSize)
            break;

        if (dwRequedSize > dwOutSize)
            break;

        DWORD dwStrIdx=0;
        while (dwSize--)
        {
            WCHAR wChr=*lpIn;
            if (!UrlEncoderIsGoodCharW(wChr))
            {
                lpOut[dwStrIdx++]=L'%';
                ByteToWChar(wChr,&lpOut[dwStrIdx]);
                dwStrIdx+=2;
            }
            else
                lpOut[dwStrIdx++]=wChr;

            lpIn++;
        }

        bRet=true;
    }
    while (false);

    return bRet;
}

static bool UrlEncoderIsGoodCharA(char cChr)
{
    bool bRet=true;
    if (((cChr < '0') && (cChr != '-') && (cChr != '.')) || ((cChr < 'A') && (cChr > '9')) || ((cChr > 'Z') && (cChr < 'a') && (cChr != '_')) || (cChr > 'z'))
        bRet=false;
    return bRet;
}

static void ByteToChar(byte bByte,LPSTR lpStr)
{
    lpStr[0]=(BYTE)(bByte >> 4);
    lpStr[1]=(BYTE)(bByte & 0xF);

    lpStr[0]+=(lpStr[0] > 0x9 ? ('A' - 0xA) : '0');
    lpStr[1]+=(lpStr[1] > 0x9 ? ('A' - 0xA) : '0');
    return;
}

SYSLIBFUNC(BOOL) NetUrlEncodeBufferA(LPCSTR lpIn,DWORD dwSize,LPSTR lpOut,DWORD dwOutSize)
{
    BOOL bRet=false;
    do
    {
        if ((!lpIn) || (!lpOut))
            break;

        if (!dwSize)
        {
            dwSize=lstrlenA(lpIn);
            if (!dwSize)
                break;
        }

        DWORD dwRequedSize=NetUrlCalcEncodedSizeA(lpIn,dwSize);
        if (!dwRequedSize)
            break;

        if (dwRequedSize > dwOutSize)
            break;

        DWORD dwStrIdx=0;
        while (dwSize--)
        {
            char cChr=*lpIn;
            if (!UrlEncoderIsGoodCharA(cChr))
            {
                lpOut[dwStrIdx++]='%';
                ByteToChar(cChr,&lpOut[dwStrIdx]);
                dwStrIdx+=2;
            }
            else
                lpOut[dwStrIdx++]=cChr;

            lpIn++;
        }

        bRet=true;
    }
    while (false);

    return bRet;
}

SYSLIBFUNC(DWORD) NetUrlCalcEncodedSizeW(LPCWSTR lpIn,DWORD dwSize)
{
    DWORD dwRequedSize=0;
    do
    {
        if (!lpIn)
            break;

        if (!dwSize)
        {
            dwSize=lstrlenW(lpIn);
            if (!dwSize)
                break;
        }

        dwRequedSize=dwSize;
        while (dwSize--)
        {
            WCHAR wChr=*lpIn++;
            if (!UrlEncoderIsGoodCharW(wChr))
                dwRequedSize+=2;
        }
    }
    while (false);

    return dwRequedSize;
}

SYSLIBFUNC(DWORD) NetUrlCalcEncodedSizeA(LPCSTR lpIn,DWORD dwSize)
{
    DWORD dwRequedSize=0;
    do
    {
        if (!lpIn)
            break;

        if (!dwSize)
        {
            dwSize=lstrlenA(lpIn);
            if (!dwSize)
                break;
        }

        dwRequedSize=dwSize;
        while (dwSize--)
        {
            char cChr=*lpIn++;
            if (!UrlEncoderIsGoodCharA(cChr))
                dwRequedSize+=2;
        }
    }
    while (false);

    return dwRequedSize;
}
