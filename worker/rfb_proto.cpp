#include "includes.h"
#include <shlwapi.h>
#include <stdio.h>

#include "d3des.h"

static void rfbEncryptBytes(LPBYTE lpBytes,LPCSTR lpPasswd)
{
    DWORD dwPasswdLen=lstrlenA(lpPasswd);
    byte bKey[8];
    for (DWORD i=0; i < 8; i++)
    {
		if (i < dwPasswdLen)
		    bKey[i]=lpPasswd[i];
		 else
		    bKey[i]=0;
    }

    rfbDesKey(bKey,EN0);
    for (DWORD i=0; i < CHALLENGESIZE; i+=8)
		rfbDes(lpBytes+i,lpBytes+i);
    return;
}

static byte bFixedkey[8]={23,82,107,6,35,78,88,7};
static void rfbEncryptPasswd(LPBYTE lpEncryptedPasswd,LPCSTR lpPasswd)
{
    DWORD dwPasswdLen=lstrlenA(lpPasswd);
    for (DWORD i=0; i < MAXPWLEN; i++)
    {
		if (i < dwPasswdLen)
			lpEncryptedPasswd[i]=lpPasswd[i];
		else
			lpEncryptedPasswd[i]=0;
    }

    rfbDesKey(bFixedkey,EN0);
    rfbDes(lpEncryptedPasswd,lpEncryptedPasswd);
    return;
}

static void RFB_NeedMoreData(PIOCP_CLIENT lpClient,DWORD dwSize)
{
    lpClient->RfbClient.dwErr=NEED_MORE_DATA;
    IOCP_RecvMoreData(lpClient,dwSize);
    return;
}

static bool RFB_HandleAuth(PIOCP_CLIENT lpClient,PCHAR lpBuf,DWORD dwBufSize,LPCSTR lpPassword)
{
    bool bRet=false;
    switch (lpClient->RfbClient.dwAuthScheme)
    {
        case rfbNoAuth:
        {
            bRet=true;
            break;
        }
        case rfbVncAuth:
        {
            if (dwBufSize < CHALLENGESIZE)
            {
                RFB_NeedMoreData(lpClient,CHALLENGESIZE-dwBufSize);
                break;
            }

            byte bEncPasswd[8];
            rfbEncryptPasswd(bEncPasswd,lpPassword);

            rfbEncryptBytes((LPBYTE)lpBuf,lpPassword);
            bRet=IOCP_SendData(lpClient,lpBuf,CHALLENGESIZE);
            break;
        }
    }
    return bRet;
}

static bool RFB_ReadSupportedSecurityType(PIOCP_CLIENT lpClient,byte bCount,LPBYTE lpTypes)
{
    bool bRet=false;
    for (byte i=0; i < bCount; i++)
    {
        if ((lpTypes[i] == rfbVncAuth) || (lpTypes[i] == rfbNoAuth))
        {
            lpClient->RfbClient.dwAuthScheme=lpTypes[i];
            bRet=IOCP_SendData(lpClient,(PCHAR)&lpTypes[i],1);
            break;
        }
    }
    return bRet;
}

bool RFB_HandleAnswer(PIOCP_CLIENT lpClient)
{
    lpClient->RfbClient.dwErr=OK;

    PCHAR lpBuffer=lpClient->cRecvBuf,lpPtr=lpBuffer;
    DWORD dwBytesRecv=lpClient->ovl.dwBytesProcessed;

    switch (lpClient->RfbClient.dwState)
    {
        case VNC_HANDSHAKE:
        {
            if ((dwBytesRecv < RFB_BANNER_SIZE) || (StrCmpNA(lpBuffer,"RFB ",4)))
            {
                lpClient->RfbClient.dwErr=ERR_NOT_RFB;
                break;
            }

            if (sscanf(lpBuffer,"RFB %03d.%03d\n",&lpClient->RfbClient.dwMajorVersion,&lpClient->RfbClient.dwMinorVersion) < 2)
            {
                lpClient->RfbClient.dwErr=ERR_NOT_RFB;
                break;
            }

            if (MemStrA(lpBuffer,dwBytesRecv," failures"))
            {
                lpClient->RfbClient.dwErr=ERR_TOO_MANY_AUTH;
                break;
            }

            if (lpClient->RfbClient.dwMajorVersion == 3)
            {
                if ((lpClient->RfbClient.dwMinorVersion == 14) || (lpClient->RfbClient.dwMinorVersion == 16))
                    lpClient->RfbClient.dwMinorVersion-=10;
            }

            if ((lpClient->RfbClient.dwMajorVersion > 3) || (lpClient->RfbClient.dwMinorVersion > 8))
            {
                lpClient->RfbClient.dwMajorVersion=3;
                lpClient->RfbClient.dwMinorVersion=8;
            }

            char szInit[13];
            wsprintfA(szInit,"RFB %03d.%03d\n",lpClient->RfbClient.dwMajorVersion,lpClient->RfbClient.dwMinorVersion);

            if (!IOCP_SendData(lpClient,szInit,RFB_BANNER_SIZE))
            {
                lpClient->RfbClient.dwErr=ERR_CONN_FAILED;
                break;
            }

            lpClient->RfbClient.dwState=VNC_SECURITY_TYPE;
            break;
        }
        case VNC_SECURITY_TYPE:
        {
            if (MemStrA(lpBuffer,dwBytesRecv," failures"))
            {
                lpClient->RfbClient.dwErr=ERR_TOO_MANY_AUTH;
                break;
            }

            if (lpClient->RfbClient.dwMinorVersion > 6)
            {
                if (!lpBuffer[0])
                {
                    lpClient->RfbClient.dwErr=ERR_CONN_FAILED;
                    break;
                }

                if (lpBuffer[0] > (dwBytesRecv-1))
                {
                    RFB_NeedMoreData(lpClient,lpBuffer[0]-(dwBytesRecv-1));
                    break;
                }

                if (!RFB_ReadSupportedSecurityType(lpClient,lpBuffer[0],(LPBYTE)&lpBuffer[1]))
                {
                    lpClient->RfbClient.dwErr=ERR_AUTH_UNSUPPORTED;
                    break;
                }

                lpClient->RfbClient.dwState=VNC_AUTH;
                break;
            }

            if (dwBytesRecv < sizeof(DWORD))
            {
                lpClient->RfbClient.dwErr=ERR_CONN_FAILED;
                break;
            }

            lpClient->RfbClient.dwAuthScheme=rfbClientSwap32IfLE(*(LPDWORD)lpBuffer);
            lpPtr+=sizeof(DWORD);
            dwBytesRecv-=sizeof(DWORD);
        }
        case VNC_AUTH:
        {
            char szPassword[9];
            if (!RFB_HandleAuth(lpClient,lpPtr,dwBytesRecv,Dict_GetPassword(lpClient->RfbClient.dwIdx,szPassword)))
            {
                if (lpClient->RfbClient.dwErr != NEED_MORE_DATA)
                    lpClient->RfbClient.dwErr=ERR_CONN_FAILED;
                break;
            }

            if (lpClient->RfbClient.dwAuthScheme != rfbNoAuth)
            {
                lpClient->RfbClient.dwState=VNC_SECURITY_RESULT;
                break;
            }

            char cRfbInit=1;
            IOCP_SendData(lpClient,&cRfbInit,sizeof(cRfbInit));

            lpClient->RfbClient.dwState=VNC_SERVER_INFO;
            break;
        }
        case VNC_SECURITY_RESULT:
        {
            switch (rfbClientSwap32IfLE(*(LPDWORD)lpBuffer))
            {
                case rfbVncAuthOK:
                {
                    char cRfbInit=1;
                    IOCP_SendData(lpClient,&cRfbInit,sizeof(cRfbInit));

                    lpClient->RfbClient.dwState=VNC_SERVER_INFO;
                    break;
                }
                case rfbVncAuthFailed:
                {
                    lpClient->RfbClient.dwErr=ERR_AUTH_FAILED;
                    break;
                }
                case rfbVncAuthTooMany:
                {
                    lpClient->RfbClient.dwErr=ERR_TOO_MANY_AUTH;
                    break;
                }
            }
            break;
        }
        case VNC_SERVER_INFO:
        {
            if (dwBytesRecv < sizeof(rfbServerInitMsg))
            {
                RFB_NeedMoreData(lpClient,sizeof(rfbServerInitMsg)-dwBytesRecv);
                break;
            }

            rfbServerInitMsg *lpRfbInitMsg=(rfbServerInitMsg*)(lpBuffer);
            if (dwBytesRecv < (sizeof(rfbServerInitMsg)+rfbClientSwap32IfLE(lpRfbInitMsg->nameLength)))
            {
                RFB_NeedMoreData(lpClient,sizeof(rfbServerInitMsg)+rfbClientSwap32IfLE(lpRfbInitMsg->nameLength)-dwBytesRecv);
                break;
            }

            LPSTR lpDesk=(LPSTR)((LPBYTE)lpRfbInitMsg+sizeof(*lpRfbInitMsg));
            StrCpyNA(lpClient->RfbClient.szDesktop,lpDesk,rfbClientSwap32IfLE(lpRfbInitMsg->nameLength)+1);

            lpClient->RfbClient.dwState=VNC_AUTH_DONE;
            break;
        }
    }
    return ((lpClient->RfbClient.dwErr == OK) || (lpClient->RfbClient.dwErr == NEED_MORE_DATA));
}

