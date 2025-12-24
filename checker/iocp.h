#ifndef IOCP_H_INCLUDED
#define IOCP_H_INCLUDED

#define IOCP_BUFFER_SIZE 512

enum IOCPOL_CONN_STATE
{
    STATE_NONE,
    STATE_CONNECTED,
    STATE_DATA_RECEIVED
};

#include <pshpack1.h>
    typedef struct _IOCPOL_IO_CTX
    {
        OVERLAPPED olOverlapped;

        DWORD dwOperationsCount;

        IOCPOL_CONN_STATE dwConnState;
        WSABUF wsaBuf;
        DWORD dwBytesToProcess;
        DWORD dwBytesProcessed;
    } IOCPOL_IO_CTX, *PIOCPOL_IO_CTX;

    typedef struct _IOCP_CLIENT
    {
        DWORD dwLastActivityTime;

        char cRecvBuf[IOCP_BUFFER_SIZE];
        IOCPOL_IO_CTX ovl;

        CRITICAL_SECTION csClient;

        SOCKET hSock;
        sockaddr_in caddr;
        bool bInUse;
        bool bResultQueued;

        DWORD dwAttempt;

        _IOCP_CLIENT *lpNext;
        _IOCP_CLIENT *lpPrev;
    } IOCP_CLIENT, *PIOCP_CLIENT;
#include <poppack.h>

bool IOCP_AddAddressToCheckList(DWORD dwIP,WORD wPort);

void IOCP_Stop();
bool IOCP_Init(cJSON *jsConfig);

#define DEFAULT_TIMEOUT 40
#define DEFAULT_RETRY_COUNT 3
#define DEFAULT_MAX_CONNECTIONS 5000
#define DEFAULT_THREADS_PER_CORE 2

extern DWORD dwTotalActiveTasks;
extern HANDLE hShutdownEvent;

extern volatile DWORD dwActiveTasks,bIOCP_StopWork,dwChecked,dwFailed,dwRFB,dwNotRFB;

#define RFB_BANNER_SIZE 12

#define IOCP_IsStopped() (InterlockedExchangeAdd(&bIOCP_StopWork,0) != 0)

#endif // IOCP_H_INCLUDED
