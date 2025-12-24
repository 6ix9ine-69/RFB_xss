#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

typedef struct _QUEUE_ITEM
{
    DWORD dwIP;
    WORD wPort;
    DWORD dwIdx;
    char szPassword[9];
    char szDesktop[512];
    WORKER_RESULT dwResult;

    _QUEUE_ITEM *lpNext;
} QUEUE_ITEM, *PQUEUE_ITEM;

void WINAPI IOCP_ResultsQueueThread(LPVOID);

void IOCP_QueueResultItem(PQUEUE_ITEM lpItem);
void IOCP_QueueResult(DWORD dwIP,WORD wPort,DWORD dwIdx,LPCSTR lpPassword,LPCSTR lpDesktop,WORKER_RESULT dwResult);
cJSON *IOCP_QueueGetUnsentResults();

#endif // QUEUE_H_INCLUDED
