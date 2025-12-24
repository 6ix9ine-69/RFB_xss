#ifndef IOCP_H_INCLUDED
#define IOCP_H_INCLUDED

#include "rfb_proto.h"

#define IOCP_BUFFER_SIZE 512

enum IOCPOL_CONN_STATE
{
    STATE_CONNECTED,
    STATE_DATA_RECEIVED,
    STATE_DATA_SENT,
};

typedef struct _IOCPOL_IO_CTX
{
    OVERLAPPED olOverlapped;

    DWORD dwOperationsCount;

    IOCPOL_CONN_STATE dwConnState;
    WSABUF wsaBuf;
    DWORD dwBytesToProcess;
    DWORD dwBytesProcessed;

    PCHAR lpSendBuffer;

    _IOCPOL_IO_CTX *lpPrev;
    _IOCPOL_IO_CTX *lpNext;
} IOCPOL_IO_CTX, *PIOCPOL_IO_CTX;

typedef struct _IOCP_CLIENT
{
    DWORD dwLastActivityTime;

    char cRecvBuf[IOCP_BUFFER_SIZE];
    IOCPOL_IO_CTX ovl;

    CRITICAL_SECTION csClient;
    CRITICAL_SECTION csSendList;
    PIOCPOL_IO_CTX lpOvlSend;

    SOCKET hSock;
    sockaddr_in caddr;
    bool bTooManyAuthErr;
    bool bInUse;
    bool bClosing;
    bool bResultQueued;

    RFB_CLIENT RfbClient;

    DWORD dwSleepUntil;

    _IOCP_CLIENT *lpNext;
    _IOCP_CLIENT *lpPrev;
} IOCP_CLIENT, *PIOCP_CLIENT;

#define DEFAULT_TIMEOUT 40
#define DEFAULT_MAX_CONNECTIONS 5000
#define DEFAULT_THREADS_PER_CORE 2

bool IOCP_Init(cJSON *jsConfig);
void IOCP_Stop();

bool IOCP_RecvData(PIOCP_CLIENT lpClient,DWORD dwDataSize);
bool IOCP_RecvMoreData(PIOCP_CLIENT lpClient,DWORD dwDataSize);
bool IOCP_SendData(PIOCP_CLIENT lpClient,PCHAR lpData,DWORD dwDataSize);
bool IOCP_AddAddressToBruteList(DWORD dwIP,WORD wPort,DWORD dwIdx);

extern DWORD dwTotalActiveTasks;
extern HANDLE hShutdownEvent;

extern volatile DWORD dwActiveTasks,bIOCP_StopWork,dwUnsupported,dwBrutted,dwNotRFB;

#define IOCP_AddNextIPToBruteList(lpClient) {\
                                                IOCP_Disconnect(lpClient);\
                                                lpClient->RfbClient.bGetOutOfHere=true;\
                                            }

#define IOCP_IsStopped() (InterlockedExchangeAdd(&bIOCP_StopWork,0) != 0)

#define RFB_BRUTE_TIME_DELTA 10
#define RFB_BRUTE_TIME_DELTA_RECONNECT 60

#endif // IOCP_H_INCLUDED
