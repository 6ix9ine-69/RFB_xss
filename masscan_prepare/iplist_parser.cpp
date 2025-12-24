#include <windows.h>
#include <shlwapi.h>

#include <syslib\net.h>

#include <pshpack1.h>
    typedef struct _MASSCAN
    {
        DWORD dwIP;
        WORD wPort;
    } MASSCAN, *PMASSCAN;
#include <poppack.h>

extern SYSTEM_INFO siInfo;

#define GET_MAPPING_SIZE() siInfo.dwAllocationGranularity*16

static void IPList_ParseIP(LPSTR lpIP,PMASSCAN lpItem)
{
    char szIP[80];
    WORD wPort;

    LPSTR lpDiv=StrChrA(lpIP,':');
    if (!lpDiv)
    {
        lstrcpyA(szIP,lpIP);
        wPort=5900;
    }
    else
    {
        *lpDiv=0;
        lstrcpyA(szIP,lpIP);
        wPort=StrToIntA(lpDiv+1);
    }

    lpItem->wPort=wPort;
    lpItem->dwIP=inet_addr(szIP);
    return;
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

static HANDLE hTxtFile=INVALID_HANDLE_VALUE,hMapping=NULL;
static LPVOID lpMap=NULL;
static LARGE_INTEGER liOffset={0},liFileSize={0};
static DWORD dwFilePortion;
static  char szLastIP[100]={0};
static bool bLastPart=false,bEndsWithNewLine=true;
static LPCSTR lpListBegin=NULL,lpListEnd=NULL;

static bool IPList_GetNextIPInt(PMASSCAN lpItem)
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
        {
            /**
                это последняя запись и не кончается переводом строки -
                отсавим для следующей итерации
            **/
            break;
        }

        char szIP[100];
        memcpy(szIP,lpListBegin,dwLen);
        szIP[dwLen]=0;

        IPList_ParseIP(szIP,lpItem);

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

void IPList_ReInit()
{
    if (lpMap)
    {
        UnmapViewOfFile(lpMap);
        lpMap=NULL;
        lpListBegin=lpListEnd=NULL;
    }

    liOffset.QuadPart=0;

    dwFilePortion=GET_MAPPING_SIZE();

    bLastPart=false;
    bEndsWithNewLine=true;

    szLastIP[0]=0;

    return;
}

static bool IPList_GetFileMapping()
{
    bool bRet=false;
    do
    {
        if (!lpMap)
        {
            /// первый запуск?
            if (!IPList_RemapFileMapping())
                break;

            bRet=true;
        }

        /// еще есть что читать?
        if (lpListBegin < lpListEnd)
        {
            bRet=true;
            break;
        }

        UnmapViewOfFile(lpMap);
        lpMap=NULL;
        lpListBegin=lpListEnd=NULL;

        /// есть, что мапить?
        if (liOffset.QuadPart < liFileSize.QuadPart)
        {
            liOffset.QuadPart+=dwFilePortion;

            if (!IPList_RemapFileMapping())
                break;

            bRet=true;
        } /// иначе - прочли весь файл, ошибки нет
    }
    while (false);
    return bRet;
}

bool IPList_GetNextIP_Initial(PMASSCAN lpItem)
{
    bool bRet=false;
    do
    {
        if (!IPList_GetFileMapping())
            break;

        if ((lpListBegin == (LPCSTR)lpMap) && (!bEndsWithNewLine))
        {
            /**
                прощлая итерация завершилась не переводом строки,
                возможно есть не завершенный адрес. проверяем
            **/

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

            IPList_ParseIP(szLastIP,lpItem);
            bEndsWithNewLine=true;
            bRet=true;
            break;
        }

        if (IPList_GetNextIPInt(lpItem))
        {
            bRet=true;
            break;
        }

        lpListEnd--;
        if ((*lpListEnd != '\r') && (*lpListEnd != '\n') && (!bLastPart))
        {
            /**
                если нет перевода строки в конце - возможно
                это не завершенный адрес и на следующей итерации
                будет его окончание. откатываемся в его начало
            **/

            DWORD dwLastIPLen=1;
            while ((LPCSTR)lpMap < lpListEnd)
            {
                if ((*(lpListEnd-1) == '\r') || (*(lpListEnd-1) == '\n'))
                    break;

                dwLastIPLen++;
                lpListEnd--;
            }

            /// запоминаем последний адрес
            memcpy(szLastIP,lpListEnd,dwLastIPLen);
            szLastIP[dwLastIPLen]=0;

            bEndsWithNewLine=false;
        }
        else
            bEndsWithNewLine=true;

        bRet=IPList_GetNextIP_Initial(lpItem);
    }
    while (false);
    return bRet;
}

void IPList_Parse(LPCWSTR lpTxtFileName,LPCWSTR lpDatFileName)
{
    do
    {
        hTxtFile=CreateFileW(lpTxtFileName,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
        if (hTxtFile == INVALID_HANDLE_VALUE)
            break;

        if ((!GetFileSizeEx(hTxtFile,&liFileSize)) || (!liFileSize.QuadPart))
            break;

        hMapping=CreateFileMapping(hTxtFile,NULL,PAGE_READONLY,NULL,NULL,NULL);
        if (!hMapping)
            break;

        dwFilePortion=GET_MAPPING_SIZE();

        HANDLE hDatFile=CreateFileW(lpDatFileName,GENERIC_WRITE,0,NULL,OPEN_ALWAYS,0,NULL);
        if (hDatFile  == INVALID_HANDLE_VALUE)
            break;

        LARGE_INTEGER liFileSize={0};
        SetFilePointerEx(hDatFile,liFileSize,NULL,FILE_END);

        while (true)
        {
            MASSCAN Item;
            if (!IPList_GetNextIP_Initial(&Item))
                break;

            DWORD dwTmp;
            WriteFile(hDatFile,&Item,sizeof(Item),&dwTmp,NULL);
        }

        CloseHandle(hDatFile);
    }
    while (false);
    return;
}

