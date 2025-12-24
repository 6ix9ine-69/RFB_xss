#ifndef SOCKET_H_INCLUDED
#define SOCKET_H_INCLUDED

SOCKET SocketBindOnPort(WORD wPort,DWORD dwIP);

typedef struct _SOCKET_INFO
{
    sockaddr_in si;

    SOCKET hSock;
    bool bConnected;
    CRITICAL_SECTION csSocket;

    PCHAR lpRecvBuf;
    DWORD dwRecvBufSize;

    PCHAR lpSendBuf;
    DWORD dwSendBufSize;

    DWORD dwNeedToRecv;
    DWORD dwReceived;
} SOCKET_INFO, *PSOCKET_INFO;

#endif // SOCKET_H_INCLUDED
