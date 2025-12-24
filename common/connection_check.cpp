#include <windows.h>
#include <stdio.h>
#include <ipexport.h>
#include <icmpapi.h>
#include <tchar.h>
#include <wbemcli.h>
#include <objbase.h>
#include <wininet.h>

#include <cjson\cjson.h>

#include <syslib\net.h>
#include <syslib\time.h>
#include <syslib\system.h>
#include <syslib\str.h>
#include <syslib\mem.h>
#include <syslib\threadsgroup.h>

#include "json_packet.h"
#include "dbg_log.h"
#include "common_dlg.h"
#include "connection_check.h"

static DWORD dwLastLinkUpTime=0,dwConnectionLostTime=0,dwPingTimeout=CHECK_DEFAULT_TIMEOUT;
bool IsOnline()
{
    return (Now()-InterlockedExchangeAdd(&dwLastLinkUpTime,0) < dwPingTimeout);
}

static IWbemLocator *CreateIWbemLocator(IWbemContext **lppContext)
{
    IWbemLocator *lpLocator=NULL;

    if (SUCCEEDED(CoCreateInstance(CLSID_WbemContext,0,CLSCTX_INPROC_SERVER,IID_IWbemContext,(LPVOID*)lppContext)))
    {
        if (SysIsWindows64())
        {
            VARIANT vArch;
            VariantInit(&vArch);
            vArch.vt=VT_I4;
            vArch.lVal=64;

            (*lppContext)->SetValue(L"__ProviderArchitecture",0,&vArch);
        }

        if (FAILED(CoCreateInstance(CLSID_WbemLocator,NULL,CLSCTX_INPROC_SERVER|CLSCTX_NO_FAILURE_LOG|CLSCTX_NO_CODE_DOWNLOAD,IID_IWbemLocator,(LPVOID*)&lpLocator)))
        {
            (*lppContext)->Release();
            *lppContext=NULL;
        }
    }
    return lpLocator;
}

static BSTR bsQuery;
static WORD GetStatus()
{
    WORD wStatus=0;

    IWbemContext *lpContext=NULL;
    IWbemLocator *lpLocator=CreateIWbemLocator(&lpContext);
    if ((lpContext) && (lpLocator))
    {
        BSTR bsResource=SysAllocString(L"ROOT\\cimv2"),
             bsWQL=SysAllocString(L"WQL");

        IWbemServices *lpService;
        if ((SUCCEEDED(lpLocator->ConnectServer(bsResource,NULL,NULL,NULL,NULL,NULL,lpContext,&lpService))) && (lpService))
        {
            if (SUCCEEDED(CoSetProxyBlanket(lpService,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,NULL,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE)))
            {
                IEnumWbemClassObject *lpEnumerator=NULL;
                if (SUCCEEDED(lpService->ExecQuery(bsWQL,bsQuery,WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,NULL,&lpEnumerator)))
                {
                    IWbemClassObject *lpItem;
                    ULONG uReturn=0;
                    lpEnumerator->Next(WBEM_INFINITE,1,&lpItem,&uReturn);
                    if (uReturn)
                    {
                        VARIANT vtPath;
                        VariantInit(&vtPath);
                        if (SUCCEEDED(lpItem->Get(L"__Path",0,&vtPath,NULL,NULL)))
                        {
                            VARIANT vtStatus;
                            VariantInit(&vtStatus);
                            if (SUCCEEDED(lpItem->Get(L"NetConnectionStatus",0,&vtStatus,NULL,NULL)))
                            {
                                wStatus=vtStatus.uiVal;
                                VariantClear(&vtStatus);
                            }
                            VariantClear(&vtPath);
                        }
                        lpItem->Release();
                    }
                    lpEnumerator->Release();
                }
            }
            lpService->Release();
        }

        SysFreeString(bsResource);
        SysFreeString(bsWQL);

        lpLocator->Release();
        lpContext->Release();
    }
    return wStatus;
}

static HANDLE hShutdownEvent;
static void ReconnectWAN()
{
    IWbemContext *lpContext=NULL;
    IWbemLocator *lpLocator=CreateIWbemLocator(&lpContext);
    if ((lpContext) && (lpLocator))
    {
        BSTR bsResource=SysAllocString(L"ROOT\\cimv2"),
             bsWQL=SysAllocString(L"WQL"),
             bsDisable=SysAllocString(L"Disable"),
             bsEnable=SysAllocString(L"Enable");

        if (GetStatus() == 2)
        {
            IWbemServices *lpService;
            if ((SUCCEEDED(lpLocator->ConnectServer(bsResource,NULL,NULL,NULL,NULL,NULL,lpContext,&lpService))) && (lpService))
            {
                if (SUCCEEDED(CoSetProxyBlanket(lpService,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,NULL,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE)))
                {
                    IEnumWbemClassObject *lpEnumerator=NULL;
                    if (SUCCEEDED(lpService->ExecQuery(bsWQL,bsQuery,WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,NULL,&lpEnumerator)))
                    {
                        IWbemClassObject *lpItem;
                        ULONG uReturn=0;
                        lpEnumerator->Next(WBEM_INFINITE,1,&lpItem,&uReturn);
                        if (uReturn)
                        {
                            VARIANT vtPath;
                            VariantInit(&vtPath);
                            if (SUCCEEDED(lpItem->Get(L"__Path",0,&vtPath,NULL,NULL)))
                            {
                                IWbemClassObject *lpResult=NULL;
                                if (SUCCEEDED(lpService->ExecMethod(vtPath.bstrVal,bsDisable,0,NULL,NULL,&lpResult,NULL)))
                                {
                                    lpResult->Release();

                                    Sleep(5*MILLISECONDS_PER_SECOND);

                                    lpResult=NULL;
                                    if (SUCCEEDED(lpService->ExecMethod(vtPath.bstrVal,bsEnable,0,NULL,NULL,&lpResult,NULL)))
                                        lpResult->Release();

                                    while (WaitForSingleObject(hShutdownEvent,1*MILLISECONDS_PER_SECOND) == WAIT_TIMEOUT)
                                    {
                                        if (GetStatus() == 2)
                                            break;
                                    }
                                }
                                VariantClear(&vtPath);
                            }
                            lpItem->Release();
                        }
                        lpEnumerator->Release();
                    }
                }
                lpService->Release();
            }
        }

        SysFreeString(bsResource);
        SysFreeString(bsWQL);
        SysFreeString(bsDisable);
        SysFreeString(bsEnable);

        lpLocator->Release();
        lpContext->Release();
    }
    return;
}

static INTERNET_STATE InternetCheckConnectionA(LPCSTR lpHost,WORD wPort)
{
    INTERNET_STATE dwState=INET_FAILED;
    do
    {
        sockaddr_in srv_addr={0};
        srv_addr.sin_family=AF_INET;
        srv_addr.sin_port=ntohs(wPort);
        srv_addr.sin_addr.s_addr=NetResolveAddress(lpHost);
        if (srv_addr.sin_addr.s_addr == INADDR_NONE)
        {
            dwState=INET_NO_INET;
            break;
        }

        SOCKET hSock=socket(AF_INET,SOCK_STREAM,0);
        if (hSock == INVALID_SOCKET)
            break;

        DWORD dwResult=connect(hSock,(sockaddr *)&srv_addr,sizeof(srv_addr));
        DWORD dwLastError=WSAGetLastError();

        closesocket(hSock);

        if (!dwResult)
        {
            dwState=INET_OK;
            break;
        }

        if (dwLastError != WSAEADDRNOTAVAIL)
            break;

        dwState=INET_NO_INET;
    }
    while (false);

    return dwState;
}

static void WINAPI PingThread(LPVOID)
{
    HANDLE hIcmpFile=IcmpCreateFile();
    if (!hIcmpFile)
        return;

    DWORD dwAddress=NetResolveAddress("8.8.8.8");
    byte bData[]="ping",bReply[sizeof(ICMP_ECHO_REPLY)+sizeof(bData)+8];
    while (WaitForSingleObject(hShutdownEvent,1) == WAIT_TIMEOUT)
    {
        if (IcmpSendEcho2(hIcmpFile,NULL,NULL,NULL,dwAddress,bData,sizeof(bData),NULL,bReply,sizeof(bReply),1))
            continue;

        DWORD dwGLE=GetLastError();
        if ((dwGLE != ERROR_NETWORK_UNREACHABLE) && (dwGLE != IP_GENERAL_FAILURE))
            continue;

        InterlockedExchange(&dwLastLinkUpTime,1);
        InterlockedExchange(&dwConnectionLostTime,1);
    }

    IcmpCloseHandle(hIcmpFile);
    return;
}

extern cJSON *jsConfig;
static void WINAPI InetCheckThread(HWND hDlg)
{
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_PKT_PRIVACY,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,0,NULL);

    bool bEnabled=true,bReconnect=false;

    DWORD dwReconnectTimeout=CHECK_DEFAULT_TIMEOUT;

    char szUrl[MAX_PATH]="www.google.com:80";

    cJSON *jsPinger=cJSON_GetObjectItem(jsConfig,"connection_check");
    if (jsPinger)
    {
        bEnabled=cJSON_GetBoolFromObject(jsPinger,"enabled");
        if (bEnabled)
        {
            dwPingTimeout=cJSON_GetIntFromObject(jsPinger,"timeout");
            if (!dwPingTimeout)
                dwPingTimeout=CHECK_DEFAULT_TIMEOUT;

            LPCSTR lpUrl=cJSON_GetStringFromObject(jsPinger,"address");
            if ((lpUrl) && (lpUrl[0]))
                lstrcpyA(szUrl,lpUrl);

            cJSON *jsReconnect=cJSON_GetObjectItem(jsPinger,"reconnect");
            if (jsReconnect)
            {
                bReconnect=cJSON_GetBoolFromObject(jsReconnect,"enabled");
                if (bReconnect)
                {
                    dwReconnectTimeout=cJSON_GetIntFromObject(jsReconnect,"timeout");
                    if (!dwReconnectTimeout)
                        dwReconnectTimeout=CHECK_DEFAULT_TIMEOUT;

                    bReconnect=false;
                    LPWSTR lpQuery;
                    if (StrFormatExW(&lpQuery,L"SELECT * FROM Win32_NetworkAdapter WHERE NetConnectionID=\"%S\"",cJSON_GetStringFromObject(jsReconnect,"connection")))
                    {
                        bsQuery=SysAllocString(lpQuery);
                        MemFree(lpQuery);

                        bReconnect=true;
                    }
                }
            }
        }
    }

    char szHost[INTERNET_MAX_HOST_NAME_LENGTH];
    WORD wPort=80;
    sscanf(szUrl,"%[^:]:%d",szHost,&wPort);

    bool bNAStatusSent=false,bOnlineStatusSent=false;
    do
    {
        do
        {
            INTERNET_STATE dwState=InternetCheckConnectionA(szHost,wPort);
            if ((bEnabled) && (dwState != INET_OK))
            {
                if (IsOnline())
                    break;

                if (bNAStatusSent)
                    break;

                if ((bReconnect) && (!InterlockedExchangeAdd(&dwConnectionLostTime,0)))
                    InterlockedExchange(&dwConnectionLostTime,Now());

                bOnlineStatusSent=false;
                bNAStatusSent=true;
                if (siManager.hSock == INVALID_SOCKET)
                    break;

                if (!siManager.bConnected)
                    break;

                PostMessage(hDlg,WM_COMMAND,CMD_SEND_STATUS,(LPARAM)"n/a");

                DbgLog_Event(&siManager,LOG_LEVEL_CONNECTION,"Connection lost.");
                break;
            }

            InterlockedExchange(&dwLastLinkUpTime,Now());

            if (bOnlineStatusSent)
                break;

            InterlockedExchange(&dwConnectionLostTime,0);

            bOnlineStatusSent=true;
            bNAStatusSent=false;
            if (siManager.hSock == INVALID_SOCKET)
                break;

            if (!siManager.bConnected)
                break;

            PostMessage(hDlg,WM_COMMAND,CMD_SEND_STATUS,(LPARAM)"online");
            break;
        }
        while (false);

        if ((!InterlockedExchangeAdd(&dwConnectionLostTime,0)) || (IsActiveTasks()) || (Now()-InterlockedExchangeAdd(&dwConnectionLostTime,0) < dwReconnectTimeout))
            continue;

        ReconnectWAN();

        if (WaitForSingleObject(hShutdownEvent,30*MILLISECONDS_PER_SECOND) == WAIT_OBJECT_0)
            break;

        InterlockedExchange(&dwConnectionLostTime,Now()+10);
    }
    while (WaitForSingleObject(hShutdownEvent,10*MILLISECONDS_PER_SECOND) == WAIT_TIMEOUT);
    return;
}

static HANDLE hPingThreads;
void InetCheck_Start(HWND hDlg)
{
    hPingThreads=ThreadsGroup_Create();
    hShutdownEvent=CreateEvent(NULL,true,false,NULL);

    ThreadsGroup_CreateThread(hPingThreads,0,(LPTHREAD_START_ROUTINE)InetCheckThread,hDlg,0,NULL);
    ThreadsGroup_CreateThread(hPingThreads,0,(LPTHREAD_START_ROUTINE)PingThread,NULL,0,NULL);
    return;
}

void InetCheck_Stop()
{
    SetEvent(hShutdownEvent);
    ThreadsGroup_WaitForAllExit(hPingThreads,INFINITE);
    return;
}
