#include "includes.h"
#include <ntdll.h>
#include <stdio.h>
#include "results_dlg.h"
#include "backup.h"

#include <syslib\str.h>

sqlite3 *hWorkerDB=NULL,
        *hCheckerDB=NULL;

void DB_Masscan_Remove(PCONNECTION lpConnection,DWORD dwIP,WORD wPort);
void DB_Worker_Update(DWORD dwIP,WORD wPort,DWORD dwIdx,DWORD dwFlags);
void DB_Worker_AppendGood(PCONNECTION lpConneciton,DWORD dwIP,WORD wPort,DWORD dwIdx,LPCSTR lpPassword,LPCSTR lpDesktop);
void DB_Worker_AppendBad(DWORD dwIP,WORD wPort,DWORD dwIdx);

bool DB_Worker_RescanIgnored();
void DB_Worker_RebuildBad();
void DB_Worker_RebuildIgnored();
void DB_Worker_RebuildGood();
void DB_Worker_RebuildIgnore();
void DB_TotalIPs_Rebuild();

int sqlite3_step_ex(sqlite3_stmt *hStmt)
{
    int iErr;
    while (true)
    {
        iErr=sqlite3_step(hStmt);

        if ((iErr == SQLITE_BUSY) || (iErr == SQLITE_LOCKED))
        {
            if (WaitForSingleObject(hShutdownEvent,1) == WAIT_TIMEOUT)
                continue;
        }

        break;
    }
    return iErr;
}

static int sqlite3_exec_ex(sqlite3 *hDB,LPCSTR lpSql)
{
    sqlite3_stmt *hStmt=NULL;
    int iErr=sqlite3_prepare_v2(hDB,lpSql,-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        iErr=sqlite3_step_ex(hStmt);
        sqlite3_finalize(hStmt);
    }
    return iErr;
}

static DB_TRANS Trans[2];
static PDB_TRANS DB_FindTrans(sqlite3 *hDB)
{
    PDB_TRANS lpTrans=NULL;
    for (DWORD i=0; i < ARRAYSIZE(Trans); i++)
    {
        if (Trans[i].hDB != hDB)
            continue;

        lpTrans=&Trans[i];
        break;
    }
    return lpTrans;
}

static void DB_Close(sqlite3 *hDB)
{
    sqlite3_wal_checkpoint_v2(hDB,NULL,SQLITE_CHECKPOINT_RESTART,NULL,NULL);
    sqlite3_close(hDB);

    PDB_TRANS lpTrans=DB_FindTrans(hDB);
    if (lpTrans)
        DeleteCriticalSection(&lpTrans->csTrans);
    return;
}

static void DB_BeginTransaction(sqlite3 *hDB)
{
    PDB_TRANS lpTrans=DB_FindTrans(hDB);
    if (lpTrans)
    {
        EnterCriticalSection(&lpTrans->csTrans);
        {
            if (!lpTrans->dwRef)
                sqlite3_exec_ex(hDB,"BEGIN TRANSACTION");

            lpTrans->dwRef++;
        }
        LeaveCriticalSection(&lpTrans->csTrans);
    }
    return;
}

static void DB_CommitTransaction(sqlite3 *hDB)
{
    PDB_TRANS lpTrans=DB_FindTrans(hDB);
    if ((lpTrans) && (lpTrans->dwRef))
    {
        EnterCriticalSection(&lpTrans->csTrans);
        {
            lpTrans->dwRef--;

            if (!lpTrans->dwRef)
                sqlite3_exec_ex(hDB,"COMMIT");
        }
        LeaveCriticalSection(&lpTrans->csTrans);
    }
    return;
}

static volatile DWORD dwWorkerModsCount=0;
CRITICAL_SECTION csMasscanAccess,csWorkerAccess;
static sqlite3_stmt *DB_Worker_Update_Begin()
{
    EnterCriticalSection(&csWorkerAccess);

    sqlite3_stmt *hStmt=NULL;
    int iErr=sqlite3_prepare_v2(hWorkerDB,"UPDATE bad SET idx=? WHERE ip=? AND port=?",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
        DB_BeginTransaction(hWorkerDB);
    else
        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    return hStmt;
}

static bool DB_Worker_Update(sqlite3_stmt *hStmt,DWORD dwIP,WORD wPort,DWORD dwIdx)
{
    sqlite3_reset(hStmt);

    sqlite3_bind_int(hStmt,1,dwIdx);
    sqlite3_bind_text(hStmt,2,NetNtoA(dwIP),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(hStmt,3,wPort);

    int iErr=sqlite3_step_ex(hStmt);
    bool bRet=(iErr == SQLITE_DONE);

    if (!bRet)
        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");
    return bRet;
}

static void DB_Worker_Update_End(sqlite3_stmt *hStmt)
{
    sqlite3_finalize(hStmt);
    DB_CommitTransaction(hWorkerDB);

    LeaveCriticalSection(&csWorkerAccess);
    return;
}

static void DB_MarkUpdated()
{
    LARGE_INTEGER liTime;
    RtlSecondsSince1980ToTime(Now(),&liTime);

    FILETIME ft;
    ft.dwHighDateTime=liTime.HighPart;
    ft.dwLowDateTime=liTime.LowPart;

    FILETIME ftLocal;
    FileTimeToLocalFileTime(&ft,&ftLocal);

    SYSTEMTIME st;
    FileTimeToSystemTime(&ftLocal,&st);

    TCHAR szTime[MAX_PATH];
    wsprintf(szTime,_T("DB updated: %.4d/%.2d/%.2d %.2d:%.2d:%.2d"),st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);

    SendDlgItemMessage(hMainDlg,IDC_SBR1,SB_SETTEXT,1,(LPARAM)szTime);
    return;
}

DWORD dwDBUpdateThreadId;
static CRITICAL_SECTION csDBUpdate;
static PUPDATE_QUEUE lpUpdateQueue=NULL,lpLastUpdateQueue=NULL;
static void DB_Update()
{
    PUPDATE_QUEUE lpQueueItem;
    EnterCriticalSection(&csDBUpdate);
    {
        lpQueueItem=lpUpdateQueue;
        lpLastUpdateQueue=NULL;
        lpUpdateQueue=NULL;
    }
    LeaveCriticalSection(&csDBUpdate);

    do
    {
        if (!lpQueueItem)
            break;

        sqlite3_stmt *hStmt=DB_Worker_Update_Begin();
        if (!hStmt)
        {
            do
            {
                PUPDATE_QUEUE lpNext=lpQueueItem->lpNext;
                MemFree(lpQueueItem);
                lpQueueItem=lpNext;
            }
            while (lpQueueItem);
            break;
        }

        DB_MarkUpdated();

        do
        {
            DB_Worker_Update(hStmt,lpQueueItem->bii.dwIP,lpQueueItem->bii.wPort,lpQueueItem->bii.dwIdx);

            PUPDATE_QUEUE lpNext=lpQueueItem->lpNext;
            MemFree(lpQueueItem);
            lpQueueItem=lpNext;
        }
        while (lpQueueItem);

        DB_MarkUpdated();

        DB_Worker_Update_End(hStmt);
    }
    while (false);
    return;
}

static void WINAPI DB_OptimizeThread(LPVOID)
{
    while (WaitForSingleObject(hShutdownEvent,60*MILLISECONDS_PER_MINUTE) == WAIT_TIMEOUT)
    {
        if (hWorkerDB)
            DB_Exec(hWorkerDB,"PRAGMA optimize;");

        if (hCheckerDB)
            DB_Exec(hCheckerDB,"PRAGMA optimize;");
    }
    return;
}

static void WINAPI DB_UpdateThread(LPVOID)
{
    while (WaitForSingleObject(hShutdownEvent,1*MILLISECONDS_PER_MINUTE) == WAIT_TIMEOUT)
        DB_Update();

    DB_Update();
    return;
}

static void DB_Worker_AppendBad(LPCSTR lpIP,WORD wPort,DWORD dwIdx)
{
    sqlite3_stmt *hStmt;
    int iErr=sqlite3_prepare_v2(hWorkerDB,"INSERT INTO bad(ip,port,idx) VALUES(?,?,?)",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        sqlite3_bind_text(hStmt,1,lpIP,-1,SQLITE_TRANSIENT);
        sqlite3_bind_int(hStmt,2,wPort);
        sqlite3_bind_int(hStmt,3,dwIdx);

        iErr=sqlite3_step_ex(hStmt);
        if (iErr == SQLITE_DONE)
        {
            InterlockedIncrement(&dwWorkerModsCount);

            Worker_AddItem(NetResolveAddress(lpIP),wPort,dwIdx,false);
        }
        else
            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");

        sqlite3_finalize(hStmt);
    }
    else
        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    return;
}

static void WINAPI DB_UpdateMessageThread(HANDLE hEvent)
{
    MSG msg;
    PeekMessage(&msg,NULL,WM_USER,WM_USER,PM_NOREMOVE);
    SetEvent(hEvent);

    while ((int)GetMessage(&msg,NULL,0,0) > 0)
    {
        switch (msg.message)
        {
            case TM_DB_MAINTANCE_RESCAN_INGORED:
            {
                EnableDBMainanceMenu(false);
                {
                    if (DB_Worker_RescanIgnored())
                        EnableDBMainanceMenu(true);
                }
                break;
            }
            case TM_DB_MAINTANCE_REBUILD_BAD:
            {
                EnableDBMainanceMenu(false);
                {
                    DB_Worker_RebuildBad();
                }
                EnableDBMainanceMenu(true);
                break;
            }
            case TM_DB_MAINTANCE_REBUILD_IGNORED:
            {
                EnableDBMainanceMenu(false);
                {
                    DB_Worker_RebuildIgnored();
                }
                EnableDBMainanceMenu(true);
                break;
            }
            case TM_DB_MAINTANCE_REBUILD_GOOD:
            {
                EnableDBMainanceMenu(false);
                {
                    DB_Worker_RebuildGood();
                }
                EnableDBMainanceMenu(true);
                break;
            }
            case TM_DB_MAINTANCE_REBUILD_IGNORE_MASKS:
            {
                EnableDBMainanceMenu(false);
                {
                    DB_Worker_RebuildIgnore();
                }
                EnableDBMainanceMenu(true);
                break;
            }
            case TM_DB_MAINTANCE_RESCAN_UNSUPPORTED:
            {
                EnableDBMainanceMenu(false);
                {
                    PCONNECTION lpConnection=lpConnections;
                    while (lpConnection)
                    {
                        if (lpConnection->dwType == TYPE_WORKER)
                         lpConnection->Sts.ws.dwUnsupported=0;

                        lpConnection=lpConnection->lpNext;
                    }

                    InterlockedExchange(&dwUnsupported,0);
                    Worker_RescanUnsupported();
                }
                EnableDBMainanceMenu(true);
                break;
            }
            case TM_DB_MAINTANCE_REBUILD_TOTAL_IPS:
            {
                EnableDBMainanceMenu(false);
                {
                    DB_TotalIPs_Rebuild();
                }
                EnableDBMainanceMenu(true);
                break;
            }
            case IM_DB_MAINTANCE_PURGE_CHECKER_DB_BEGIN:
            {
                EnableDBMainanceMenu(false);
                {
                    Checker_PurgeDB_Begin();
                }
                break;
            }
            case IM_DB_MAINTANCE_PURGE_CHECKER_DB_END:
            {
                if (Checker_PurgeDB_End())
                {
                    EnableDBMainanceMenu(true);
                }
                break;
            }
            case TM_DB_RESCAN_ITEM:
            {
                if (!msg.lParam)
                    break;

                PRESCAN_ITEM_PARAM lpParam=(PRESCAN_ITEM_PARAM)msg.lParam;

                EnterCriticalSection(&csWorkerAccess);
                {
                    if (lpParam->dwIdx)
                        lpParam->dwIdx+=2;

                    char szIP[100];
                    WORD wPort=5900;
                    sscanf(lpParam->szAddress,"%[^:]:%d",szIP,&wPort);
                    DB_Worker_AppendBad(szIP,wPort,lpParam->dwIdx);

                    sqlite3_stmt *hStmt;
                    int iErr=sqlite3_prepare_v2(hWorkerDB,"DELETE FROM good WHERE address=?",-1,&hStmt,0);
                    if (iErr == SQLITE_OK)
                    {
                        sqlite3_bind_text(hStmt,1,lpParam->szAddress,-1,SQLITE_TRANSIENT);

                        iErr=sqlite3_step_ex(hStmt);
                        if (iErr != SQLITE_DONE)
                            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");

                        sqlite3_finalize(hStmt);
                    }
                    else
                        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");

                    iErr=sqlite3_prepare_v2(hWorkerDB,"DELETE FROM good_ignored WHERE address=?",-1,&hStmt,0);
                    if (iErr == SQLITE_OK)
                    {
                        sqlite3_bind_text(hStmt,1,lpParam->szAddress,-1,SQLITE_TRANSIENT);

                        iErr=sqlite3_step_ex(hStmt);
                        if (iErr != SQLITE_DONE)
                            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");

                        sqlite3_finalize(hStmt);
                    }
                    else
                        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
                }
                LeaveCriticalSection(&csWorkerAccess);

                MemFree(lpParam);
                break;
            }
            case TM_DB_ITEM_COMMENT:
            {
                if (!msg.lParam)
                    break;

                PCHANGE_ITEM_COMMENT_PARAM lpParam=(PCHANGE_ITEM_COMMENT_PARAM)msg.lParam;

                EnterCriticalSection(&csWorkerAccess);
                {
                    sqlite3_stmt *hStmt;
                    int iErr=sqlite3_prepare_v2(hWorkerDB,"UPDATE good SET comments=? WHERE address=?",-1,&hStmt,0);
                    if (iErr == SQLITE_OK)
                    {
                        sqlite3_bind_text(hStmt,1,lpParam->szComment,-1,SQLITE_TRANSIENT);
                        sqlite3_bind_text(hStmt,2,lpParam->szAddress,-1,SQLITE_TRANSIENT);

                        iErr=sqlite3_step_ex(hStmt);
                        if (iErr != SQLITE_DONE)
                            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");

                        sqlite3_finalize(hStmt);
                    }
                    else
                        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
                }
                LeaveCriticalSection(&csWorkerAccess);

                MemFree(lpParam);
                break;
            }
            case TM_DB_ADD_IGNORE_FILTER:
            {
                if (!msg.lParam)
                    break;

                EnterCriticalSection(&csWorkerAccess);
                {
                    sqlite3_stmt *hStmt;
                    int iErr=sqlite3_prepare_v2(hWorkerDB,"INSERT INTO ignore(desk) VALUES(?)",-1,&hStmt,0);
                    if (iErr == SQLITE_OK)
                    {
                        sqlite3_bind_text(hStmt,1,(LPCSTR)msg.lParam,-1,SQLITE_TRANSIENT);

                        iErr=sqlite3_step_ex(hStmt);
                        if (iErr != SQLITE_DONE)
                            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");

                        sqlite3_finalize(hStmt);
                    }
                    else
                        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
                }
                LeaveCriticalSection(&csWorkerAccess);

                MemFree((LPVOID)msg.lParam);
                break;
            }
            case TM_DB_MOVE_TO_IGNORED:
            {
                if (!msg.lParam)
                    break;

                EnterCriticalSection(&csWorkerAccess);
                {
                    bool bMoved=false;
                    sqlite3_stmt *hStmt;
                    int iErr=sqlite3_prepare_v2(hWorkerDB,"INSERT INTO good_ignored(address,idx,pwd,desk,comments,found) SELECT address,idx,pwd,desk,comments,found FROM good WHERE address=?",-1,&hStmt,0);
                    if (iErr == SQLITE_OK)
                    {
                        sqlite3_bind_text(hStmt,1,(LPCSTR)msg.lParam,-1,SQLITE_TRANSIENT);

                        iErr=sqlite3_step_ex(hStmt);
                        bMoved=(iErr == SQLITE_DONE);

                        if (!bMoved)
                            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");

                        sqlite3_finalize(hStmt);
                    }
                    else
                        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");

                    if (bMoved)
                    {
                        sqlite3_stmt *hStmt;
                        int iErr=sqlite3_prepare_v2(hWorkerDB,"DELETE FROM good WHERE address=?",-1,&hStmt,0);
                        if (iErr == SQLITE_OK)
                        {
                            sqlite3_bind_text(hStmt,1,(LPCSTR)msg.lParam,-1,SQLITE_TRANSIENT);

                            iErr=sqlite3_step_ex(hStmt);
                            if (iErr != SQLITE_DONE)
                                DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");

                            sqlite3_finalize(hStmt);
                        }
                        else
                            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
                    }
                }
                LeaveCriticalSection(&csWorkerAccess);

                MemFree((LPVOID)msg.lParam);
                break;
            }
            case TM_DB_MOVE_TO_IGNORED_FINISH:
            {
                EnterCriticalSection(&csWorkerAccess);
                {
                    LPCSTR lpRebuild="DROP TABLE IF EXISTS new_good_ignored;"
                                     "CREATE TABLE new_good_ignored(id INTEGER NOT NULL UNIQUE,address TEXT NOT NULL UNIQUE,idx INTEGER,pwd TEXT,desk TEXT,comments TEXT,found TEXT,PRIMARY KEY(id AUTOINCREMENT));"
                                     "INSERT INTO new_good_ignored(address,idx,pwd,desk,comments,found) SELECT address,idx,pwd,desk,comments,found FROM good_ignored ORDER BY found,address;"
                                     "DROP TABLE good_ignored;"
                                     "ALTER TABLE new_good_ignored RENAME TO good_ignored;"
                                     "DROP TABLE IF EXISTS new_good;"
                                     "CREATE TABLE new_good(id INTEGER NOT NULL UNIQUE,address TEXT NOT NULL UNIQUE,idx INTEGER,pwd TEXT,desk TEXT,comments TEXT,found TEXT,PRIMARY KEY(id AUTOINCREMENT));"
                                     "INSERT INTO new_good(address,idx,pwd,desk,comments,found) SELECT address,idx,pwd,desk,comments,found FROM good ORDER BY found,address;"
                                     "DROP TABLE good;"
                                     "ALTER TABLE new_good RENAME TO good;"
                                     "VACUUM;";

                    DB_Exec(hWorkerDB,lpRebuild);
                }
                LeaveCriticalSection(&csWorkerAccess);

                EnableDBMainanceMenu(true);
                break;
            }
            case TM_DB_UPDATE:
            {
                PUPDATE_QUEUE lpItem=(PUPDATE_QUEUE)msg.lParam;
                if (!lpItem)
                    break;

                while (lpItem)
                {
                    PUPDATE_QUEUE lpNext=lpItem->lpNext;
                    switch (lpItem->ut)
                    {
                        case UT_RFB_FOUND:
                        {
                            DebugLog_AddItem2(NULL,LOG_LEVEL_INFO,"New RFB found: %s:%d",NetNtoA(lpItem->mii.dwIP),lpItem->mii.wPort);

                            EnterCriticalSection(&csWorkerAccess);
                            {
                                DB_Worker_AppendBad(lpItem->mii.dwIP,lpItem->mii.wPort,0);
                                DB_TotalIPs_Append(lpItem->mii.dwIP,lpItem->mii.wPort);
                            }
                            LeaveCriticalSection(&csWorkerAccess);
                        }
                        case UT_NON_RFB_FOUND:
                        {
                            DB_Masscan_Remove(lpItem->lpConnection,lpItem->mii.dwIP,lpItem->mii.wPort);
                            break;
                        }
                        case UT_BAD_UPDATE:
                        {
                            if (lpItem->bii.dwFlg)
                            {
                                EnterCriticalSection(&csWorkerAccess);
                                    DB_Worker_Update(lpItem->gii.dwIP,lpItem->gii.wPort,lpItem->gii.dwIdx,lpItem->bii.dwFlg);
                                LeaveCriticalSection(&csWorkerAccess);
                                break;
                            }

                            lpItem->lpNext=NULL;

                            EnterCriticalSection(&csDBUpdate);
                            {
                                if (lpLastUpdateQueue)
                                {
                                    lpLastUpdateQueue->lpNext=lpItem;
                                    lpLastUpdateQueue=lpItem;
                                }
                                else
                                {
                                    lpUpdateQueue=lpItem;
                                    lpLastUpdateQueue=lpItem;
                                }
                            }
                            LeaveCriticalSection(&csDBUpdate);

                            lpItem=lpNext;
                            continue;
                        }
                        case UT_GOOD_FOUND:
                        {
                            LPCSTR lpPassword=NULL;
                            if (!lpItem->gii.bNoAuth)
                                lpPassword=lpItem->gii.szPassword;

                            Worker_RemoveBad(lpItem->lpConnection,lpItem->gii.dwIP,lpItem->gii.wPort);

                            EnterCriticalSection(&csWorkerAccess);
                                DB_Worker_AppendGood(lpItem->lpConnection,lpItem->gii.dwIP,lpItem->gii.wPort,lpItem->gii.dwIdx,lpPassword,lpItem->gii.lpDesktop);
                            LeaveCriticalSection(&csWorkerAccess);

                            MemFree(lpItem->gii.lpDesktop);
                            break;
                        }
                    }

                    MemFree(lpItem);
                    lpItem=lpNext;
                }
                break;
            }
        }
    }
    return;
}

void DB_PostUpdateQueue(PUPDATE_QUEUE lpQueue)
{
    PostThreadMessageEx(dwDBUpdateThreadId,TM_DB_UPDATE,NULL,(LPARAM)lpQueue);
    return;
}

void DB_Worker_AddIgnoreFilter(LPCSTR lpFilter)
{
    if ((lpFilter) && (lpFilter[0]))
        PostThreadMessageEx(dwDBUpdateThreadId,TM_DB_ADD_IGNORE_FILTER,NULL,(LPARAM)StrDuplicateA(lpFilter,0));
    return;
}

bool DB_Worker_IsGoodAddressPresent(LPCSTR lpAddress,LPDWORD lpdwIdx)
{
    if (!lpAddress[0])
        return false;

    bool bRet=false;

    sqlite3_stmt *hStmt;
    int iErr=sqlite3_prepare_v2(hWorkerDB,"SELECT idx FROM good WHERE address=?",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        sqlite3_bind_text(hStmt,1,lpAddress,-1,SQLITE_TRANSIENT);

        if (sqlite3_step_ex(hStmt) == SQLITE_ROW)
        {
            *lpdwIdx=sqlite3_column_int(hStmt,0);
            bRet=true;
        }

        sqlite3_finalize(hStmt);
    }
    else
        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");

    if (!bRet)
    {
        int iErr=sqlite3_prepare_v2(hWorkerDB,"SELECT idx FROM good_ignored WHERE address=?",-1,&hStmt,0);
        if (iErr == SQLITE_OK)
        {
            sqlite3_bind_text(hStmt,1,lpAddress,-1,SQLITE_TRANSIENT);

            if (sqlite3_step_ex(hStmt) == SQLITE_ROW)
            {
                *lpdwIdx=sqlite3_column_int(hStmt,0);
                bRet=true;
            }

            sqlite3_finalize(hStmt);
        }
        else
            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    }
    return bRet;
}

bool DB_Worker_IsIgnored(LPCSTR lpDesk)
{
    if (!lpDesk[0])
        return true;

    bool bRet=false;

    sqlite3_stmt *hStmt;
    int iErr=sqlite3_prepare_v2(hWorkerDB,"SELECT desk FROM ignore",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        while (sqlite3_step_ex(hStmt) == SQLITE_ROW)
        {
            LPCSTR lpMask=(LPCSTR)sqlite3_column_text(hStmt,0);
            if (!lstrcmpA(lpMask,"{hex}"))
            {
                DWORD dwLen=lstrlenA(lpDesk);
                if ((dwLen < 6) || (dwLen > 8))
                    continue;

                if (lpDesk[0] == '0')
                    continue;

                DWORD dwHexVal=StrToHexA(lpDesk);
                if (!dwHexVal)
                    continue;

                char szHexVal[10];
                wsprintfA(szHexVal,"%X",dwHexVal);

                if (lstrcmpA(szHexVal,lpDesk))
                    continue;
            }
            else if (!lstrcmpA(lpMask,"{lettercase}"))
            {
                DWORD dwLen=lstrlenA(lpDesk);
                bool bUpperPresent=false,pLowerPresent=false,bIgnored=false;
                for (DWORD i=0; i < dwLen; i++)
                {
                    LPCSTR lpAnsiTable="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789`~!@#$%^&*()-_=+\"№;%:?[{]};:'\\|,<.>/? \t";
                    if (!StrChrA(lpAnsiTable,lpDesk[i]))
                        continue;

                    if (IsCharLowerA(lpDesk[i]))
                    {
                        if (bUpperPresent)
                        {
                            bIgnored=true;
                            break;
                        }

                        pLowerPresent=true;
                        continue;
                    }
                    else if (IsCharUpperA(lpDesk[i]))
                    {
                        if (pLowerPresent)
                        {
                            bIgnored=true;
                            break;
                        }

                        bUpperPresent=true;
                        continue;
                    }
                }

                if (!bIgnored)
                    continue;
            }
            else if (!lstrcmpA(lpMask,"{ip}"))
            {
                sockaddr_in sa;
                if (inet_pton(AF_INET,lpDesk,&sa.sin_addr) != 1)
                    continue;
            }
            else if (!lstrcmpA(lpMask,"{et_hmi}"))
            {
                DWORD dwLen=lstrlenA(lpDesk);
                if (dwLen != 14)
                    continue;

                if (lpDesk[0] != 'E')
                    continue;

                if (lpDesk[1] != 'T')
                    continue;

                DWORD64 dwHexVal=StrToHex64A(&lpDesk[2]);
                if (!dwHexVal)
                    continue;

                char szHexVal[20];
                wsprintfA(szHexVal,"ET%012I64X",dwHexVal);

                if (lstrcmpA(szHexVal,lpDesk))
                    continue;
            }
            else
            {
                if (!PathMatchSpecA(lpDesk,lpMask))
                    continue;
            }

            bRet=true;
            break;
        }

        sqlite3_finalize(hStmt);
    }
    else
        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    return bRet;
}

static void DB_Worker_AppendGood(PCONNECTION lpConnection,DWORD dwIP,WORD wPort,DWORD dwIdx,LPCSTR lpPassword,LPCSTR lpDesktop)
{
    bool bIgnored=false;

    char szAddress[MAX_PATH],szNow[MAX_PATH];

    if (!lpDesktop)
        lpDesktop="";

    sqlite3_stmt *hStmt;
    int iErr=sqlite3_prepare_v2(hWorkerDB,"DELETE FROM bad WHERE ip=? AND port=?",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        sqlite3_bind_text(hStmt,1,NetNtoA(dwIP),-1,SQLITE_TRANSIENT);
        sqlite3_bind_int(hStmt,2,wPort);

        iErr=sqlite3_step_ex(hStmt);
        if (iErr == SQLITE_DONE)
            InterlockedIncrement(&dwWorkerModsCount);
        else
            DB_LogErr(lpConnection,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");

        sqlite3_finalize(hStmt);
    }
    else
        DB_LogErr(lpConnection,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");

    LPCSTR lpStatement;
    if (DB_Worker_IsIgnored(lpDesktop))
    {
        lpStatement="INSERT INTO good_ignored(address,pwd,idx,desk,found) VALUES(?,?,?,?,?)";
        bIgnored=true;
    }
    else
        lpStatement="INSERT INTO good(address,pwd,idx,desk,found) VALUES(?,?,?,?,?)";

    iErr=sqlite3_prepare_v2(hWorkerDB,lpStatement,-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        if (wPort == 5900)
            lstrcpyA(szAddress,NetNtoA(dwIP));
        else
            wsprintfA(szAddress,"%s:%d",NetNtoA(dwIP),wPort);

        sqlite3_bind_text(hStmt,1,szAddress,-1,SQLITE_TRANSIENT);

        if (!lpPassword)
            sqlite3_bind_null(hStmt,2);
        else
            sqlite3_bind_text(hStmt,2,lpPassword,-1,SQLITE_TRANSIENT);

        sqlite3_bind_int(hStmt,3,dwIdx);
        sqlite3_bind_text(hStmt,4,lpDesktop,-1,SQLITE_TRANSIENT);

        SYSTEMTIME st;
        GetLocalTime(&st);

        wsprintfA(szNow,"%.4d/%.2d/%.2d %.2d:%.2d:%.2d",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
        sqlite3_bind_text(hStmt,5,szNow,-1,SQLITE_TRANSIENT);

        iErr=sqlite3_step_ex(hStmt);
        if (iErr != SQLITE_DONE)
            DB_LogErr(lpConnection,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");

        sqlite3_finalize(hStmt);
    }
    else
        DB_LogErr(lpConnection,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");

    char szPassword[256]={0};
    if (!lpPassword)
        lstrcpyA(szPassword,"[empty]");
    else
        wsprintfA(szPassword,"\"%s\"",lpPassword);

    if (!bIgnored)
        Notify_SendFound(lpDesktop,dwIdx,szPassword);

    if (!lpPassword)
        lpPassword="[empty]";

    if (!bIgnored)
        Results_Append(szNow,szAddress,lpDesktop,dwIdx,lpPassword);

    Worker_AppendLog(lpConnection,szNow,szAddress,lpDesktop,dwIdx,lpPassword);
    return;
}

static void DB_Worker_AppendBad(DWORD dwIP,WORD wPort,DWORD dwIdx)
{
    DB_Worker_AppendBad(NetNtoA(dwIP),wPort,dwIdx);
    return;
}

void DB_Worker_Rescan(LPCSTR lpAddress,DWORD dwIdx)
{
    if ((lpAddress) && (lpAddress[0]))
    {
        PRESCAN_ITEM_PARAM lpParam=(PRESCAN_ITEM_PARAM)MemQuickAlloc(sizeof(*lpParam));
        if (lpParam)
        {
            lstrcpyA(lpParam->szAddress,lpAddress);
            lpParam->dwIdx=dwIdx;

            PostThreadMessageEx(dwDBUpdateThreadId,TM_DB_RESCAN_ITEM,NULL,(LPARAM)lpParam);
        }
    }
    return;
}

void DB_Worker_ChangeComment(LPCSTR lpAddress,LPCSTR lpComment)
{
    if (((lpAddress) && (lpAddress[0])) && ((lpComment) && (lpComment[0])))
    {
        PCHANGE_ITEM_COMMENT_PARAM lpParam=(PCHANGE_ITEM_COMMENT_PARAM)MemQuickAlloc(sizeof(*lpParam));
        if (lpParam)
        {
            lstrcpyA(lpParam->szAddress,lpAddress);
            lstrcpyA(lpParam->szComment,lpComment);

            PostThreadMessageEx(dwDBUpdateThreadId,TM_DB_ITEM_COMMENT,NULL,(LPARAM)lpParam);
        }
    }
}

static void DB_Worker_Update(DWORD dwIP,WORD wPort,DWORD dwIdx,DWORD dwFlags)
{
    sqlite3_stmt *hStmt;
    int iErr=sqlite3_prepare_v2(hWorkerDB,"UPDATE bad SET flg=?, idx=? WHERE ip=? AND port=?",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        sqlite3_bind_int(hStmt,1,dwFlags);
        sqlite3_bind_int(hStmt,2,dwIdx);
        sqlite3_bind_text(hStmt,3,NetNtoA(dwIP),-1,SQLITE_TRANSIENT);
        sqlite3_bind_int(hStmt,4,wPort);

        iErr=sqlite3_step_ex(hStmt);
        if (iErr != SQLITE_DONE)
            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");

        sqlite3_finalize(hStmt);
    }
    else
        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    return;
}

void DB_Worker_Read()
{
    sqlite3_stmt *hStmt;
    int iErr=sqlite3_prepare_v2(hWorkerDB,"SELECT ip,port,idx FROM bad WHERE flg=0 ORDER BY RANDOM()",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        while (sqlite3_step_ex(hStmt) == SQLITE_ROW)
        {
            Worker_AddItem(NetResolveAddress((LPCSTR)sqlite3_column_text(hStmt,0)),
                           sqlite3_column_int(hStmt,1),
                           sqlite3_column_int(hStmt,2),true);
        }
        sqlite3_finalize(hStmt);
    }
    else
        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    return;
}

DWORD DB_Worker_RescanUnsupported(PRFB_SERVER *lppUnsupportedItems,PRFB_SERVER *lppLastUnsupportedItem)
{
    DWORD dwRet=0;

    sqlite3_stmt *hStmt;
    int iErr=sqlite3_prepare_v2(hWorkerDB,"SELECT ip,port,idx FROM bad WHERE flg != 0 ORDER BY RANDOM()",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        while (sqlite3_step_ex(hStmt) == SQLITE_ROW)
        {
            if (Worker_AddItemForRescan(lppUnsupportedItems,lppLastUnsupportedItem,
                                        NetResolveAddress((LPCSTR)sqlite3_column_text(hStmt,0)),
                                        sqlite3_column_int(hStmt,1),
                                        sqlite3_column_int(hStmt,2)))
            dwRet++;
        }
        sqlite3_finalize(hStmt);
    }
    else
        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");

    if (dwRet)
    {
        EnterCriticalSection(&csWorkerAccess);
            DB_Exec(hWorkerDB,"UPDATE bad SET flg=0 WHERE flg!=0");
        LeaveCriticalSection(&csWorkerAccess);
    }
    return dwRet;
}

void DB_Worker_MoveToIgnored(LPCSTR lpAddress)
{
    if (lpAddress)
        PostThreadMessageEx(dwDBUpdateThreadId,TM_DB_MOVE_TO_IGNORED,NULL,(LPARAM)StrDuplicateA(lpAddress,0));
    return;
}

static bool DB_Worker_RescanIgnored()
{
    bool bEnable=true;
    DB_BeginTransaction(hWorkerDB);
    {
        sqlite3_stmt *hStmt;
        int iErr=sqlite3_prepare_v2(hWorkerDB,"SELECT address,desk FROM good",-1,&hStmt,0);
        if (iErr == SQLITE_OK)
        {
            DWORD dwMovedItems=0;
            while (sqlite3_step_ex(hStmt) == SQLITE_ROW)
            {
                LPCSTR lpDesk=(LPCSTR)sqlite3_column_text(hStmt,1);
                if (!DB_Worker_IsIgnored(lpDesk))
                    continue;

                DebugLog_AddItem2(NULL,LOG_LEVEL_INFO,"New ignored item: %s",lpDesk);
                DB_Worker_MoveToIgnored((LPCSTR)sqlite3_column_text(hStmt,0));

                dwMovedItems++;
            }
            sqlite3_finalize(hStmt);

            if (dwMovedItems)
            {
                bEnable=false;
                PostThreadMessageEx(dwDBUpdateThreadId,TM_DB_MOVE_TO_IGNORED_FINISH,NULL,NULL);
            }
        }
        else
            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    }
    DB_CommitTransaction(hWorkerDB);
    return bEnable;
}

static void DB_Worker_RebuildBad()
{
    DB_BeginTransaction(hWorkerDB);
    {
        LPCSTR lpRebuild="DROP TABLE IF EXISTS new_bad;"
                         "CREATE TABLE new_bad(id INTEGER NOT NULL UNIQUE,ip TEXT NOT NULL,port INTEGER NOT NULL DEFAULT 5900,idx INTEGER DEFAULT 0,flg INTEGER DEFAULT 0,PRIMARY KEY(id AUTOINCREMENT),UNIQUE(ip,port));"
                         "INSERT INTO new_bad(ip,port,idx,flg) SELECT ip,port,idx,flg FROM bad ORDER BY ip,port;"
                         "DROP TABLE bad;"
                         "ALTER TABLE new_bad RENAME TO bad;";

        DB_Exec(hWorkerDB,lpRebuild);
    }
    DB_CommitTransaction(hWorkerDB);

    EnterCriticalSection(&csWorkerAccess);
        DB_Exec(hWorkerDB,"VACUUM;");
    LeaveCriticalSection(&csWorkerAccess);
    return;
}

static void DB_Worker_RebuildIgnored()
{
    DB_BeginTransaction(hWorkerDB);
    {
        LPCSTR lpRebuild="DROP TABLE IF EXISTS new_good_ignored;"
                         "CREATE TABLE new_good_ignored(id INTEGER NOT NULL UNIQUE,address TEXT NOT NULL UNIQUE,idx INTEGER,pwd TEXT,desk TEXT,comments TEXT,found TEXT,PRIMARY KEY(id AUTOINCREMENT));"
                         "INSERT INTO new_good_ignored(address,idx,pwd,desk,comments,found) SELECT address,idx,pwd,desk,comments,found FROM good_ignored ORDER BY found,address;"
                         "DROP TABLE good_ignored;"
                         "ALTER TABLE new_good_ignored RENAME TO good_ignored;";

        DB_Exec(hWorkerDB,lpRebuild);
    }
    DB_CommitTransaction(hWorkerDB);

    EnterCriticalSection(&csWorkerAccess);
        DB_Exec(hWorkerDB,"VACUUM;");
    LeaveCriticalSection(&csWorkerAccess);
    return;
}

static void DB_Worker_RebuildGood()
{
    DB_BeginTransaction(hWorkerDB);
    {
        LPCSTR lpRebuild="DROP TABLE IF EXISTS new_good;"
                         "CREATE TABLE new_good(id INTEGER NOT NULL UNIQUE,address TEXT NOT NULL UNIQUE,idx INTEGER,pwd TEXT,desk TEXT,comments TEXT,found TEXT,PRIMARY KEY(id AUTOINCREMENT));"
                         "INSERT INTO new_good(address,idx,pwd,desk,comments,found) SELECT address,idx,pwd,desk,comments,found FROM good ORDER BY found,address;"
                         "DROP TABLE good;"
                         "ALTER TABLE new_good RENAME TO good;";

        DB_Exec(hWorkerDB,lpRebuild);
    }
    DB_CommitTransaction(hWorkerDB);

    EnterCriticalSection(&csWorkerAccess);
        DB_Exec(hWorkerDB,"VACUUM;");
    LeaveCriticalSection(&csWorkerAccess);
    return;
}

static void DB_Worker_RebuildIgnore()
{
    DB_BeginTransaction(hWorkerDB);
    {
        LPCSTR lpRebuild="DROP TABLE IF EXISTS new_ignore;"
                         "CREATE TABLE new_ignore(id INTEGER NOT NULL UNIQUE,desk TEXT NOT NULL UNIQUE,PRIMARY KEY(id AUTOINCREMENT));"
                         "INSERT INTO new_ignore(desk) SELECT desk FROM ignore ORDER BY desk;"
                         "DROP TABLE ignore;"
                         "ALTER TABLE new_ignore RENAME TO ignore;";

        DB_Exec(hWorkerDB,lpRebuild);
    }
    DB_CommitTransaction(hWorkerDB);

    EnterCriticalSection(&csWorkerAccess);
        DB_Exec(hWorkerDB,"VACUUM;");
    LeaveCriticalSection(&csWorkerAccess);
    return;
}

static void DB_TotalIPs_Rebuild()
{
    DB_BeginTransaction(hWorkerDB);
    {
        LPCSTR lpRebuild="DROP TABLE IF EXISTS new_total_ips;"
                         "CREATE TABLE new_total_ips(ip TEXT NOT NULL,port INTEGER NOT NULL,UNIQUE(ip,port));"
                         "INSERT INTO new_total_ips(ip,port) SELECT ip,port FROM total_ips ORDER BY ip,port;"
                         "DROP TABLE total_ips;"
                         "ALTER TABLE new_total_ips RENAME TO total_ips;";

        DB_Exec(hWorkerDB,lpRebuild);
    }
    DB_CommitTransaction(hWorkerDB);

    EnterCriticalSection(&csWorkerAccess);
        DB_Exec(hWorkerDB,"VACUUM;");
    LeaveCriticalSection(&csWorkerAccess);
    return;
}

static volatile DWORD dwTotalIPsModsCount=0;
static DWORD dwTotalIPsTls;
bool DB_TotalIPs_IsPresent(DWORD dwIP,WORD wPort)
{
    sqlite3_stmt *hStmt=(sqlite3_stmt*)TlsGetValue(dwTotalIPsTls);
    if (!hStmt)
    {
        int iErr=sqlite3_prepare_v2(hWorkerDB,"SELECT * FROM total_ips WHERE ip=? AND port=?",-1,&hStmt,0);
        if (iErr != SQLITE_OK)
        {
            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
            return false;
        }

        TlsSetValue(dwTotalIPsTls,hStmt);
    }

    sqlite3_reset(hStmt);

    sqlite3_bind_text(hStmt,1,NetNtoA(dwIP),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(hStmt,2,wPort);

    return (sqlite3_step_ex(hStmt) == SQLITE_ROW);
}

void DB_TotalIPs_Append(DWORD dwIP,WORD wPort)
{
    sqlite3_stmt *hStmt;
    int iErr=sqlite3_prepare_v2(hWorkerDB,"INSERT INTO total_ips(ip,port) VALUES(?,?)",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        sqlite3_bind_text(hStmt,1,NetNtoA(dwIP),-1,SQLITE_TRANSIENT);
        sqlite3_bind_int(hStmt,2,wPort);

        iErr=sqlite3_step_ex(hStmt);
        if (iErr == SQLITE_DONE)
            InterlockedIncrement(&dwTotalIPsModsCount);
        else
            DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_step");

        sqlite3_finalize(hStmt);
    }
    else
        DB_LogErr(NULL,hWorkerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    return;
}

static volatile DWORD dwCheckerModsCount=0;
static void DB_Masscan_Remove(PCONNECTION lpConnection,DWORD dwIP,WORD wPort)
{
    sqlite3_stmt *hStmt;
    int iErr=sqlite3_prepare_v2(hCheckerDB,"DELETE FROM list WHERE ip=? AND port=?",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        sqlite3_bind_text(hStmt,1,NetNtoA(dwIP),-1,SQLITE_TRANSIENT);
        sqlite3_bind_int(hStmt,2,wPort);

        iErr=sqlite3_step_ex(hStmt);
        if (iErr == SQLITE_DONE)
            InterlockedIncrement(&dwCheckerModsCount);
        else
            DB_LogErr(NULL,hCheckerDB,LOG_LEVEL_ERROR,"sqlite3_step");

        sqlite3_finalize(hStmt);
    }
    else
        DB_LogErr(NULL,hCheckerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");

    Checker_RemoveAddress(lpConnection,dwIP,wPort);
    return;
}

sqlite3_stmt *DB_Masscan_Append_Begin()
{
    EnterCriticalSection(&csMasscanAccess);

    sqlite3_stmt *hStmt=NULL;
    int iErr=sqlite3_prepare_v2(hCheckerDB,"INSERT INTO list(ip,port) VALUES(?,?)",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
        DB_BeginTransaction(hCheckerDB);
    else
        DB_LogErr(NULL,hCheckerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    return hStmt;
}

bool DB_Masscan_Append(sqlite3_stmt *hStmt,DWORD dwIP,WORD wPort)
{
    sqlite3_reset(hStmt);

    sqlite3_bind_text(hStmt,1,NetNtoA(dwIP),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(hStmt,2,wPort);

    int iErr=sqlite3_step_ex(hStmt);
    bool bRet=(iErr == SQLITE_DONE);

    if ((!bRet) && (iErr != SQLITE_CONSTRAINT))
        DB_LogErr(NULL,hCheckerDB,LOG_LEVEL_ERROR,"sqlite3_step");
    return bRet;
}

void DB_Masscan_Append_End(sqlite3_stmt *hStmt)
{
    sqlite3_finalize(hStmt);
    DB_CommitTransaction(hCheckerDB);

    LeaveCriticalSection(&csMasscanAccess);
    return;
}

DWORD DB_Masscan_CalcItemsToCheck()
{
    DWORD dwItems=0;

    sqlite3_stmt *hStmt;
    int iErr=sqlite3_prepare_v2(hCheckerDB,"SELECT COUNT(id) FROM list",-1,&hStmt,0);
    if (iErr == SQLITE_OK)
    {
        int iErr=sqlite3_step_ex(hStmt);
        if (iErr == SQLITE_ROW)
            dwItems=sqlite3_column_int(hStmt,0);
        else
            DB_LogErr(NULL,hCheckerDB,LOG_LEVEL_ERROR,"sqlite3_step");

        sqlite3_finalize(hStmt);
    }
    else
        DB_LogErr(NULL,hCheckerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    return dwItems;
}

LONGLONG DB_Masscan_GetLastItemId()
{
    LONGLONG llMaxId=0;

    EnterCriticalSection(&csMasscanAccess);
    {
        sqlite3_stmt *hStmt;
        int iErr=sqlite3_prepare_v2(hCheckerDB,"SELECT seq FROM sqlite_sequence WHERE name=\"list\"",-1,&hStmt,0);
        if (iErr == SQLITE_OK)
        {
            int iErr=sqlite3_step_ex(hStmt);
            if (iErr == SQLITE_ROW)
                llMaxId=sqlite3_column_int64(hStmt,0);
            else
                DB_LogErr(NULL,hCheckerDB,LOG_LEVEL_ERROR,"sqlite3_step");

            sqlite3_finalize(hStmt);
        }
        else
            DB_LogErr(NULL,hCheckerDB,LOG_LEVEL_ERROR,"sqlite3_prepare_v2");
    }
    LeaveCriticalSection(&csMasscanAccess);
    return llMaxId;
}

bool DB_Masscan_Purge()
{
    bool bRet=true;
    EnterCriticalSection(&csMasscanAccess);
    {
        DB_BeginTransaction(hCheckerDB);
        {
            LPCSTR lpPurge="DROP TABLE list;"
                           "CREATE TABLE list(id INTEGER NOT NULL UNIQUE,ip TEXT NOT NULL,port INTEGER NOT NULL,UNIQUE(ip,port),PRIMARY KEY(id AUTOINCREMENT));";

            int iErr=sqlite3_exec(hCheckerDB,lpPurge,NULL,NULL,NULL);
            if (iErr != SQLITE_OK)
            {
                bRet=false;
                DB_LogErr(NULL,hCheckerDB,LOG_LEVEL_ERROR,"sqlite3_exec");
            }
        }
        DB_CommitTransaction(hCheckerDB);

        if (bRet)
        {
            sqlite3_exec(hCheckerDB,"VACUUM;",NULL,NULL,NULL);
            dwCheckerModsCount=0;
        }
    }
    LeaveCriticalSection(&csMasscanAccess);
    return bRet;
}

static cJSON *jsDB;
static bool bClearFlagsOnExit,bRebuildCheckersOnExit;
void DB_UpdateMenuState(HMENU hMenu,DWORD dwItem)
{
    switch (dwItem)
    {
        case -1:
        {
            UpdateMenuState(hMenu,IDM_REBUILD_CHECKERS_ON_EXIT,bRebuildCheckersOnExit);
            UpdateMenuState(hMenu,IDM_CLEAR_UNSUPPORTED_ON_EXIT,bClearFlagsOnExit);
            break;
        }
        case IDM_REBUILD_CHECKERS_ON_EXIT:
        {
            bRebuildCheckersOnExit=!bRebuildCheckersOnExit;
            UpdateMenuState(hMenu,dwItem,bRebuildCheckersOnExit);

            cJSON *jsRebuildCheckersOnExit=cJSON_GetObjectItem(jsDB,"rebuild_checkers_on_exit");
            if (jsRebuildCheckersOnExit)
                cJSON_SetBoolValue(jsRebuildCheckersOnExit,bRebuildCheckersOnExit);

            Config_Update();
            break;
        }
        case IDM_CLEAR_UNSUPPORTED_ON_EXIT:
        {
            bClearFlagsOnExit=!bClearFlagsOnExit;
            UpdateMenuState(hMenu,dwItem,bClearFlagsOnExit);

            cJSON *jsClearFlagsOnExit=cJSON_GetObjectItem(jsDB,"clear_flags_on_exit");
            if (jsClearFlagsOnExit)
                cJSON_SetBoolValue(jsClearFlagsOnExit,bClearFlagsOnExit);

            Config_Update();
            break;
        }
    }
    return;
}

HANDLE hDBThreadsGroup;
bool DB_Init(cJSON *jsConfig)
{
    bool bRet=false;
    do
    {
        if (sqlite3_initialize() != SQLITE_OK)
            break;

        jsDB=cJSON_GetObjectItem(jsConfig,"db");
        if (!jsDB)
            break;

        if (sqlite3_open_v2(cJSON_GetStringFromObject(jsDB,"worker"),&hWorkerDB,SQLITE_OPEN_READWRITE,NULL) != SQLITE_OK)
            break;

        sqlite3_wal_checkpoint_v2(hWorkerDB,NULL,SQLITE_CHECKPOINT_FULL,NULL,NULL);

        if (sqlite3_open_v2(cJSON_GetStringFromObject(jsDB,"checker"),&hCheckerDB,SQLITE_OPEN_READWRITE,NULL) != SQLITE_OK)
            break;

        sqlite3_wal_checkpoint_v2(hCheckerDB,NULL,SQLITE_CHECKPOINT_FULL,NULL,NULL);

        dwTotalIPsTls=TlsAlloc();

        HANDLE hEvent=CreateEvent(NULL,false,false,NULL);
        if (!hEvent)
            break;

        hDBThreadsGroup=ThreadsGroup_Create();
        if (!hDBThreadsGroup)
        {
            CloseHandle(hEvent);
            break;
        }

        Trans[0].hDB=hWorkerDB;
        InitializeCriticalSection(&Trans[0].csTrans);

        Trans[1].hDB=hCheckerDB;
        InitializeCriticalSection(&Trans[1].csTrans);

        bClearFlagsOnExit=cJSON_GetBoolFromObject(jsDB,"clear_flags_on_exit");
        bRebuildCheckersOnExit=cJSON_GetBoolFromObject(jsDB,"rebuild_checkers_on_exit");

        InitializeCriticalSection(&csDBUpdate);
        InitializeCriticalSection(&csWorkerAccess);
        InitializeCriticalSection(&csMasscanAccess);

        ThreadsGroup_CreateThread(hDBThreadsGroup,0,(LPTHREAD_START_ROUTINE)DB_UpdateMessageThread,hEvent,&dwDBUpdateThreadId,NULL);
        WaitForSingleObject(hEvent,INFINITE);
        CloseHandle(hEvent);

        ThreadsGroup_CreateThread(hDBThreadsGroup,0,(LPTHREAD_START_ROUTINE)DB_UpdateThread,NULL,NULL,NULL);
        ThreadsGroup_CreateThread(hDBThreadsGroup,0,(LPTHREAD_START_ROUTINE)DB_OptimizeThread,NULL,NULL,NULL);

        bRet=true;
    }
    while (false);

    if (!bRet)
        DB_Cleanup();

    return bRet;
}

static void DB_CloseAllStmts(sqlite3 *hDB)
{
    while (true)
    {
        sqlite3_stmt *hStmt=sqlite3_next_stmt(hDB,NULL);
        if (!hStmt)
            break;

        sqlite3_finalize(hStmt);
    }
    return;
}

void DB_Cleanup()
{
    Backup_BackupOnExit();

    Splash_SetText(_T("DB cleanup..."));

    if (hDBThreadsGroup)
    {
        PostThreadMessage(dwDBUpdateThreadId,WM_QUIT,NULL,NULL);
        ThreadsGroup_WaitForAllExit(hDBThreadsGroup,INFINITE);
        ThreadsGroup_CloseGroup(hDBThreadsGroup);
        hDBThreadsGroup=NULL;
    }

    if (hCheckerDB)
    {
        DB_CloseAllStmts(hCheckerDB);

        if (dwCheckerModsCount >= REBUILD_REASONS_COUNT)
        {
            if (bRebuildCheckersOnExit)
            {
                Splash_SetText(_T("Checkers table rebuilding..."));

                bool bOk=true;
                DB_BeginTransaction(hCheckerDB);
                {
                    LPCSTR lpRebuild="DROP TABLE IF EXISTS new_list;"
                                     "CREATE TABLE new_list(id INTEGER NOT NULL UNIQUE,ip TEXT NOT NULL,port INTEGER NOT NULL,UNIQUE(ip,port),PRIMARY KEY(id AUTOINCREMENT));"
                                     "INSERT INTO new_list(ip,port) SELECT ip,port FROM list ORDER BY RANDOM();"
                                     "DROP TABLE list;"
                                     "ALTER TABLE new_list RENAME TO list;";

                    if (sqlite3_exec(hCheckerDB,lpRebuild,NULL,NULL,NULL) != SQLITE_OK)
                    {
                        bOk=false;
                        MessageBoxA(GetForegroundWindow(),sqlite3_errmsg(hCheckerDB),NULL,MB_ICONEXCLAMATION);
                    }
                }
                DB_CommitTransaction(hCheckerDB);

                if (bOk)
                {
                    sqlite3_exec(hCheckerDB,"VACUUM;",NULL,NULL,NULL);
                    Checker_UpdateLastId(0);
                }
            }
        }

        sqlite3_exec(hCheckerDB,"PRAGMA optimize;",NULL,NULL,NULL);
        DB_Close(hCheckerDB);
        hCheckerDB=NULL;
    }

    if (hWorkerDB)
    {
        DB_CloseAllStmts(hWorkerDB);

        if (bClearFlagsOnExit)
            sqlite3_exec(hWorkerDB,"UPDATE bad SET flg=0 WHERE flg!=0",NULL,NULL,NULL);

        bool bVacuum=false;
        if (dwWorkerModsCount >= REBUILD_REASONS_COUNT)
        {
            Splash_SetText(_T("Workers table rebuilding..."));

            bVacuum=true;
            DB_BeginTransaction(hWorkerDB);
            {
                LPCSTR lpRebuild="DROP TABLE IF EXISTS new_bad;"
                                 "CREATE TABLE new_bad(id INTEGER NOT NULL UNIQUE,ip TEXT NOT NULL,port INTEGER NOT NULL DEFAULT 5900,idx INTEGER DEFAULT 0,flg INTEGER DEFAULT 0,PRIMARY KEY(id AUTOINCREMENT),UNIQUE(ip,port));"
                                 "INSERT INTO new_bad(ip,port,idx,flg) SELECT ip,port,idx,flg FROM bad ORDER BY ip,port;"
                                 "DROP TABLE bad;"
                                 "ALTER TABLE new_bad RENAME TO bad;";

                if (sqlite3_exec(hWorkerDB,lpRebuild,NULL,NULL,NULL) != SQLITE_OK)
                {
                    bVacuum=false;
                    MessageBoxA(GetForegroundWindow(),sqlite3_errmsg(hWorkerDB),NULL,MB_ICONEXCLAMATION);
                }
            }
            DB_CommitTransaction(hWorkerDB);

        }

        if (dwTotalIPsModsCount >= REBUILD_REASONS_COUNT)
        {
            Splash_SetText(_T("Total IPs table rebuilding..."));

            bVacuum=true;
            DB_BeginTransaction(hWorkerDB);
            {
                LPCSTR lpRebuild="DROP TABLE IF EXISTS new_total_ips;"
                                 "CREATE TABLE new_total_ips(ip TEXT NOT NULL,port INTEGER NOT NULL,UNIQUE(ip,port));"
                                 "INSERT INTO new_total_ips(ip,port) SELECT ip,port FROM total_ips ORDER BY ip,port;"
                                 "DROP TABLE total_ips;"
                                 "ALTER TABLE new_total_ips RENAME TO total_ips;";

                if (sqlite3_exec(hWorkerDB,lpRebuild,NULL,NULL,NULL) != SQLITE_OK)
                {
                    bVacuum=false;
                    MessageBoxA(GetForegroundWindow(),sqlite3_errmsg(hWorkerDB),NULL,MB_ICONEXCLAMATION);
                }
            }
            DB_CommitTransaction(hWorkerDB);
        }

        if (bVacuum)
            sqlite3_exec(hWorkerDB,"VACUUM;",NULL,NULL,NULL);

        sqlite3_exec(hWorkerDB,"PRAGMA optimize;",NULL,NULL,NULL);
        DB_Close(hWorkerDB);
        hWorkerDB=NULL;
    }

    sqlite3_shutdown();
    return;
}

