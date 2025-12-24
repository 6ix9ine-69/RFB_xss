#include <windows.h>
#include <tchar.h>

#include <cjson\cjson.h>
#include <syslib\mem.h>

#include "socket.h"

extern HANDLE hShutdownEvent;

void JSON_SendPacket(PSOCKET_INFO lpSock,cJSON *jsPacket)
{
    EnterCriticalSection(&lpSock->csSocket);
    {
        do
        {
            if (lpSock->hSock == INVALID_SOCKET)
                break;

            if (!lpSock->bConnected)
                break;

            LPSTR lpPacket=cJSON_PrintUnformatted(jsPacket);
            if (!lpPacket)
                break;

            DWORD dwLen=lstrlenA(lpPacket)+1,
                  dwDataSize=dwLen+sizeof(dwLen);
            if (lpSock->dwSendBufSize < dwDataSize)
            {
                lpSock->lpSendBuf=(PCHAR)MemRealloc(lpSock->lpSendBuf,dwDataSize);
                if (!lpSock->lpSendBuf)
                    OutputDebugString(_T("lpSock->lpSendBuf == null")); /// !!!

                lpSock->dwSendBufSize=dwDataSize;
            }

            *(LPDWORD)lpSock->lpSendBuf=dwLen;
            memcpy(&lpSock->lpSendBuf[sizeof(dwLen)],lpPacket,dwLen);
            while (dwDataSize)
            {
                int iSent=send(lpSock->hSock,lpSock->lpSendBuf,dwDataSize,0);
                if (iSent == SOCKET_ERROR)
                {
                    if (WSAGetLastError() != WSAEWOULDBLOCK)
                        break;

                    if (WaitForSingleObject(hShutdownEvent,1) == WAIT_OBJECT_0)
                        break;

                    continue;
                }
                dwDataSize-=iSent;
            }
            MemFree(lpPacket);
        }
        while (false);
    }
    LeaveCriticalSection(&lpSock->csSocket);
    return;
}

void JSON_SendPacketAndFree(PSOCKET_INFO lpSock,cJSON *jsPacket)
{
    JSON_SendPacket(lpSock,jsPacket);
    cJSON_Delete(jsPacket);
    return;
}

