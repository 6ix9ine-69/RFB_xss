#include "includes.h"

static CRITICAL_SECTION csQueue;
static PQUEUE_ITEM lpQueue=NULL,lpUnsentQueue=NULL;

static cJSON *IOCP_GetQueuedResult(PQUEUE_ITEM lpItem)
{
    cJSON *jsResult=cJSON_CreateObject();
    if (jsResult)
    {
        cJSON_AddIntToObject(jsResult,"ip",lpItem->dwIP);
        cJSON_AddIntToObject(jsResult,"port",lpItem->wPort);
        cJSON_AddIntToObject(jsResult,"result",lpItem->dwResult);
    }
    return jsResult;
}

void WINAPI IOCP_ResultsQueueThread(LPVOID)
{
    InitializeCriticalSection(&csQueue);
    lpQueue=NULL;

// FIXME (Гость#1#): поправить логику. реагировать на IOCP_IsStopped() надо один раз и ждать сброса флага
// отправляем результаты и ждем пока будет resume или end

    while (true)
    {
        bool bEnd=(WaitForSingleObject(hShutdownEvent,1) == WAIT_OBJECT_0);
        if ((bEnd) || (IOCP_IsStopped()))
        {
            if (IsActiveTasks())
            {
                Sleep(1);
                continue;
            }
        }

        DWORD dwResults=0;
        PQUEUE_ITEM lpItem=NULL;
        if (lpQueue)
        {
            EnterCriticalSection(&csQueue);
            {
                lpItem=lpQueue;
                lpQueue=NULL;
            }
            LeaveCriticalSection(&csQueue);

            if (siManager.bConnected)
            {
                cJSON *jsResults=cJSON_CreateArray();
                do
                {
                    cJSON *jsResult=IOCP_GetQueuedResult(lpItem);
                    if (jsResult)
                    {
                        cJSON_AddItemToArray(jsResults,jsResult);
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
                PQUEUE_ITEM lpLastQueue=lpItem;
                while (lpLastQueue->lpNext)
                    lpLastQueue=lpLastQueue->lpNext;

                EnterCriticalSection(&csQueue);
                {
                    lpLastQueue->lpNext=lpUnsentQueue;
                    lpUnsentQueue=lpItem;
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

void IOCP_QueueResult(DWORD dwIP,WORD wPort,CHECKER_RESULT dwResult)
{
    PQUEUE_ITEM lpItem=(PQUEUE_ITEM)MemQuickAlloc(sizeof(*lpQueue));
    if (lpItem)
    {
        lpItem->dwIP=dwIP;
        lpItem->wPort=wPort;
        lpItem->dwResult=dwResult;

        EnterCriticalSection(&csQueue);
        {
            lpItem->lpNext=lpQueue;
            lpQueue=lpItem;
        }
        LeaveCriticalSection(&csQueue);
    }
    return;
}

