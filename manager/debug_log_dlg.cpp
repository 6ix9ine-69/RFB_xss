#include "includes.h"
#include <ntdll.h>
#include <richedit.h>

#include <syslib\str.h>

namespace SYSLIB
{
    DWORD StrFmt_FormatStringA(LPSTR lpDest,LPCSTR lpFormat,va_list args);
}

static HWND hLogWnd;
static void DebugLog_AddText(LPCSTR lpText,bool bBold)
{
    CHARFORMAT cf={sizeof(cf),CFM_BOLD};

    if (bBold)
        cf.dwEffects|=CFE_BOLD;

    SendMessage(hLogWnd,EM_SETSEL,0,0);

    SendMessage(hLogWnd,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&cf);
    SendMessageA(hLogWnd,EM_REPLACESEL,NULL,(LPARAM)lpText);

    SendMessage(hLogWnd,EM_SETSEL,0,0);
    SendMessage(hLogWnd,EM_SCROLLCARET,NULL,NULL);
    return;
}

static void DebugLog_UpdateMenuStateInt(HMENU hMenu,DWORD dwItem,bool bChecked)
{
    DWORD dwState=MF_BYCOMMAND;
    if (bChecked)
        dwState|=MF_CHECKED;

    CheckMenuItem(hMenu,dwItem,dwState);
    return;
}

static cJSON *jsLogging;
static bool bInfo,bWarning,bError,bConnection;
void DebugLog_UpdateMenuState(HMENU hMenu,DWORD dwItem)
{
    switch (dwItem)
    {
        case -1:
        {
            DebugLog_UpdateMenuStateInt(hMenu,IDM_LEVEL_INFO,bInfo);
            DebugLog_UpdateMenuStateInt(hMenu,IDM_LEVEL_WARNING,bWarning);
            DebugLog_UpdateMenuStateInt(hMenu,IDM_LEVEL_ERROR,bError);
            DebugLog_UpdateMenuStateInt(hMenu,IDM_LEVEL_CONNECTION,bConnection);
            break;
        }
        case IDM_LEVEL_INFO:
        {
            bInfo=!bInfo;
            DebugLog_UpdateMenuStateInt(hMenu,IDM_LEVEL_INFO,bInfo);

            cJSON *jsInfo=cJSON_GetObjectItem(jsLogging,"info");
            if (jsInfo)
                cJSON_SetBoolValue(jsInfo,bInfo);

            Config_Update();
            break;
        }
        case IDM_LEVEL_WARNING:
        {
            bWarning=!bWarning;
            DebugLog_UpdateMenuStateInt(hMenu,IDM_LEVEL_WARNING,bWarning);

            cJSON *jsInfo=cJSON_GetObjectItem(jsLogging,"warning");
            if (jsInfo)
                cJSON_SetBoolValue(jsInfo,bWarning);

            Config_Update();
            break;
        }
        case IDM_LEVEL_ERROR:
        {
            bError=!bError;
            DebugLog_UpdateMenuStateInt(hMenu,IDM_LEVEL_ERROR,bError);

            cJSON *jsInfo=cJSON_GetObjectItem(jsLogging,"error");
            if (jsInfo)
                cJSON_SetBoolValue(jsInfo,bError);

            Config_Update();
            break;
        }
        case IDM_LEVEL_CONNECTION:
        {
            bConnection=!bConnection;
            DebugLog_UpdateMenuStateInt(hMenu,IDM_LEVEL_CONNECTION,bConnection);

            cJSON *jsInfo=cJSON_GetObjectItem(jsLogging,"connection");
            if (jsInfo)
                cJSON_SetBoolValue(jsInfo,bConnection);

            Config_Update();
            break;
        }
    }
    return;
}

static HWND hDebugLogDlg;
static void DebugLog_RealAddItem(PCONNECTION lpConnection,DWORD dwTime,LOG_LEVEL dwLevel,LPCSTR lpFile,DWORD dwLine,LPCSTR lpFunc,LPCSTR lpBody)
{
    if (dwLevel == LOG_LEVEL_INFO)
    {
        if (!bInfo)
            return;
    }
    else if (dwLevel == LOG_LEVEL_WARNING)
    {
        if (!bWarning)
            return;
    }
    else if (dwLevel == LOG_LEVEL_ERROR)
    {
        if (!bError)
            return;
    }
    else if (dwLevel == LOG_LEVEL_CONNECTION)
    {
        if (!bConnection)
            return;
    }

    PostMessage(hDebugLogDlg,WM_COMMAND,CMD_NOTIFY_TAB,0);

    LARGE_INTEGER liTime;
    RtlSecondsSince1980ToTime(dwTime,&liTime);

    FILETIME ft;
    ft.dwHighDateTime=liTime.HighPart;
    ft.dwLowDateTime=liTime.LowPart;

    FILETIME ftLocal;
    FileTimeToLocalFileTime(&ft,&ftLocal);

    SYSTEMTIME st;
    FileTimeToSystemTime(&ftLocal,&st);

    char szTime[MAX_PATH];
    wsprintfA(szTime,"[%.4d/%.2d/%.2d %.2d:%.2d:%.2d] ",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);

    LPCSTR lpLevel;
    switch (dwLevel)
    {
        case LOG_LEVEL_INFO:
        {
            lpLevel="INFO: ";
            break;
        }
        case LOG_LEVEL_WARNING:
        {
            lpLevel="WARNING: ";
            break;
        }
        case LOG_LEVEL_ERROR:
        {
            lpLevel="ERROR: ";
            break;
        }
        case LOG_LEVEL_CONNECTION:
        {
            lpLevel="CONNECTION: ";
            break;
        }
        default:
        {
            lpLevel="UNKNOWN: ";
            break;
        }
    }

    LPSTR lpText=NULL;

    if (lpConnection)
    {
        if (lpConnection->dwType == TYPE_WORKER)
            lpText=StrDuplicateA("from worker",0);
        else if (lpConnection->dwType == TYPE_CHECKER)
            lpText=StrDuplicateA("from checker",0);

        if (lpConnection->szAlias[0])
            StrCatFormatExA(&lpText,0," (%s), ",lpConnection->szAlias);
        else
            StrCatFormatExA(&lpText,0," (%s), ",NetNtoA(lpConnection->Socket.si.sin_addr.s_addr));
    }

    StrCatFormatExA(&lpText,0,"%s%s:%d:%s: %s\r\n",lpLevel,lpFile,dwLine,lpFunc,lpBody);

    DebugLog_AddText(lpText,false);
    DebugLog_AddText(szTime,true);

    MemFree(lpText);
    return;
}

static DWORD dwLogThreadId;
static void WINAPI LogThread(HANDLE hEvent)
{
    MSG msg;
    PeekMessage(&msg,NULL,WM_USER,WM_USER,PM_NOREMOVE);
    SetEvent(hEvent);

    while ((int)GetMessage(&msg,NULL,0,0) > 0)
    {
        switch (msg.message)
        {
            case ADD_LOG_MESSAGE:
            {
                PLOG_ITEM lpItem=(PLOG_ITEM)msg.lParam;
                if (!lpItem)
                    break;

                DebugLog_RealAddItem(lpItem->lpConnection,lpItem->dwTime,lpItem->dwLevel,lpItem->lpFile,lpItem->dwLine,lpItem->lpFunc,lpItem->lpBody);

                MemFree(lpItem->lpFile);
                MemFree(lpItem->lpFunc);
                MemFree(lpItem->lpBody);
                MemFree(lpItem);
                break;
            }
            case CLEAR_LOG_MESSAGE:
            {
                SetWindowText(hLogWnd,NULL);
                break;
            }
        }
    }
    return;
}

static void DebugLog_AddItemInt(PCONNECTION lpConnection,DWORD dwTime,LOG_LEVEL dwLevel,LPCSTR lpFile,DWORD dwLine,LPCSTR lpFunc,LPCSTR lpBody)
{
    PLOG_ITEM lpItem=(PLOG_ITEM)MemQuickAlloc(sizeof(*lpItem));
    if (lpItem)
    {
        lpItem->lpConnection=lpConnection;
        lpItem->dwTime=dwTime;
        lpItem->dwLevel=dwLevel;
        lpItem->lpFile=StrDuplicateA(lpFile,0);
        lpItem->dwLine=dwLine;
        lpItem->lpFunc=StrDuplicateA(lpFunc,0);
        lpItem->lpBody=StrDuplicateA(lpBody,0);

        PostThreadMessageEx(dwLogThreadId,ADD_LOG_MESSAGE,NULL,(LPARAM)lpItem);
    }
    return;
}

void DebugLog_AddItemEx(PCONNECTION lpConnection,DWORD dwTime,LOG_LEVEL dwLevel,LPCSTR lpFile,DWORD dwLine,LPCSTR lpFunc,LPCSTR lpFormat,...)
{
    LPSTR lpBody=(LPSTR)MemQuickAlloc(1024);
    if (lpBody)
    {
        va_list mylist;
        va_start(mylist,lpFormat);
        SYSLIB::StrFmt_FormatStringA(lpBody,lpFormat,mylist);
        va_end(mylist);

        DebugLog_AddItemInt(lpConnection,dwTime,dwLevel,lpFile,dwLine,lpFunc,lpBody);
        MemFree(lpBody);
    }
    return;
}

void DebugLog_AddItem(PCONNECTION lpConnection,cJSON *jsLog)
{
    DebugLog_AddItemInt(lpConnection,cJSON_GetIntFromObject(jsLog,"time"),
                        (LOG_LEVEL)cJSON_GetIntFromObject(jsLog,"level"),
                        cJSON_GetStringFromObject(jsLog,"file"),
                        cJSON_GetIntFromObject(jsLog,"line"),
                        cJSON_GetStringFromObject(jsLog,"func"),
                        cJSON_GetStringFromObject(jsLog,"body"));
    return;
}

static LRESULT CALLBACK NewEditProc(HWND hEdit,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    WNDPROC lpOldWndProc=(WNDPROC)GetWindowLongPtr(hEdit,GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_MOUSEWHEEL:
        {
            if (((int)wParam & MK_CONTROL) == MK_CONTROL)
                return 0;
        }
    }
    return CallWindowProc(lpOldWndProc,hEdit,uMsg,wParam,lParam);
}

static HMENU hContextMenu;
INT_PTR CALLBACK DebugLogDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    PCOMM_TAB lpParam=(PCOMM_TAB)GetWindowLongPtr(hDlg,GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            lpParam=(PCOMM_TAB)lParam;
            SetWindowLongPtr(hDlg,GWLP_USERDATA,(LONG_PTR)lpParam);

            hDebugLogDlg=hDlg;
            hLogWnd=GetDlgItem(hDlg,IDC_DEBUG_LOG);

            SetWindowLongPtr(hLogWnd,GWLP_USERDATA,(LONG_PTR)SetWindowLongPtr(hLogWnd,GWLP_WNDPROC,(LONG_PTR)NewEditProc));

            HANDLE hEvent=CreateEvent(NULL,false,false,NULL);
            ThreadsGroup_CreateThread(hThreadsGroup,0,(LPTHREAD_START_ROUTINE)LogThread,hEvent,&dwLogThreadId,NULL);
            WaitForSingleObject(hEvent,INFINITE);
            CloseHandle(hEvent);

            hContextMenu=GetSubMenu(LoadMenu(hInstance,MAKEINTRESOURCE(IDR_CONTEXT_LOG)),0);

            SetFocus(hLogWnd);
            SendMessage(hLogWnd,EM_SETSEL,0,0);
            SendMessage(hLogWnd,EM_SCROLLCARET,NULL,NULL);
            break;
        }
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDM_LOG_CLEAR:
                {
                    PostThreadMessageEx(dwLogThreadId,CLEAR_LOG_MESSAGE,NULL,NULL);
                    break;
                }
                case CMD_NOTIFY_TAB:
                {
                    SendMessage(hMainDlg,WM_COMMAND,CMD_NOTIFY_TAB,lpParam->dwTabIdx);
                    break;
                }
            }
            break;
        }
        case WM_SIZE:
        {
            RECT rc;
            GetClientRect(GetParent(hDlg),&rc);
            MoveWindow(hDlg,0,0,rc.right,rc.bottom,true);

            MoveWindow(hLogWnd,0,0,rc.right,rc.bottom,true);
            break;
        }
        case WM_CLOSE:
        {
            PostThreadMessage(dwLogThreadId,WM_QUIT,NULL,NULL);
            break;
        }
        case WM_CONTEXTMENU:
        {
            if ((HWND)wParam == hLogWnd)
            {
                DWORD dwState=MF_BYCOMMAND;
                if (!GetWindowTextLength(hLogWnd))
                    dwState|=MF_DISABLED|MF_GRAYED;

                POINT pt;
                GetCursorPos(&pt);

                EnableMenuItem(hContextMenu,IDM_LOG_CLEAR,dwState);
                TrackPopupMenu(hContextMenu,TPM_TOPALIGN|TPM_LEFTALIGN,pt.x,pt.y,0,hDlg,NULL);
            }
            break;
        }
    }
    return FALSE;
}

void DebugLog_Init(cJSON *jsConfig)
{
    do
    {
        Splash_SetText(_T("Initializing debug log..."));

        jsLogging=cJSON_GetObjectItem(jsConfig,"logging");
        if (!jsLogging)
            break;

        bInfo=cJSON_GetBoolFromObject(jsLogging,"info");
        bWarning=cJSON_GetBoolFromObject(jsLogging,"warning");
        bError=cJSON_GetBoolFromObject(jsLogging,"error");
        bConnection=cJSON_GetBoolFromObject(jsLogging,"connection");
    }
    while (false);
    return;
}

