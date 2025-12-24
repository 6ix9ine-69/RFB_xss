#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

typedef struct _QUEUE_ITEM
{
    DWORD dwIP;
    WORD wPort;
    CHECKER_RESULT dwResult;

    _QUEUE_ITEM *lpNext;
} QUEUE_ITEM, *PQUEUE_ITEM;

void WINAPI IOCP_ResultsQueueThread(LPVOID);

void IOCP_QueueResult(DWORD dwIP,WORD wPort,CHECKER_RESULT dwResult);
cJSON *IOCP_QueueGetUnsentResults();

#endif // QUEUE_H_INCLUDED
