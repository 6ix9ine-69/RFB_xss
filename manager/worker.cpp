#include "includes.h"
#include "mergesort.h"
#include "commands.h"

volatile DWORD dwActiveServers=0,dwWorkersCount=0,dwWorkersTasks=0,
               dwItemsToBrute=0,dwBrutted=0,dwUnsupported=0,dwWorkerItemsUsed=0;

static PRFB_SERVER lpItems=NULL,lpLastItem=NULL,lpLastUsedItem=NULL,lpFirstNewItemAdded=NULL;
static CRITICAL_SECTION csBrute;
static volatile DWORD bCanBeResorted=0;

void Worker_AppendLog(PCONNECTION lpConnection,LPCSTR lpTime,LPCSTR lpAddress,LPCSTR lpDesktop,DWORD dwIdx,LPCSTR lpPassword)
{
    LVITEMA lvi={0};
    lvi.mask=LVIF_TEXT;

    char szItem[513];
    lvi.pszText=szItem;
    lvi.cchTextMax=lstrlenA(lstrcpyA(szItem,lpTime));
    SendDlgItemMessageA(lpConnection->hDlg,IDC_FOUND_LOG,LVM_INSERTITEMA,NULL,(LPARAM)&lvi);

    lvi.iSubItem++;
    lvi.cchTextMax=lstrlenA(lstrcpyA(szItem,lpAddress));
    SendDlgItemMessageA(lpConnection->hDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

    lvi.iSubItem++;
    LPCSTR lpDesk="[empty]";
    if ((lpDesktop) && (lpDesktop[0]))
        lpDesk=lpDesktop;

    lvi.cchTextMax=lstrlenA(lstrcpyA(szItem,lpDesk));
    SendDlgItemMessageA(lpConnection->hDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

    lvi.iSubItem++;
    lvi.cchTextMax=wsprintfA(szItem,"%d",dwIdx);
    SendDlgItemMessageA(lpConnection->hDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

    lvi.iSubItem++;
    lvi.cchTextMax=lstrlenA(lstrcpyA(szItem,lpPassword));
    SendDlgItemMessageA(lpConnection->hDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

    SetItemBold(lpConnection);
    return;
}

static void Worker_FreeServerInt(PRFB_SERVER lpItem)
{
    PCONNECTION lpConnection=lpItem->Assigned.lpAssignedTo;
    if (lpConnection)
        List_UnAssign(lpItem);
    return;
}

void Worker_FreeServer(PRFB_SERVER lpItem)
{
    EnterCriticalSection(&csBrute);
        Worker_FreeServerInt(lpItem);
    LeaveCriticalSection(&csBrute);
    return;
}

void Worker_FreeConnection(PCONNECTION lpConnection)
{
    EnterCriticalSection(&csBrute);
    {
        while (lpConnection->lpAssigned)
            Worker_FreeServerInt((PRFB_SERVER)lpConnection->lpAssigned);
    }
    LeaveCriticalSection(&csBrute);
    return;
}

static PRFB_SERVER Worker_Find(PCONNECTION lpConnection,DWORD dwIP,WORD wPort,bool bSilent)
{
    if ((!dwIP) || (!wPort))
        return NULL;

    PRFB_SERVER lpItem=(PRFB_SERVER)lpConnection->lpAssigned;
    while (lpItem)
    {
        if ((lpItem->dwIP == dwIP) && (lpItem->wPort == wPort))
            break;

        lpItem=(PRFB_SERVER)lpItem->Assigned.lpNext;
    }

    if (!lpItem)
    {
        if (!bSilent)
            DebugLog_AddItem2(lpConnection,LOG_LEVEL_WARNING,"Worker_FindAssigned(\"%s\",%d) failed",NetNtoA(dwIP),wPort);

        lpItem=lpItems;
        while (lpItem)
        {
            if ((lpItem->dwIP == dwIP) && (lpItem->wPort == wPort))
                break;

            lpItem=(PRFB_SERVER)lpItem->lpNext;
        }

        if ((!lpItem) && (!bSilent))
            DebugLog_AddItem2(lpConnection,LOG_LEVEL_WARNING,"Worker_FindGlobal(\"%s\",%d) failed",NetNtoA(dwIP),wPort);
    }
    return lpItem;
}

PRFB_SERVER Worker_FindServer(PCONNECTION lpConnection,DWORD dwIP,WORD wPort,bool bSilent)
{
    PRFB_SERVER lpItem=NULL;
    EnterCriticalSection(&csBrute);
        lpItem=Worker_Find(lpConnection,dwIP,wPort,bSilent);
    LeaveCriticalSection(&csBrute);
    return lpItem;
}

static void Worker_RemoveBadInt(PRFB_SERVER lpItem)
{
    if (lpItem == lpFirstNewItemAdded)
        lpFirstNewItemAdded=(PRFB_SERVER)lpItem->lpNext;
    else if (lpItem == lpLastUsedItem)
        lpLastUsedItem=(PRFB_SERVER)lpItem->lpNext;

    Worker_FreeServerInt(lpItem);
    List_RemoveItem(lpItem,(PCOMMON_LIST*)&lpItems,(PCOMMON_LIST*)&lpLastItem);

    MemFree(lpItem);
    return;
}

void Worker_RemoveBad(PCONNECTION lpConnection,DWORD dwIP,WORD wPort)
{
    EnterCriticalSection(&csBrute);
    {
        PRFB_SERVER lpItem=Worker_Find(lpConnection,dwIP,wPort,false);
        if (lpItem)
            Worker_RemoveBadInt(lpItem);
    }
    LeaveCriticalSection(&csBrute);
    return;
}

bool Worker_AddItemForRescan(PRFB_SERVER *lppUnsupportedItems,PRFB_SERVER *lppLastUnsupportedItem,DWORD dwIP,WORD wPort,DWORD dwIdx)
{
    bool bRet=false;

    PRFB_SERVER lpItem=(PRFB_SERVER)MemAlloc(sizeof(*lpItem));
    if (lpItem)
    {
        InterlockedIncrement(&dwItemsToBrute);

        lpItem->dwIP=dwIP;
        lpItem->wPort=wPort;

        if (dwIdx)
            dwIdx--;

        lpItem->dwIdx=dwIdx;

        if (!*lppUnsupportedItems)
            *lppUnsupportedItems=lpItem;

        List_InsertItem(lpItem,(PCOMMON_LIST*)lppUnsupportedItems,(PCOMMON_LIST*)lppLastUnsupportedItem);

        bRet=true;
    }
    return bRet;
}

void Worker_RescanUnsupported()
{
    PRFB_SERVER lpTail=NULL,lpTailLastItem=NULL;
    DWORD dwAdded=DB_Worker_RescanUnsupported(&lpTail,&lpTailLastItem);

    if (dwAdded)
    {
        EnterCriticalSection(&csBrute);
        {
            List_Merge((PCOMMON_LIST*)&lpItems,(PCOMMON_LIST*)&lpLastItem,(PCOMMON_LIST)lpTail,(PCOMMON_LIST)lpTailLastItem);

            if (!lpFirstNewItemAdded)
                lpFirstNewItemAdded=lpTail;

            DebugLog_AddItem2(NULL,LOG_LEVEL_INFO,"New items added: %d",dwAdded);
        }
        LeaveCriticalSection(&csBrute);
    }
    return;
}

static bool Worker_Cmp(PDOUBLY_LINKED_LIST lpFirst,PDOUBLY_LINKED_LIST lpSecond)
{
    return (((PRFB_SERVER)lpFirst)->dwLastActivityTime < ((PRFB_SERVER)lpSecond)->dwLastActivityTime);
}

static DWORD dwLastResortTime;
static void Worker_PrepareList()
{
    if (lpItems)
    {
        PRFB_SERVER lpItem=lpItems;
        while (lpItem)
        {
            PRFB_SERVER lpNext=(PRFB_SERVER)lpItem->lpNext;

            if (!lpItem->bDontUseMe)
            {
                lpItem=lpNext;
                continue;
            }

            bool bFirst=(lpItem == lpItems);
            Worker_RemoveBadInt(lpItem);
            lpItem=lpNext;

            if (!bFirst)
                continue;

            lpItems=lpItem;
        }

        MergeSort((PDOUBLY_LINKED_LIST*)&lpItems,(PDOUBLY_LINKED_LIST*)&lpLastItem,Worker_Cmp);

        char szTime[MAX_PATH];
        CalcDHMS(InterlockedExchangeAdd(&dwLastResortTime,0),szTime);

        DebugLog_AddItem2(NULL,LOG_LEVEL_INFO,"Workers DB resorted after %s",szTime);

        InterlockedExchange(&dwLastResortTime,Now());
        InterlockedExchange(&dwWorkerItemsUsed,0);
    }
    return;
}

void Worker_ResortList()
{
    do
    {
        if (bStopping)
            break;

        if (InterlockedExchangeAdd(&dwWorkersTasks,0))
            break;

        if (!InterlockedExchange(&bCanBeResorted,0))
            break;

        if (lpLastUsedItem == lpItems)
            break;

        EnterCriticalSection(&csBrute);
        {
            lpFirstNewItemAdded=NULL;

            Worker_PrepareList();

            lpLastUsedItem=lpItems;
        }
        LeaveCriticalSection(&csBrute);

        /// при остановке обнуляем время сортировки
        InterlockedExchange(&dwLastResortTime,0);
    }
    while (false);
    return;
}

static PRFB_SERVER Worker_GetNextFromPosition(PCONNECTION lpConnection,PRFB_SERVER lpPosition)
{
    PRFB_SERVER lpItem=lpPosition;

    while (lpItem)
    {
        if ((!lpItem->bDontUseMe) && (!lpItem->Assigned.lpAssignedTo))
        {
            if ((Now()-lpItem->dwLastActivityTime) >= SERVER_SLEEP_TIME)
                break;
        }

        lpItem=(PRFB_SERVER)lpItem->lpNext;
    }
    return lpItem;
}

PRFB_SERVER Worker_GetNext(PCONNECTION lpConnection)
{
    PRFB_SERVER lpItem=NULL;
    EnterCriticalSection(&csBrute);
    {
        do
        {
            /// первый запрос, считать время будем отсюда
            if (!InterlockedExchangeAdd(&dwLastResortTime,0))
                InterlockedExchange(&dwLastResortTime,Now());

            lpFirstNewItemAdded=lpItem=Worker_GetNextFromPosition(lpConnection,lpFirstNewItemAdded);
            if (lpItem)
                break;

            lpLastUsedItem=lpItem=Worker_GetNextFromPosition(lpConnection,lpLastUsedItem);
            if (lpItem)
                break;

            if (!InterlockedExchange(&bCanBeResorted,0))
                break;

            Worker_PrepareList();

            lpLastUsedItem=lpItem=Worker_GetNextFromPosition(lpConnection,lpItems);
        }
        while (false);

        if (lpItem)
        {
            InterlockedIncrement(&dwWorkerItemsUsed);
            InterlockedExchange(&bCanBeResorted,1);

            lpItem->dwLastActivityTime=Now();
            List_AssignItem(lpConnection,lpItem);

            InterlockedIncrement(&dwActiveServers);
        }
    }
    LeaveCriticalSection(&csBrute);
    return lpItem;
}

void Worker_AddItem(DWORD dwIP,WORD wPort,DWORD dwIdx,bool bInit)
{
    PRFB_SERVER lpItem=(PRFB_SERVER)MemAlloc(sizeof(*lpItem));
    if (lpItem)
    {
        InterlockedIncrement(&dwItemsToBrute);

        lpItem->dwIP=dwIP;
        lpItem->wPort=wPort;

        if (dwIdx)
            dwIdx--;

        lpItem->dwIdx=dwIdx;

        EnterCriticalSection(&csBrute);
        {
            if (!lpItems)
                lpLastUsedItem=lpItem;

            List_InsertItem(lpItem,(PCOMMON_LIST*)&lpItems,(PCOMMON_LIST*)&lpLastItem);

            if ((!bInit) && (!lpFirstNewItemAdded))
                lpFirstNewItemAdded=lpItem;
        }
        LeaveCriticalSection(&csBrute);
    }
    return;
}

void Worker_Init(cJSON *jsConfig)
{
    do
    {
        Splash_SetText(_T("Initializing workers..."));

        InitializeCriticalSection(&csBrute);
        Connection_AddRoot(TYPE_WORKER);

        DB_Worker_Read();

        cJSON *jsWorkers=cJSON_GetObjectItem(jsConfig,"items");
        if (!jsWorkers)
            break;

        DWORD dwCount=cJSON_GetArraySize(jsWorkers);
        if (!dwCount)
            break;

        for (int i=0; i < dwCount; i++)
        {
            cJSON *jsItem=cJSON_GetArrayItem(jsWorkers,i);
            if (!jsItem)
                break;

            Connection_Add(jsItem,
                           cJSON_GetStringFromObject(jsItem,"address"),
                           cJSON_GetStringFromObject(jsItem,"alias"),
                           TYPE_WORKER);
        }
    }
    while (false);
    return;
}

