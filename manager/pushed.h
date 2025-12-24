#ifndef PUSHED_H_INCLUDED
#define PUSHED_H_INCLUDED

typedef struct _NOTIFY
{
    char szContent[300];
    _NOTIFY *lpNext;
} NOTIFY, *PNOTIFY;

void Notify_Init(cJSON *jsConfig);

void Notify_SendFound(LPCSTR lpDesktop,DWORD dwIdx,LPCSTR lpPassword);
void Notify_SendOffline(LPCSTR lpWorker);

#endif // PUSHED_H_INCLUDED
