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

        if (IOCP_AddAddressToCheckList((DWORD)cJSON_GetIntFromObject(jsItem,"ip"),(WORD)cJSON_GetIntFromObject(jsItem,"port")))
        {
            dwProcessed++;
            continue;
        }

        if (IOCP_IsStopped())
            break;

        DbgLog_Event(&siManager,LOG_LEVEL_WARNING,"IOCP_AddAddressToCheckList() failed, position %d/%d",i,dwItems);
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
        cJSON_AddIntToObject(jsStats,"checked",dwChecked);
        cJSON_AddIntToObject(jsStats,"rfb",dwRFB);
        cJSON_AddIntToObject(jsStats,"not_rfb",dwNotRFB);
        cJSON_AddIntToObject(jsStats,"conn_failed",dwFailed);
    }
    return jsStats;
}

static void UpdateStat(HWND hStat,DWORD dwId,DWORD dwData,LPDWORD lpdwPrevData)
{
    if (dwData != *lpdwPrevData)
    {
        *lpdwPrevData=dwData;

        char szDataFormatted[MAX_PATH];
        StrNumberFormatA(dwData,szDataFormatted,ARRAYSIZE(szDataFormatted),",");

        LVITEMA lvi={0};
        lvi.mask=LVIF_TEXT;
        lvi.iItem=dwId;
        lvi.iSubItem=1;
        lvi.cchTextMax=ARRAYSIZE(szDataFormatted);
        lvi.pszText=szDataFormatted;
        SendMessageA(hStat,LVM_SETITEMA,NULL,(LPARAM)&lvi);
    }
    return;
}

typedef struct _STAT_DATA
{
    DWORD dwActiveTasks;
    DWORD dwChecked;
    DWORD dwRFB;
    DWORD dwNotRFB;
    DWORD dwFailed;
} STAT_DATA, *PSTAT_DATA;

static STAT_DATA sd={0};
void UpdateStats()
{
    HWND hStat=GetDlgItem(hCommonDlg,IDC_STAT);
    if (hStat)
    {
        SendMessageW(hStat,WM_SETREDRAW,false,NULL);

        if (InterlockedExchangeAdd(&dwActiveTasks,0) != sd.dwActiveTasks)
        {
            sd.dwActiveTasks=InterlockedExchangeAdd(&dwActiveTasks,0);

            char szText[MAX_PATH];
            LVITEMA lvi={0};
            lvi.mask=LVIF_TEXT;
            lvi.cchTextMax=ARRAYSIZE(szText);
            lvi.pszText=szText;
            lvi.iSubItem=1;
            lvi.iItem=0;
            wsprintfA(szText,"%d/%d",sd.dwActiveTasks,dwTotalActiveTasks);
            SendMessageA(hStat,LVM_SETITEMA,NULL,(LPARAM)&lvi);
        }

        UpdateStat(hStat,1,dwChecked,&sd.dwChecked);
        UpdateStat(hStat,2,dwRFB,&sd.dwRFB);
        UpdateStat(hStat,3,dwNotRFB,&sd.dwNotRFB);
        UpdateStat(hStat,4,dwFailed,&sd.dwFailed);

        SendMessageW(hStat,WM_SETREDRAW,true,NULL);
    }
    return;
}

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

        cJSON *jsDlg=cJSON_GetObjectItem(jsSettings,"checker_dlg");
        if (!jsDlg)
        {
            jsDlg=cJSON_CreateObject();
            cJSON_AddItemToObject(jsSettings,"checker_dlg",jsDlg);
        }

        Setting_ChangeValue(jsDlg,"x",wp.rcNormalPosition.left);
        Setting_ChangeValue(jsDlg,"y",wp.rcNormalPosition.top);
        Setting_ChangeValue(jsDlg,"width",wp.rcNormalPosition.right-wp.rcNormalPosition.left);
        Setting_ChangeValue(jsDlg,"height",wp.rcNormalPosition.bottom-wp.rcNormalPosition.top);

        Setting_ChangeValue(jsDlg,"maximized",IsZoomed(hCommonDlg));
        Setting_ChangeValue(jsDlg,"top_most",(GetWindowLong(hCommonDlg,GWL_EXSTYLE) & WS_EX_TOPMOST));

        Settings_Save(jsSettings);
    }
    while (false);
    return;
}

void ResizeDlg()
{
    RECT rc;
    GetClientRect(hCommonDlg,&rc);

    HWND hList=GetDlgItem(hCommonDlg,IDC_STAT);
    MoveWindow(hList,10,(rc.bottom-(5*24+12))/2,rc.right-20,5*24+12,true);

    LVCOLUMN lvc={0};
    lvc.mask=LVCF_WIDTH;
    lvc.cx=rc.right-125;
    SendMessage(hList,LVM_SETCOLUMN,1,(LPARAM)&lvc);
    return;
}

static void GridView_AddItem(HWND hList,LPLVITEMA lpLVI,LPSTR lpLabel,LPSTR lpValue)
{
    lpLVI->iSubItem=0;
    lpLVI->pszText=lpLabel;
    SendMessageA(hList,LVM_INSERTITEMA,NULL,(LPARAM)lpLVI);

    lpLVI->iSubItem++;
    lpLVI->pszText=lpValue;
    SendMessageA(hList,LVM_SETITEMA,NULL,(LPARAM)lpLVI);

    lpLVI->iItem++;
    return;
}

LRESULT GridView_ProcessCustomDraw(LPARAM lParam)
{
    LRESULT dwRet=CDRF_DODEFAULT;

    LPNMLVCUSTOMDRAW lplvcd=(LPNMLVCUSTOMDRAW)lParam;
    switch (lplvcd->nmcd.dwDrawStage)
    {
        case CDDS_PREPAINT:
        case CDDS_ITEMPREPAINT:
        {
            dwRet=CDRF_NOTIFYSUBITEMDRAW;
            break;
        }
        case CDDS_ITEMPREPAINT|CDDS_SUBITEM:
        {
            lplvcd->clrTextBk=RGB(225,225,225);
            lplvcd->clrText=RGB(0,0,0);
            dwRet=CDRF_NEWFONT;

            switch (lplvcd->iSubItem)
            {
                case 0:
                case 2:
                {
                    lplvcd->clrTextBk=GetSysColor(COLOR_BTNFACE);
                    break;
                }
            }

            if (!(lplvcd->iSubItem % 2))
                break;

            switch (lplvcd->nmcd.dwItemSpec)
            {
                case 2:
                {
                    lplvcd->clrText=RGB(0,153,0);
                    break;
                }
                case 3:
                {
                    lplvcd->clrText=RGB(153,0,0);
                    break;
                }
                case 4:
                {
                    lplvcd->clrText=RGB(128,128,128);
                    break;
                }
            }
            break;
        }
    }
    return dwRet;
}

static LRESULT CALLBACK StatWndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    WNDPROC lpWndProc=(WNDPROC)GetWindowLongPtr(hWnd,GWLP_USERDATA);
    switch (uMsg)
    {
        case WM_MOUSEMOVE:
        {
            if (wParam == MK_LBUTTON)
            {
                ReleaseCapture();
                SendMessage(hCommonDlg,WM_NCLBUTTONDOWN,HTCAPTION,0);
            }
            break;
        }
    }
    return CallWindowProc(lpWndProc,hWnd,uMsg,wParam,lParam);
}

void InitDlg()
{
    cJSON *jsDlg=cJSON_GetObjectItem(jsSettings,"checker_dlg");
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

    HWND hStat=GetDlgItem(hCommonDlg,IDC_STAT);
    SendMessage(hStat,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_GRIDLINES|LVS_EX_FLATSB|LVS_EX_LABELTIP|LVS_EX_DOUBLEBUFFER);

    LOGFONT lf;
    GetObject((HFONT)SendMessage(hStat,WM_GETFONT,NULL,NULL),sizeof(lf),&lf);

    lf.lfWeight=FW_BOLD;;
    HFONT hBoldFont=CreateFontIndirect(&lf);
    SendMessage(hStat,WM_SETFONT,(WPARAM)hBoldFont,true);

    LVCOLUMN lvc={0};
    lvc.mask=LVCF_WIDTH;
    lvc.cx=100;
    SendMessage(hStat,LVM_INSERTCOLUMN,0,(LPARAM)&lvc);
    SendMessage(hStat,LVM_INSERTCOLUMN,1,(LPARAM)&lvc);
    SendMessage(hStat,LVM_SETBKCOLOR,0,GetSysColor(COLOR_BTNFACE));

    LVITEMA lvi={0};
    lvi.mask=LVIF_TEXT;
    lvi.cchTextMax=MAX_PATH;
    GridView_AddItem(hStat,&lvi,"Active tasks","1/1");
    GridView_AddItem(hStat,&lvi,"Total checked","0");
    GridView_AddItem(hStat,&lvi,"RFB","0");
    GridView_AddItem(hStat,&lvi,"Non-RFB","0");
    GridView_AddItem(hStat,&lvi,"Unavailble","0");
    sd.dwActiveTasks=1;

    SendMessage(hStat,LVM_SETIMAGELIST,LVSIL_SMALL,(LPARAM)ImageList_Create(1,24,0,1,1));
    SetWindowLongPtr(hStat,GWLP_USERDATA,(LONG_PTR)SetWindowLongPtr(hStat,GWLP_WNDPROC,(LONG_PTR)StatWndProc));
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
        hShowEvent=CreateEvent(NULL,false,false,_T("rfb.brute.ng.checker.show.event"));

        CreateMutex(NULL,false,_T("rfb.brute.ng.checker.protection.mutex"));
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

