#include "includes.h"
#include <syslib\utils.h>

namespace SYSLIB
{
    bool IsDotsNameA(LPSTR lpName);
}

static PMASSCAN_ITEM lpItems=NULL,lpLastItem=NULL,lpLastFreeItem=NULL;
static CRITICAL_SECTION csMasscan;
volatile DWORD dwCheckersCount=0,dwCheckersTasks=0,dwItemsToCheck=0,
               dwChecked=0,dwRFB=0,dwNotRFB=0,dwUnavailble=0,dwCheckerItemsUsed=0;

static volatile DWORD dwMaxItemsToCheck,dwItemsToCheckAllocated;
static sqlite3_stmt *hReadAllStmt,*hReadNewStmt;

static bool bProcessingMasscan,bReadNewItems;
bool Checker_IsReadingNew()
{
    return ((bReadNewItems) || (bProcessingMasscan));
}

static bool bPurging;
bool Checker_IsPurging()
{
    return bPurging;
}

static bool DB_Masscan_ReadInt(sqlite3_stmt *hStmt,PLONGLONG lpllLastId,LPDWORD lpdwIP,LPWORD lpwPort)
{
    bool bRet=false;
    do
    {
        int iErr=sqlite3_step_ex(hStmt);
        if (iErr != SQLITE_ROW)
        {
            if (iErr != SQLITE_DONE)
                DB_LogErr(NULL,hCheckerDB,LOG_LEVEL_ERROR,"sqlite3_step");

            sqlite3_reset(hStmt);

            if (!lpllLastId)
                break;

            sqlite3_bind_int64(hStmt,1,0);
            *lpllLastId=0;
            break;
        }

        if (lpllLastId)
            *lpllLastId=sqlite3_column_int64(hStmt,0);

        if (lpdwIP)
            *lpdwIP=NetResolveAddress((LPCSTR)sqlite3_column_text(hStmt,1));

        if (lpwPort)
            *lpwPort=sqlite3_column_int(hStmt,2);

        bRet=true;
    }
    while (false);
    return bRet;
}

static cJSON *jsLastId;
void Checker_UpdateLastId(LONGLONG llNewValue)
{
    cJSON_SetNumberValue(jsLastId,llNewValue);
    return;
}

static void Checker_CheckPosition()
{
    if (InterlockedExchangeAdd(&dwItemsToCheck,0) < InterlockedExchangeAdd(&dwCheckerItemsUsed,0))
        InterlockedExchange(&dwCheckerItemsUsed,0);

    return;
}

static bool DB_Masscan_Read(LPDWORD lpdwIP,LPWORD lpwPort)
{
    bool bRet=false;
    do
    {
        if (Checker_IsPurging())
            break;

        if (bReadNewItems)
        {
            if (DB_Masscan_ReadInt(hReadNewStmt,NULL,lpdwIP,lpwPort))
            {
                bRet=true;
                break;
            }

            bReadNewItems=false;
        }

        Checker_CheckPosition();

        LONGLONG llLastId;
        if (DB_Masscan_ReadInt(hReadAllStmt,&llLastId,lpdwIP,lpwPort))
        {
            Checker_UpdateLastId(llLastId);
            bRet=true;
            break;
        }

        if (!DB_Masscan_ReadInt(hReadAllStmt,&llLastId,lpdwIP,lpwPort))
            break;

        Checker_UpdateLastId(llLastId);
        bRet=true;
    }
    while (false);
    return bRet;
}

static sqlite3_stmt *DB_Masscan_CreateHandle()
{
    sqlite3_stmt *hStmt=NULL;
    int iErr=sqlite3_prepare_v2(hCheckerDB,"SELECT id,ip,port FROM list WHERE id > ?",-1,&hStmt,0);
    if (iErr != SQLITE_OK)
        DB_LogErr(NULL,hCheckerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    return hStmt;
}

static void DB_Masscan_ReadNext(PMASSCAN_ITEM lpItem)
{
    do
    {
        if (!lpItem)
            break;

        if (!DB_Masscan_Read(&lpItem->Address.dwIP,&lpItem->Address.wPort))
            break;

        lpItem->bInit=true;

        List_RemoveItem(lpItem,(PCOMMON_LIST*)&lpItems,(PCOMMON_LIST*)&lpLastItem);
        List_InsertItem(lpItem,(PCOMMON_LIST*)&lpItems,(PCOMMON_LIST*)&lpLastItem);
    }
    while (false);
    return;
}

extern bool bStopping;
static void Checker_FreeAddressInt(PMASSCAN_ITEM lpItem)
{
    do
    {
        PCONNECTION lpConnection=lpItem->Assigned.lpAssignedTo;
        if (!lpConnection)
            break;

        List_UnAssign(lpItem);

        if (bStopping)
            break;

        lpItem->bInit=false;
        DB_Masscan_ReadNext(lpItem);
    }
    while (false);
    return;
}

void Checker_FreeConnection(PCONNECTION lpConnection)
{
    EnterCriticalSection(&csMasscan);
    {
        while (lpConnection->lpAssigned)
            Checker_FreeAddressInt((PMASSCAN_ITEM)lpConnection->lpAssigned);
    }
    LeaveCriticalSection(&csMasscan);
    return;
}

static PMASSCAN_ITEM Checker_Find(PCONNECTION lpConnection,DWORD dwIP,WORD wPort)
{
    if ((!dwIP) || (!wPort))
        return NULL;

    PMASSCAN_ITEM lpItem=(PMASSCAN_ITEM)lpConnection->lpAssigned;
    while (lpItem)
    {
        if ((lpItem->Address.dwIP == dwIP) && (lpItem->Address.wPort == wPort))
            break;

        lpItem=(PMASSCAN_ITEM)lpItem->Assigned.lpNext;
    }

    if (!lpItem)
    {
        DebugLog_AddItem2(lpConnection,LOG_LEVEL_WARNING,"Checker_FindAssigned(\"%s\",%d) failed",NetNtoA(dwIP),wPort);

        lpItem=lpItems;
        while (lpItem)
        {
            if ((lpItem->Address.dwIP == dwIP) && (lpItem->Address.wPort == wPort))
                break;

            lpItem=(PMASSCAN_ITEM)lpItem->lpNext;
        }

        if (!lpItem)
            DebugLog_AddItem2(lpConnection,LOG_LEVEL_WARNING,"Checker_FindGlobal(\"%s\",%d) failed",NetNtoA(dwIP),wPort);
    }
    return lpItem;
}

void Checker_FreeAddress(PCONNECTION lpConnection,DWORD dwIP,WORD wPort)
{
    EnterCriticalSection(&csMasscan);
    {
        PMASSCAN_ITEM lpItem=Checker_Find(lpConnection,dwIP,wPort);
        if (lpItem)
            Checker_FreeAddressInt(lpItem);
    }
    LeaveCriticalSection(&csMasscan);
    return;
}

void Checker_RemoveAddress(PCONNECTION lpConnection,DWORD dwIP,WORD wPort)
{
    if (Checker_IsPurging())
        return;

    EnterCriticalSection(&csMasscan);
    {
        PMASSCAN_ITEM lpItem=Checker_Find(lpConnection,dwIP,wPort);
        if (lpItem)
        {
            if (lpItem == lpLastFreeItem)
            {
                if (lpItem->lpNext)
                    lpLastFreeItem=(PMASSCAN_ITEM)lpItem->lpNext;
                else
                    lpLastFreeItem=(PMASSCAN_ITEM)lpItem->lpPrev;
            }

            Checker_FreeAddressInt(lpItem);

            InterlockedDecrement(&dwItemsToCheck);
            InterlockedDecrement(&dwCheckerItemsUsed);
        }
    }
    LeaveCriticalSection(&csMasscan);
    return;
}

static bool Masscan_ReadFile(LPCSTR lpFileName,LONGLONG *lpllShift)
{
    bool bMalformed=false,bRet=false;
    HANDLE hFile=CreateFileA(lpFileName,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER liFileSize={0};
        GetFileSizeEx(hFile,&liFileSize);

        if ((liFileSize.QuadPart) && (!(liFileSize.QuadPart % sizeof(MASSCAN))))
        {
            sqlite3_stmt *hStmt=DB_Masscan_Append_Begin();
            if (hStmt)
            {
                HANDLE hMapping=CreateFileMapping(hFile,NULL,PAGE_READWRITE,0,0,NULL);
                if (hMapping)
                {
                    PMASSCAN lpNewItems=(PMASSCAN)MapViewOfFile(hMapping,FILE_MAP_READ,0,0,0);
                    if (lpNewItems)
                    {
                        DWORD dwItems=(DWORD)(liFileSize.QuadPart/sizeof(MASSCAN));

                        DWORD dwAdded=0;
                        while (liFileSize.QuadPart)
                        {
                            if (WaitForSingleObject(hShutdownEvent,0) == WAIT_OBJECT_0)
                                break;

                            dwItems--;
                            liFileSize.QuadPart-=sizeof(MASSCAN);

                            if (DB_TotalIPs_IsPresent(lpNewItems[dwItems].dwIP,lpNewItems[dwItems].wPort))
                                continue;

                            if (!DB_Masscan_Append(hStmt,lpNewItems[dwItems].dwIP,lpNewItems[dwItems].wPort))
                                continue;

                            dwAdded++;

                            if (InterlockedExchangeAdd(&dwItemsToCheck,0) < dwMaxItemsToCheck)
                            {
                                EnterCriticalSection(&csMasscan);
                                {
                                    PMASSCAN_ITEM lpItem=lpItems;
                                    while (lpItem)
                                    {
                                        if ((lpItem->bInit) || (lpItem->bFreshAdded))
                                        {
                                            lpItem=(PMASSCAN_ITEM)lpItem->lpNext;
                                            continue;
                                        }

                                        lpItem->Address.dwIP=lpNewItems[dwItems].dwIP;
                                        lpItem->Address.wPort=lpNewItems[dwItems].wPort;
                                        lpItem->bFreshAdded=true;

                                        (*lpllShift)++;
                                        break;
                                    }
                                }
                                LeaveCriticalSection(&csMasscan);
                            }

                            InterlockedIncrement(&dwItemsToCheck);
                        }
                        UnmapViewOfFile(lpNewItems);

                        char szDataFormatted[MAX_PATH];
                        StrNumberFormatA(dwAdded,szDataFormatted,ARRAYSIZE(szDataFormatted),",");

                        DebugLog_AddItem2(NULL,LOG_LEVEL_INFO,"New items added: %s",szDataFormatted);

                        bRet=(dwAdded != 0);
                    }
                    CloseHandle(hMapping);

                    SetFilePointerEx(hFile,liFileSize,NULL,FILE_BEGIN);
                    SetEndOfFile(hFile);
                }
                DB_Masscan_Append_End(hStmt);
            }
        }
        else
            bMalformed=true;

        CloseHandle(hFile);

        if (!liFileSize.QuadPart)
            DeleteFileA(lpFileName);
        else if (bMalformed)
        {
            char szNewFile[MAX_PATH];
            wsprintfA(szNewFile,"%s-malformed",lpFileName);
            if ((!MoveFileA(lpFileName,szNewFile)) && (GetLastError() == ERROR_FILE_EXISTS))
            {
                while (true)
                {
                    wsprintfA(szNewFile,"%s-malformed.%x",lpFileName,GetRndDWORD);
                    if (MoveFileA(lpFileName,szNewFile))
                        break;

                    if (GetLastError() != ERROR_FILE_EXISTS)
                        break;
                }
            }
        }
    }
    return bRet;
}

static void Masscan_ReadFiles(LPCSTR lpPath)
{
    char szDir[MAX_PATH];
    wsprintfA(szDir,"%s\\*.masscan",lpPath);

    do
    {
        if (Checker_IsPurging())
            break;

        bool bReaded=false;
        LONGLONG llMaxId=0;
        WIN32_FIND_DATAA wfd;
        HANDLE hFind=FindFirstFileA(szDir,&wfd);
        DWORD dwPrevItemsToCheck=InterlockedExchangeAdd(&dwItemsToCheck,0);
        if (hFind == INVALID_HANDLE_VALUE)
            break;

        do
        {
            if (WaitForSingleObject(hShutdownEvent,0) == WAIT_OBJECT_0)
                break;

            if (!llMaxId)
            {
                if (dwPrevItemsToCheck)
                    llMaxId=DB_Masscan_GetLastItemId();
            }

            bProcessingMasscan=true;

            char szFileName[MAX_PATH];
            wsprintfA(szFileName,"%s\\%s",lpPath,wfd.cFileName);
            if (Masscan_ReadFile(szFileName,&llMaxId))
                bReaded=true;
        }
        while (FindNextFileA(hFind,&wfd));

        FindClose(hFind);

        if (!bReaded)
            break;

        EnterCriticalSection(&csMasscan);
        {
            if (!bReadNewItems)
            {
                sqlite3_reset(hReadNewStmt);
                sqlite3_bind_int64(hReadNewStmt,1,llMaxId);

                bReadNewItems=true;
            }

            if (dwPrevItemsToCheck < dwMaxItemsToCheck)
            {
                PMASSCAN_ITEM lpItem=lpItems;
                while (lpItem)
                {
                    if (lpItem->bFreshAdded)
                    {
                        lpItem->bFreshAdded=false;
                        lpItem->bInit=true;
                    }

                    lpItem=(PMASSCAN_ITEM)lpItem->lpNext;
                }
            }
        }
        LeaveCriticalSection(&csMasscan);
    }
    while (false);

    bProcessingMasscan=false;
    return;
}

static void Checker_ScanInLoop(LPCSTR lpPath)
{
    while (WaitForSingleObject(hShutdownEvent,MILLISECONDS_PER_SECOND) == WAIT_TIMEOUT)
        Masscan_ReadFiles(lpPath);

    return;
}

static void WINAPI Checker_ReadThread(cJSON *jsConfig)
{
    do
    {
        LPCSTR lpPath=cJSON_GetStringFromObject(jsConfig,"path");
        if (!lpPath)
            break;

        if (!(GetFileAttributesA(lpPath) & FILE_ATTRIBUTE_DIRECTORY))
            break;

        Masscan_ReadFiles(lpPath);

        HANDLE hDir=CreateFileA(lpPath,FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                                NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,NULL);
        if (hDir == INVALID_HANDLE_VALUE)
            break;

        OVERLAPPED ovl={0};
        ovl.hEvent=CreateEvent(NULL,false,false,NULL);

        byte bBuf[1024];
        if (!ReadDirectoryChangesW(hDir,bBuf,sizeof(bBuf),false,FILE_WATCH_FLAGS,NULL,&ovl,NULL))
        {
            CloseHandle(ovl.hEvent);
            CloseHandle(hDir);

            Checker_ScanInLoop(lpPath);
            break;
        }

        HANDLE hEvents[]={ovl.hEvent,hShutdownEvent};
        while (WaitForMultipleObjects(ARRAYSIZE(hEvents),hEvents,false,INFINITE) == WAIT_OBJECT_0)
        {
            Masscan_ReadFiles(lpPath);
            ReadDirectoryChangesW(hDir,bBuf,sizeof(bBuf),false,FILE_WATCH_FLAGS,NULL,&ovl,NULL);
        }

        CloseHandle(ovl.hEvent);
        CloseHandle(hDir);
    }
    while (false);
    return;
}

static PMASSCAN_ITEM Checker_FindFree(PMASSCAN_ITEM lpPosition)
{
    while (lpPosition)
    {
        if ((!lpPosition->Assigned.lpAssignedTo) && (lpPosition->bInit))
            break;

        lpPosition=(PMASSCAN_ITEM)lpPosition->lpNext;
    }
    return lpPosition;
}

void Checker_NullLastFreeItem()
{
    EnterCriticalSection(&csMasscan);
        lpLastFreeItem=NULL;
    LeaveCriticalSection(&csMasscan);
    return;
}

bool bChecker_NewAddedItemsUsed=false,bChecker_UpdatePercents=false;
bool Checker_GetNextAddress(PCONNECTION lpConnection,PMASSCAN lpItem)
{
    if (Checker_IsPurging())
        return false;

    bool bRet=false;
    EnterCriticalSection(&csMasscan);
    {
        PMASSCAN_ITEM lpFreeItem=NULL;
        do
        {
            if (!bReadNewItems)
            {
                if (bChecker_NewAddedItemsUsed)
                {
                    bChecker_NewAddedItemsUsed=false;
                    bChecker_UpdatePercents=true;
                }
            }
            else
            {
                bChecker_NewAddedItemsUsed=true;
                bChecker_UpdatePercents=true;
            }

            lpLastFreeItem=lpFreeItem=Checker_FindFree(lpLastFreeItem);
            if (lpFreeItem)
                break;

            Checker_CheckPosition();

            lpFreeItem=Checker_FindFree(lpItems);
            if (!lpFreeItem)
                break;

            lpLastFreeItem=lpFreeItem;
        }
        while (false);

        if (lpFreeItem)
        {
            InterlockedIncrement(&dwCheckerItemsUsed);

            lpItem->dwIP=lpFreeItem->Address.dwIP;
            lpItem->wPort=lpFreeItem->Address.wPort;

            List_AssignItem(lpConnection,lpFreeItem);
            bRet=true;
        }
    }
    LeaveCriticalSection(&csMasscan);
    return bRet;
}

static cJSON *jsReadFromLastId;
static bool bReadFromLastId;
void Checker_UpdateMenuState(HMENU hMenu,DWORD dwItem)
{
    switch (dwItem)
    {
        case -1:
        {
            UpdateMenuState(hMenu,IDM_CHECKER_REMEMBER_LAST_ID,bReadFromLastId);
            break;
        }
        case IDM_CHECKER_REMEMBER_LAST_ID:
        {
            bReadFromLastId=!bReadFromLastId;
            UpdateMenuState(hMenu,dwItem,bReadFromLastId);

            cJSON_SetBoolValue(jsReadFromLastId,bReadFromLastId);

            Config_Update();
            break;
        }
    }
    return;
}

static void WINAPI PurgeWaitThread(LPVOID)
{
    while (true)
    {
        if (WaitForSingleObject(hShutdownEvent,100) == WAIT_OBJECT_0)
            break;

        if (InterlockedExchangeAdd(&dwCheckersTasks,0))
            continue;

        PostThreadMessageEx(dwDBUpdateThreadId,IM_DB_MAINTANCE_PURGE_CHECKER_DB_END,NULL,NULL);
        break;
    }
    return;
}

void Checker_PurgeDB_Begin()
{
    bPurging=true;
    EnterCriticalSection(&csMasscan);
    {
        PMASSCAN_ITEM lpItem=lpItems;
        while (lpItem)
        {
            lpItem->bInit=false;
            lpItem=(PMASSCAN_ITEM)lpItem->lpNext;
        }
    }
    LeaveCriticalSection(&csMasscan);

    ThreadsGroup_CreateThread(hDBThreadsGroup,0,(LPTHREAD_START_ROUTINE)PurgeWaitThread,NULL,NULL,NULL);
    return;
}

bool Checker_PurgeDB_End()
{
    EnterCriticalSection(&csMasscan);
    {
        sqlite3_finalize(hReadAllStmt);
        sqlite3_finalize(hReadNewStmt);

        bool bOk=DB_Masscan_Purge();

        hReadAllStmt=DB_Masscan_CreateHandle();
        hReadNewStmt=DB_Masscan_CreateHandle();

        sqlite3_bind_int64(hReadAllStmt,1,0);

        if (bOk)
        {
            lpLastFreeItem=NULL;

            InterlockedExchange(&dwItemsToCheck,0);
            InterlockedExchange(&dwChecked,0);
            InterlockedExchange(&dwNotRFB,0);
            InterlockedExchange(&dwRFB,0);
            InterlockedExchange(&dwUnavailble,0);
            InterlockedExchange(&dwCheckerItemsUsed,0);

            if (!bReadFromLastId)
                Checker_UpdateLastId(0);
        }
    }
    LeaveCriticalSection(&csMasscan);

    bPurging=false;
    return true;
}

void Checker_Init(cJSON *jsConfig)
{
    do
    {
        Splash_SetText(_T("Initializing checkers..."));

        InitializeCriticalSection(&csMasscan);
        Connection_AddRoot(TYPE_CHECKER);

        dwItemsToCheck=DB_Masscan_CalcItemsToCheck();

        hReadAllStmt=DB_Masscan_CreateHandle();
        hReadNewStmt=DB_Masscan_CreateHandle();

        cJSON *jsCheckerCfg=cJSON_GetObjectItem(jsConfig,"checker");
        if (!jsCheckerCfg)
            break;

        dwMaxItemsToCheck=cJSON_GetIntFromObject(jsCheckerCfg,"max_items");
        if (!dwMaxItemsToCheck)
            dwMaxItemsToCheck=100000;

        jsReadFromLastId=cJSON_GetObjectItem(jsCheckerCfg,"read_from_last_id");
        if (!jsReadFromLastId)
        {
            jsReadFromLastId=cJSON_CreateFalse();
            cJSON_AddItemToObject(jsCheckerCfg,"read_from_last_id",jsReadFromLastId);
        }

        bReadFromLastId=cJSON_IsTrue(jsReadFromLastId);

        LONGLONG llLastId=0;
        jsLastId=cJSON_GetObjectItem(jsCheckerCfg,"last_id");
        if (!jsLastId)
        {
            jsLastId=cJSON_CreateNumber(0);
            cJSON_AddItemToObject(jsCheckerCfg,"last_id",jsLastId);
        }

        if (!bReadFromLastId)
            Checker_UpdateLastId(0);
        else
        {
            llLastId=cJSON_GetNumberValue(jsLastId);

            if (llLastId >= dwMaxItemsToCheck)
                llLastId-=dwMaxItemsToCheck;
            else
                llLastId=0;
        }

        sqlite3_bind_int64(hReadAllStmt,1,llLastId);

        bool bReadDone=false;
        for (dwItemsToCheckAllocated=0; dwItemsToCheckAllocated < dwMaxItemsToCheck; dwItemsToCheckAllocated++)
        {
            PMASSCAN_ITEM lpItem=(PMASSCAN_ITEM)MemAlloc(sizeof(*lpItems));
            if (!lpItem)
                break;

            List_InsertItem(lpItem,(PCOMMON_LIST*)&lpItems,(PCOMMON_LIST*)&lpLastItem);
            InterlockedIncrement(&dwItemsToCheckAllocated);

            if (bReadDone)
                continue;

            DWORD dwIP;
            WORD wPort;
            if (!DB_Masscan_Read(&dwIP,&wPort))
            {
                bReadDone=true;
                continue;
            }

            lpItem->Address.dwIP=dwIP;
            lpItem->Address.wPort=wPort;
            lpItem->bInit=true;

            InterlockedIncrement(&dwItemsToCheck);
        }

        ThreadsGroup_CreateThread(hThreadsGroup,0,(LPTHREAD_START_ROUTINE)Checker_ReadThread,jsCheckerCfg,0,NULL);

        cJSON *jsServer=cJSON_GetObjectItem(jsConfig,"server");
        if (!jsServer)
            break;

        cJSON *jsChecker=cJSON_GetObjectItem(jsServer,"checker");
        if (!jsChecker)
            break;

        cJSON *jsCheckers=cJSON_GetObjectItem(jsChecker,"items");
        if (!jsCheckers)
            break;

        DWORD dwCount=cJSON_GetArraySize(jsCheckers);
        if (!dwCount)
            break;

        for (int i=0; i < dwCount; i++)
        {
            cJSON *jsItem=cJSON_GetArrayItem(jsCheckers,i);
            if (!jsItem)
                break;

            Connection_Add(jsItem,
                           cJSON_GetStringFromObject(jsItem,"address"),
                           cJSON_GetStringFromObject(jsItem,"alias"),
                           TYPE_CHECKER);
        }
    }
    while (false);
    return;
}

