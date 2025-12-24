#ifndef CONNECTIONS_H_INCLUDED
#define CONNECTIONS_H_INCLUDED

#include "common\socket.h"
#include "list.h"

enum CONNECTION_TYPE
{
    TYPE_NONE=-1,
    TYPE_WORKER,
    TYPE_CHECKER
};

typedef struct _WORKER_STAT
{
    DWORD dwBrutted;
    DWORD dwUnsupported;
} WORKER_STAT, *PWORKER_STAT;

typedef struct _CHECKER_STAT
{
    DWORD dwChecked;
    DWORD dwFailed;
    DWORD dwRFB;
    DWORD dwNotRFB;
} CHECKER_STAT, *PCHECKER_STAT;

typedef struct _STATS
{
    DWORD dwActiveTask;
    union
    {
        CHECKER_STAT cs;
        WORKER_STAT ws;
    };
} STATS, *PSTATS;

enum CONN_STATUS
{
    STATUS_STARTED,
    STATUS_STOPPING,
    STATUS_STOPPED
};

typedef struct _CONNECTION
{
    _CONNECTION *lpNext;

    PCOMMON_LIST lpAssigned;
    PCOMMON_LIST lpLastAssigned;

    HWND hDlg;
    HWND hParent;
    CONNECTION_TYPE dwType;
    HTREEITEM hItem;
    DWORD dwLastActivityTime;
    bool bActive;
    DWORD dwCmdParsingThreadId;

    char szAlias[MAX_PATH];
    SOCKET_INFO Socket;

    STATS Sts;
    STATS PrevSts;
    DWORD dwLastStatUpdate;

    DWORD dwPrevStatus;

    CONN_STATUS dwStatus;
    bool bResultReceived;
    bool bNA;

    DWORD dwIdx;
    bool bRoot;
    bool bInsideItemChange;

    cJSON *jsItem;
} CONNECTION, *PCONNECTION;

PCONNECTION Connection_Add(cJSON *jsItem,LPCSTR lpAddress,LPCSTR lpAlias,CONNECTION_TYPE dwType);
PCONNECTION Connection_AddRoot(CONNECTION_TYPE dwType);

extern PCONNECTION lpConnections,lpVoidConnection;

void Connections_Init();

#endif // CONNECTIONS_H_INCLUDED
