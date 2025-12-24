#ifndef WORKER_H_INCLUDED
#define WORKER_H_INCLUDED

#include "list.h"

typedef struct _RFB_SERVER:_COMMON_LIST
{
    DWORD dwLastActivityTime;
    bool bDontUseMe;

    DWORD dwIP;
    WORD wPort;
    DWORD dwIdx;
    DWORD dwFlags;
} RFB_SERVER, *PRFB_SERVER;

void Worker_FreeConnection(PCONNECTION lpConnection);
PRFB_SERVER Worker_FindServer(PCONNECTION lpConnection,DWORD dwIP,WORD wPort,bool bSilent=false);
PRFB_SERVER Worker_GetNext(PCONNECTION lpConnection);
void Worker_RemoveBad(PCONNECTION lpConnection,DWORD dwIP,WORD wPort);
void Worker_FreeServer(PRFB_SERVER lpItem);

void Worker_AddItem(DWORD dwIP,WORD wPort,DWORD dwIdx,bool bInit);
bool Worker_AddItemForRescan(PRFB_SERVER *lppUnsupportedItems,PRFB_SERVER *lppLastUnsupportedItem,DWORD dwIP,WORD wPort,DWORD dwIdx);

void Worker_ResortList();

void Worker_RescanUnsupported();

void Worker_Init(cJSON *jsConfig);

extern volatile DWORD dwActiveServers,dwWorkersCount,dwWorkersTasks,dwItemsToBrute,dwBrutted,dwUnsupported,dwWorkerItemsUsed;

#define SERVER_SLEEP_TIME 60*5

void Worker_AppendLog(PCONNECTION lpConnection,LPCSTR lpTime,LPCSTR lpAddress,LPCSTR lpDesktop,DWORD dwIdx,LPCSTR lpPassword);

#endif // WORKER_H_INCLUDED
