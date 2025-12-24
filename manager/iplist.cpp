#include "includes.h"
#include <stdio.h>

#include "iplist.h"

static HANDLE hFile=INVALID_HANDLE_VALUE,hMapping=NULL;
static LPVOID lpMap=NULL;
static LARGE_INTEGER liOffset={0},liFileSize={0};
static DWORD dwFilePortion,dwItemsAdded;
static  char szLastIP[100];
static bool bLastPart=false,bEndsWithNewLine=true;
static LPCSTR lpListBegin=NULL,lpListEnd=NULL;

static bool bReadingList;
bool IPList_IsReading()
{
    return bReadingList;
}

static DWORD GetStringSizeA(LPCSTR lpSource,LPCSTR lpStrEnd,LPCSTR *lppNewPosition)
{
    LPCSTR lpCurEnd=lpSource;
    DWORD dwSize=0;

    while ((lpCurEnd < lpStrEnd) && (*lpCurEnd != '\n') && (*lpCurEnd != '\r'))
        lpCurEnd++;

    dwSize=(DWORD)(lpCurEnd-lpSource);

    if (((lpCurEnd+1) < lpStrEnd) && (lpCurEnd[0] == '\r') && (lpCurEnd[1] == '\n'))
        lpCurEnd++;

    if (lpCurEnd < lpStrEnd)
        lpCurEnd++;

    *lppNewPosition= lpCurEnd;
    return dwSize;
}

static void IPList_AddIP(LPCSTR lpIP)
{
    do
    {
        char szIP[100];
        WORD wPort=5900;
        sscanf(lpIP,"%[^:]:%d",szIP,&wPort);

        sockaddr_in sa;
        if (inet_pton(AF_INET,szIP,&sa.sin_addr) != 1)
            break;

        if ((wPort != 5900) || (StrChrA(lpIP,':')))
            wsprintfA(szIP,"%s:%d",NetNtoA(sa.sin_addr.s_addr),wPort);
        else
            wsprintfA(szIP,"%s",NetNtoA(sa.sin_addr.s_addr));

        if (lstrcmpA(lpIP,szIP))
            break;

        if (Worker_FindServer(lpVoidConnection,sa.sin_addr.s_addr,wPort,true))
            break;

        DWORD dwIdx=0;
        if (DB_Worker_IsGoodAddressPresent(lpIP,&dwIdx))
            break;

        if (!DB_TotalIPs_IsPresent(sa.sin_addr.s_addr,wPort))
            DB_TotalIPs_Append(sa.sin_addr.s_addr,wPort);

        DB_Worker_Rescan(lpIP,dwIdx);
        dwItemsAdded++;
    }
    while (false);
    return;
}

static bool IPList_GetNextIPInt()
{
    bool bRet=false;

    DWORD dwBytesToSkip=StrSpnA(lpListBegin,"\r\n");
    lpListBegin+=dwBytesToSkip;

    do
    {
        if (lpListBegin >= lpListEnd)
            break;

        LPCSTR lpStrEnd;
        DWORD dwLen=GetStringSizeA(lpListBegin,lpListEnd,&lpStrEnd);
        if (!dwLen)
            break;

        if ((lpStrEnd == lpListEnd) && ((*(lpStrEnd-1) != '\r') && (*(lpStrEnd-1) != '\n')) && (!bLastPart))
            break;

        char szIP[100];
        memcpy(szIP,lpListBegin,dwLen);
        szIP[dwLen]=0;

        IPList_AddIP(szIP);

        lpListBegin=lpStrEnd;

        bRet=true;
    }
    while (false);
    return bRet;
}

static LPVOID IPList_RemapFileMapping()
{
    if (liOffset.QuadPart+dwFilePortion >= liFileSize.QuadPart)
    {
        dwFilePortion=(DWORD)(liFileSize.QuadPart-liOffset.QuadPart);
        bLastPart=true;
    }

    lpMap=MapViewOfFile(hMapping,FILE_MAP_READ,liOffset.HighPart,liOffset.LowPart,dwFilePortion);
    if (lpMap)
    {
        lpListBegin=(LPCSTR)lpMap;
        lpListEnd=lpListBegin+dwFilePortion;
    }
    return lpMap;
}

static bool IPList_GetFileMapping()
{
    bool bRet=false;
    do
    {
        if (!lpMap)
        {
            if (!IPList_RemapFileMapping())
                break;

            bRet=true;
        }

        if (lpListBegin < lpListEnd)
        {
            bRet=true;
            break;
        }

        UnmapViewOfFile(lpMap);
        lpMap=NULL;
        lpListBegin=lpListEnd=NULL;

        if (liOffset.QuadPart < liFileSize.QuadPart)
        {
            liOffset.QuadPart+=dwFilePortion;

            if (!IPList_RemapFileMapping())
                break;

            bRet=true;
        }
    }
    while (false);
    return bRet;
}

bool IPList_GetNextIP()
{
    bool bRet=false;
    do
    {
        if (!IPList_GetFileMapping())
            break;

        if ((lpListBegin == (LPCSTR)lpMap) && (!bEndsWithNewLine))
        {
            if ((*lpListBegin != '\r') && (*lpListBegin != '\n'))
            {
                DWORD dwLastIPLen=lstrlenA(szLastIP);
                do
                {
                    szLastIP[dwLastIPLen]=*lpListBegin;
                    dwLastIPLen++;
                    lpListBegin++;
                }
                while ((lpListBegin < lpListEnd) && (*lpListBegin != '\r') && (*lpListBegin != '\n'));
                szLastIP[dwLastIPLen]=0;
            }

            IPList_AddIP(szLastIP);
            bEndsWithNewLine=true;
            bRet=true;
            break;
        }

        if (IPList_GetNextIPInt())
        {
            bRet=true;
            break;
        }

        lpListEnd--;
        if ((*lpListEnd != '\r') && (*lpListEnd != '\n') && (!bLastPart))
        {
            DWORD dwLastIPLen=1;
            while ((LPCSTR)lpMap < lpListEnd)
            {
                if ((*(lpListEnd-1) == '\r') || (*(lpListEnd-1) == '\n'))
                    break;

                dwLastIPLen++;
                lpListEnd--;
            }

            memcpy(szLastIP,lpListEnd,dwLastIPLen);
            szLastIP[dwLastIPLen]=0;

            bEndsWithNewLine=false;
        }
        else
            bEndsWithNewLine=true;

        bRet=IPList_GetNextIP();
    }
    while (false);
    return bRet;
}

void IPList_Parse(LPCTSTR lpFileName)
{
    do
    {
        hFile=CreateFile(lpFileName,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            break;

        if ((!GetFileSizeEx(hFile,&liFileSize)) || (!liFileSize.QuadPart))
            break;

        hMapping=CreateFileMapping(hFile,NULL,PAGE_READONLY,NULL,NULL,NULL);
        if (!hMapping)
            break;

        szLastIP[0]=0;
        dwFilePortion=GET_MAPPING_SIZE();
        dwItemsAdded=0;

        bReadingList=true;
        bLastPart=false;
        bEndsWithNewLine=true;

        while (WaitForSingleObject(hShutdownEvent,0) == WAIT_TIMEOUT)
        {
            if (!IPList_GetNextIP())
                break;
        }

        bReadingList=false;
        char szDataFormatted[MAX_PATH];
        StrNumberFormatA(dwItemsAdded,szDataFormatted,ARRAYSIZE(szDataFormatted),",");

        DebugLog_AddItem2(NULL,LOG_LEVEL_INFO,"New items added: %s",szDataFormatted);
    }
    while (false);

    if (lpMap)
    {
        UnmapViewOfFile(lpMap);
        lpMap=NULL;
    }

    lpListBegin=lpListEnd=NULL;
    liOffset.QuadPart=0;

    if (hMapping)
    {
        CloseHandle(hMapping);
        hMapping=NULL;
    }

    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
        hFile=INVALID_HANDLE_VALUE;
    }
    return;
}

