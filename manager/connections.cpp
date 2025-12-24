#include "includes.h"

static CONNECTION VoidConnection={0};
PCONNECTION lpConnections=NULL,lpVoidConnection=&VoidConnection;
static CRITICAL_SECTION csConnections;
static PCONNECTION lpLastConnection=NULL;

static PCONNECTION Connection_AddInt(cJSON *jsItem,DWORD dwIP,LPCSTR lpAlias,CONNECTION_TYPE dwType)
{
    PCONNECTION lpConnection=(PCONNECTION)MemAlloc(sizeof(*lpConnection));
    if (lpConnection)
    {
        InitializeCriticalSection(&lpConnection->Socket.csSocket);

        lpConnection->dwType=dwType;
        lpConnection->Socket.si.sin_family=AF_INET;
        lpConnection->Socket.si.sin_addr.s_addr=dwIP;
        lpConnection->Socket.hSock=INVALID_SOCKET;
        lstrcpyA(lpConnection->szAlias,lpAlias);
        lpConnection->jsItem=jsItem;

        EnterCriticalSection(&csConnections);
        {
            if (lpLastConnection)
                lpLastConnection->lpNext=lpConnection;
            else
                lpConnections=lpConnection;

            lpLastConnection=lpConnection;
        }
        LeaveCriticalSection(&csConnections);
    }
    return lpConnection;
}

PCONNECTION Connection_Add(cJSON *jsItem,LPCSTR lpAddress,LPCSTR lpAlias,CONNECTION_TYPE dwType)
{
    PCONNECTION lpConnection=NULL;

    DWORD dwIP=NetResolveAddress(lpAddress);
    if ((dwIP) && (dwIP != INADDR_NONE))
        lpConnection=Connection_AddInt(jsItem,dwIP,lpAlias,dwType);

    return lpConnection;
}

PCONNECTION Connection_AddRoot(CONNECTION_TYPE dwType)
{
    PCONNECTION lpConnection=Connection_AddInt(NULL,0,NULL,dwType);
    if (lpConnection)
        lpConnection->bRoot=true;

    return lpConnection;
}

void Connections_Init()
{
    InitializeCriticalSection(&csConnections);
    return;
}

