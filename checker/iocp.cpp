#include "includes.h"
#include <shlwapi.h>
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

HANDLE hShutdownEvent;
static HANDLE hCompletionPort,hThreadsGroup,hMainThread;
static DWORD dwWorkingThreads,
             dwConnectionTimeout=DEFAULT_TIMEOUT,
             dwThreadsPerCore=DEFAULT_THREADS_PER_CORE,
             dwRetryCount=DEFAULT_RETRY_COUNT;

static LPFN_CONNECTEX pConnectEx;

static CRITICAL_SECTION csClients;
static PIOCP_CLIENT lpClients;

volatile DWORD dwActiveTasks=0,bIOCP_StopWork=1,dwChecked=0,dwFailed=0,dwRFB=0,dwNotRFB=0;

static void Result_Failed(PIOCP_CLIENT lpClient)
{
    if (lpClient->bResultQueued)
        return;

    if (!lpClient->bInUse)
        return;

    InterlockedIncrement(&dwFailed);

    IOCP_QueueResult(lpClient->caddr.sin_addr.s_addr,ntohs(lpClient->caddr.sin_port),CKR_RESULT_FAILED);
    lpClient->bResultQueued=true;
    return;
}

static void Result_RFB(PIOCP_CLIENT lpClient)
{
    if (lpClient->bResultQueued)
        return;

    if (!lpClient->bInUse)
        return;

    InterlockedIncrement(&dwRFB);

    IOCP_QueueResult(lpClient->caddr.sin_addr.s_addr,ntohs(lpClient->caddr.sin_port),CKR_RESULT_RFB);
    lpClient->bResultQueued=true;
    return;
}

static void Result_NotRFB(PIOCP_CLIENT lpClient)
{
    if (lpClient->bResultQueued)
        return;

    if (!lpClient->bInUse)
        return;

    InterlockedIncrement(&dwNotRFB);

    IOCP_QueueResult(lpClient->caddr.sin_addr.s_addr,ntohs(lpClient->caddr.sin_port),CKR_RESULT_NOT_RFB);
    lpClient->bResultQueued=true;
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

static bool IOCP_ReNewClient(PIOCP_CLIENT lpClient)
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

        lpClient->ovl.dwConnState=STATE_NONE;
        lpClient->ovl.dwOperationsCount=0;

        lpClient->dwAttempt++;

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

        lpClient->ovl.wsaBuf.len=sizeof(lpClient->cRecvBuf);
        lpClient->ovl.wsaBuf.buf=lpClient->cRecvBuf;
        lpClient->ovl.dwConnState=STATE_NONE;

        InitializeCriticalSection(&lpClient->csClient);

        bRet=true;
    }
    while (false);
    return bRet;
}

static bool IOCP_Connect(PIOCP_CLIENT lpClient,LPDWORD lpdwLastError)
{
    bool bRet=((pConnectEx(lpClient->hSock,(PSOCKADDR)&lpClient->caddr,sizeof(lpClient->caddr),NULL,0,NULL,&lpClient->ovl.olOverlapped) != false) || (WSAGetLastError() == ERROR_IO_PENDING));

    *lpdwLastError=WSAGetLastError();
    InterlockedExchange(&lpClient->dwLastActivityTime,Now());
    return bRet;
}

static void IOCP_DisconnectInt(PIOCP_CLIENT lpClient)
{
    if (lpClient->hSock != INVALID_SOCKET)
    {
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

static void IOCP_FreeClient(PIOCP_CLIENT lpClient)
{
    EnterCriticalSection(&lpClient->csClient);
    {
        if (lpClient->bInUse)
        {
            InterlockedIncrement(&dwChecked);
            InterlockedDecrement(&dwActiveTasks);

            IOCP_DisconnectInt(lpClient);

            lpClient->caddr.sin_addr.s_addr=0;
            lpClient->caddr.sin_port=0;
            lpClient->bResultQueued=false;

            IOCP_ReNewClient(lpClient);
            lpClient->bInUse=false;
        }
    }
    LeaveCriticalSection(&lpClient->csClient);
    return;
}

bool IOCP_AddAddressToCheckList(DWORD dwIP,WORD wPort)
{
    bool bRet=false;
    if (!IOCP_IsStopped())
    {
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

                            IOCP_ReNewClient(lpClient);
                        }

                        lpClient->dwAttempt=0;

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

                            IOCP_ReNewClient(lpClient);
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
            if (WaitForSingleObject(hShutdownEvent,1) != WAIT_OBJECT_0)
                continue;
        }

        bRet=false;
        break;
    }
    return bRet;
}

static bool IOCP_RecvData(PIOCP_CLIENT lpClient,DWORD dwSize)
{
    if (lpClient->hSock == INVALID_SOCKET)
        return false;

    bool bRet=false;
    if (!InterlockedExchangeAdd(&lpClient->ovl.dwOperationsCount,0))
    {
        PIOCPOL_IO_CTX lpOverlap=&lpClient->ovl;

        lpOverlap->dwConnState=STATE_DATA_RECEIVED;
        lpOverlap->dwBytesProcessed=0;
        lpOverlap->dwBytesToProcess=dwSize;

        InterlockedIncrement(&lpClient->ovl.dwOperationsCount);

        bRet=IOCP_RecvClientDataInt(lpClient);
        if (!bRet)
            InterlockedDecrement(&lpClient->ovl.dwOperationsCount);
    }
    return bRet;
}

static bool IOCP_Reconnect(PIOCP_CLIENT lpClient)
{
    bool bRet=false;
    EnterCriticalSection(&lpClient->csClient);
    {
        do
        {
            if (IOCP_IsStopped())
                break;

            if (lpClient->dwAttempt >= dwRetryCount)
                break;

            IOCP_DisconnectInt(lpClient);

            if (!IOCP_ReNewClient(lpClient))
                break;

            DWORD dwLastError;
            if (!IOCP_Connect(lpClient,&dwLastError))
            {
                DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"IOCP_Connect(%x) failed, %d",lpClient->hSock,dwLastError);
                break;
            }

            bRet=true;
        }
        while (false);
    }
    LeaveCriticalSection(&lpClient->csClient);
    return bRet;
}

static void IOCP_HandleIO(PIOCP_CLIENT lpClient,PIOCPOL_IO_CTX lpOverlapped,DWORD dwBytesTransfered,DWORD dwError)
{
    if ((dwError != NO_ERROR) || ((!dwBytesTransfered) && (lpOverlapped->dwConnState != STATE_NONE)))
    {
        if (lpOverlapped->dwConnState == STATE_DATA_RECEIVED)
            InterlockedDecrement(&lpOverlapped->dwOperationsCount);

        if (!IOCP_Reconnect(lpClient))
        {
            Result_Failed(lpClient);
            IOCP_FreeClient(lpClient);
        }
        return;
    }

    InterlockedExchange(&lpClient->dwLastActivityTime,Now());

    switch (lpOverlapped->dwConnState)
    {
        case STATE_NONE:
        {
            setsockopt(lpClient->hSock,SOL_SOCKET,SO_UPDATE_CONNECT_CONTEXT,(PCHAR)&lpClient->hSock,sizeof(lpClient->hSock));

            lpOverlapped->dwConnState=STATE_CONNECTED;
            IOCP_RecvData(lpClient,0);
            break;
        }
        case STATE_DATA_RECEIVED:
        {
            if (lpClient->hSock == INVALID_SOCKET)
            {
                InterlockedDecrement(&lpClient->ovl.dwOperationsCount);
                break;
            }

            InterlockedDecrement(&lpClient->ovl.dwOperationsCount);

            if ((dwBytesTransfered < RFB_BANNER_SIZE) || (StrCmpNA(lpClient->cRecvBuf,"RFB ",4)))
                Result_NotRFB(lpClient);
            else
                Result_RFB(lpClient);

            IOCP_FreeClient(lpClient);
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

static void IOCP_FreeClientIfNeeded(PIOCP_CLIENT lpClient)
{
    bool bFree=true;
    do
    {
        if (IOCP_IsStopped())
            break;

        if (lpClient->dwAttempt >= dwRetryCount)
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

                    if ((Now()-InterlockedExchangeAdd(&lpClient->dwLastActivityTime,0)) < dwConnectionTimeout)
                        break;

                    // timeout, reconnect
                    IOCP_Disconnect(lpClient);
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
                    IOCP_FreeClient(lpClient);

                    bDone=false;
                    break;
                }

                lpClient=lpClient->lpNext;
            }
        }
        while (!bDone);
    }

    for (DWORD i=0; i < dwWorkingThreads; i++)
        PostQueuedCompletionStatus(hCompletionPort,0,NULL,NULL);

    ThreadsGroup_WaitForAllExit(hThreadsGroup,INFINITE);
    ThreadsGroup_CloseGroup(hThreadsGroup);

    dwActiveTasks=0;
    return;
}

void IOCP_Stop()
{
    SetEvent(hShutdownEvent);
    WaitForSingleObject(hMainThread,INFINITE);
    CloseHandle(hShutdownEvent);
    CloseHandle(hMainThread);
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

        dwRetryCount=(DWORD)cJSON_GetIntFromObject(jsConfig,"retry_count");
        if (!dwRetryCount)
            dwRetryCount=DEFAULT_RETRY_COUNT;

        SYSTEM_INFO si;
        GetSystemInfo(&si);
        dwWorkingThreads=si.dwNumberOfProcessors*dwThreadsPerCore;

        InitializeCriticalSection(&csClients);
        hThreadsGroup=ThreadsGroup_Create();
        hShutdownEvent=CreateEvent(NULL,true,false,NULL);

        for (DWORD i=0; i < dwTotalActiveTasks; i++)
        {
            if (IOCP_CreateClient())
                continue;

            dwTotalActiveTasks=i;
            break;
        }

        ThreadsGroup_CreateThread(hThreadsGroup,0,(LPTHREAD_START_ROUTINE)IOCP_ResultsQueueThread,NULL,NULL,NULL);

        for (DWORD i=0; i < dwWorkingThreads; i++)
            ThreadsGroup_CreateThread(hThreadsGroup,0,(LPTHREAD_START_ROUTINE)IOCP_WorkerThread,NULL,NULL,NULL);

        hMainThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)IOCP_MainThread,NULL,0,NULL);

        bRet=true;
    }
    while (false);
    return bRet;
}

