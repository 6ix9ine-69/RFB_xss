#include "includes.h"

#include "common\stopping_dlg.h"
#include "common\settings.h"
#include "common\sticky.h"
#include "common\socket.h"

#include "commands.h"
#include "results_dlg.h"
#include "common_dlg.h"
#include "backup.h"
#include "debug_log_dlg.h"
#include "iplist.h"

SERVER_INFO siWorker,siChecker;

HANDLE hShutdownEvent,hThreadsGroup;
HINSTANCE hInstance;

void UpdateMenuState(HMENU hMenu,DWORD dwItem,bool bChecked)
{
    DWORD dwState=MF_BYCOMMAND;
    if (bChecked)
        dwState|=MF_CHECKED;

    CheckMenuItem(hMenu,dwItem,dwState);
    return;
}

void CalcDHMS(DWORD dwSecs,LPSTR lpTime)
{
    dwSecs=Now()-dwSecs;

    DWORD d=dwSecs/(3600*24),
          h=dwSecs%(3600*24)/3600,
          m=dwSecs%3600/60,
          s=dwSecs%60;

    do
    {
        if ((!d) && (!h) && (!m))
        {
            wsprintfA(lpTime,"%ds.",s);
            break;
        }

        if ((!d) && (!h))
        {
            wsprintfA(lpTime,"%.2d:%.2d",m,s);
            break;
        }

        if (!d)
        {
            wsprintfA(lpTime,"%.2d:%.2d:%.2d",h,m,s);
            break;
        }

        wsprintfA(lpTime,"%d:%.2d:%.2d:%.2d",d,h,m,s);
    }
    while (false);
    return;
}

typedef struct _TABS
{
    LPTSTR lpTitle;
    LPCTSTR lpID;
    DLGPROC lpProc;
    CONNECTION_TYPE dwType;
} TABS, *PTABS;

void PostThreadMessageEx(DWORD idThread,UINT Msg,WPARAM wParam,LPARAM lParam)
{
    while (true)
    {
        if (PostThreadMessage(idThread,Msg,wParam,lParam))
            break;

        if (GetLastError() != ERROR_NOT_ENOUGH_QUOTA)
            break;

        if (WaitForSingleObject(hShutdownEvent,100) == WAIT_OBJECT_0)
            break;
    }
    return;
}

cJSON *jsSettings;
void StoreDlgPos()
{
    do
    {
        if (!IsWindowVisible(hMainDlg))
            break;

        WINDOWPLACEMENT wp;
        wp.length=sizeof(wp);

        if (!GetWindowPlacement(hMainDlg,&wp))
            break;

        cJSON *jsDlg=cJSON_GetObjectItem(jsSettings,"manager_dlg");
        if (!jsDlg)
        {
            jsDlg=cJSON_CreateObject();
            cJSON_AddItemToObject(jsSettings,"manager_dlg",jsDlg);
        }

        Setting_ChangeValue(jsDlg,"x",wp.rcNormalPosition.left);
        Setting_ChangeValue(jsDlg,"y",wp.rcNormalPosition.top);
        Setting_ChangeValue(jsDlg,"width",wp.rcNormalPosition.right-wp.rcNormalPosition.left);
        Setting_ChangeValue(jsDlg,"height",wp.rcNormalPosition.bottom-wp.rcNormalPosition.top);

        Setting_ChangeValue(jsDlg,"maximized",IsZoomed(hMainDlg));
        Setting_ChangeValue(jsDlg,"top_most",(GetWindowLong(hMainDlg,GWL_EXSTYLE) & WS_EX_TOPMOST));

        DWORD dwTab=SendDlgItemMessage(hMainDlg,IDC_TAB,TCM_GETCURSEL,NULL,NULL);
        if (dwTab != -1)
            Setting_ChangeValue(jsDlg,"tab",dwTab);

        Settings_Save(jsSettings);
    }
    while (false);
    return;
}

static HMENU hSysMenu,hMenu;
void EnableDBMainanceMenu(bool bEnable)
{
    DWORD dwState=MF_BYCOMMAND;
    if (!bEnable)
        dwState|=MF_DISABLED|MF_GRAYED;

    DWORD dwItems[]=
    {
        IDM_RESCAN_IGNORED,
        IDM_REBUILD_BAD,
        IDM_REBUID_IGNORED,
        IDM_REBUID_GOOD,
        IDM_REBUILD_IGNORE,
        IDM_RESCAN_UNSUPPORTED,
        IDM_REBUILD_TOTAL_IPS,
        IDM_CHECKER_PURGE_DB,
    };

    for (DWORD i=0; i < ARRAYSIZE(dwItems); i++)
        EnableMenuItem(hMenu,dwItems[i],dwState);
    return;
}

static void DoDBMainance(DWORD dwMsg)
{
    DWORD dwThreadMsg=0;
    switch (dwMsg)
    {
        case IDM_RESCAN_IGNORED:
        {
            dwThreadMsg=TM_DB_MAINTANCE_RESCAN_INGORED;
            break;
        }
        case IDM_REBUILD_BAD:
        {
            dwThreadMsg=TM_DB_MAINTANCE_REBUILD_BAD;
            break;
        }
        case IDM_REBUID_IGNORED:
        {
            dwThreadMsg=TM_DB_MAINTANCE_REBUILD_IGNORED;
            break;
        }
        case IDM_REBUID_GOOD:
        {
            dwThreadMsg=TM_DB_MAINTANCE_REBUILD_GOOD;
            break;
        }
        case IDM_REBUILD_IGNORE:
        {
            dwThreadMsg=TM_DB_MAINTANCE_REBUILD_IGNORE_MASKS;
            break;
        }
        case IDM_RESCAN_UNSUPPORTED:
        {
            dwThreadMsg=TM_DB_MAINTANCE_RESCAN_UNSUPPORTED;
            break;
        }
        case IDM_REBUILD_TOTAL_IPS:
        {
            dwThreadMsg=TM_DB_MAINTANCE_REBUILD_TOTAL_IPS;
            break;
        }
        case IDM_CHECKER_PURGE_DB:
        {
            dwThreadMsg=IM_DB_MAINTANCE_PURGE_CHECKER_DB_BEGIN;
            break;
        }
    }

    if (dwThreadMsg)
        PostThreadMessageEx(dwDBUpdateThreadId,dwThreadMsg,NULL,NULL);
    return;
}

static DWORD dwStartedAt;
HWND hMainDlg;
static HHOOK hHook;
static HACCEL hAccel;
static LRESULT CALLBACK HookProc(int nCode,WPARAM wParam,LPARAM lParam)
{
    LRESULT dwResult=1;
    do
    {
        if ((nCode != HC_ACTION) || (wParam != PM_REMOVE))
            break;

        LPMSG lpMsg=(LPMSG)lParam;
        if ((lpMsg->message < WM_KEYFIRST) || (lpMsg->message > WM_KEYLAST))
            break;

        if (GetForegroundWindow() != hMainDlg)
            break;

        if (!TranslateAcceleratorW(hMainDlg,hAccel,lpMsg))
            break;

        lpMsg->message=WM_NULL;
        dwResult=0;
    }
    while (false);

    if ((nCode < 0) || (dwResult))
        dwResult=CallNextHookEx(hHook,nCode,wParam,lParam);

    return dwResult;
}

static void SwitchToTab(HWND hDlg,DWORD dwIdx)
{
    HWND hTab=GetDlgItem(hDlg,IDC_TAB);
    if (dwIdx == -1)
    {
        DWORD dwCurTab=TabCtrl_GetCurSel(hTab),
              dwTabs=TabCtrl_GetItemCount(hTab);

        if (dwCurTab == dwTabs-1)
            dwIdx=0;
        else
            dwIdx=dwCurTab+1;
    }

    NMHDR hdr={0};
    hdr.hwndFrom=hTab;
    hdr.code=TCN_SELCHANGING;
    hdr.idFrom=IDC_TAB;
    SendMessage(hDlg,WM_NOTIFY,IDC_TAB,(LPARAM)&hdr);
    TabCtrl_SetCurSel(hTab,dwIdx);

    hdr.code=TCN_SELCHANGE;
    SendMessage(hDlg,WM_NOTIFY,IDC_TAB,(LPARAM)&hdr);
    return;
}

bool bStopping=false;
static INT_PTR CALLBACK MainDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    INT_PTR dwRet=FALSE;

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            SendMessage(hDlg,WM_SETICON,ICON_BIG,(LPARAM)LoadIcon(hInstance,MAKEINTRESOURCE(100)));

            hContextMenuRoot=GetSubMenu(LoadMenu(hInstance,MAKEINTRESOURCE(IDR_CONTEXT_ROOT)),0);
            hContextMenuItem=GetSubMenu(LoadMenu(hInstance,MAKEINTRESOURCE(IDR_CONTEXT_ITEM)),0);
            hMenu=GetMenu(hDlg);

            hSysMenu=GetSystemMenu(hDlg,false);
            InsertMenu(hSysMenu,5,MF_BYPOSITION|MF_SEPARATOR,0,NULL);
            InsertMenu(hSysMenu,6,MF_BYPOSITION|MF_STRING,SYSMENU_TOPMOST_ID,_T("Always on top"));

            TABS Tabs[]=
            {
                {_T("Workers"),MAKEINTRESOURCE(IDD_COMMON),CommonTabDlgProc,TYPE_WORKER},
                {_T("Checkers"),MAKEINTRESOURCE(IDD_COMMON),CommonTabDlgProc,TYPE_CHECKER},
                {_T("Results"),MAKEINTRESOURCE(IDD_RESULTS),ResultsTabDlgProc,TYPE_NONE},
                {_T("Debug log"),MAKEINTRESOURCE(IDD_DEBUG_LOG),DebugLogDlgProc,TYPE_NONE},
            };

            HWND hTabs=GetDlgItem(hDlg,IDC_TABS),
                 hTab=GetDlgItem(hDlg,IDC_TAB);

            TCITEM tci={0};
            tci.mask=TCIF_TEXT|TCIF_PARAM;
            tci.cchTextMax=MAX_PATH;

            cJSON *jsDlg=cJSON_GetObjectItem(jsSettings,"manager_dlg");
            if (jsDlg)
            {
                DWORD dwTabIdx=cJSON_GetIntFromObject(jsDlg,"tab");
                if (dwTabIdx < ARRAYSIZE(Tabs))
                    dwCurTab=Tabs[dwTabIdx].dwType;
            }

            for (DWORD i=0; i < ARRAYSIZE(Tabs); i++)
            {
                PCOMM_TAB lpParam=(PCOMM_TAB)MemAlloc(sizeof(*lpParam));
                if (!lpParam)
                    break; /// !!!!!

                lpParam->dwConnectionType=Tabs[i].dwType;
                lpParam->dwTabIdx=i;

                tci.pszText=Tabs[i].lpTitle;
                tci.lParam=(LPARAM)CreateDialogParamW(hInstance,Tabs[i].lpID,hTabs,Tabs[i].lpProc,(LPARAM)lpParam);

                SendMessage(hTab,TCM_INSERTITEM,i,(LPARAM)&tci);
            }

            if (jsDlg)
            {
                DWORD dwTabIdx=cJSON_GetIntFromObject(jsDlg,"tab");
                if (dwTabIdx < ARRAYSIZE(Tabs))
                {
                    TabCtrl_SetCurSel(hTab,dwTabIdx);
                    dwCurTab=Tabs[dwTabIdx].dwType;
                }

                SetWindowPos(hDlg,NULL,cJSON_GetIntFromObject(jsDlg,"x"),
                                       cJSON_GetIntFromObject(jsDlg,"y"),
                                       cJSON_GetIntFromObject(jsDlg,"width"),
                                       cJSON_GetIntFromObject(jsDlg,"height"),0);

                if (cJSON_GetIntFromObject(jsDlg,"maximized"))
                    ShowWindow(hDlg,SW_MAXIMIZE);

                if (cJSON_GetIntFromObject(jsDlg,"top_most"))
                    PostMessage(hDlg,WM_SYSCOMMAND,SYSMENU_TOPMOST_ID,NULL);
            }

            PostMessage(hDlg,WM_SIZE,NULL,NULL);

            tci.mask=TCIF_PARAM;
            SendMessage(hTab,TCM_GETITEM,SendMessage(hTab,TCM_GETCURSEL,NULL,NULL),(LPARAM)&tci);
            ShowWindow((HWND)tci.lParam,SW_SHOW);

            int iParts[3];
            iParts[0]=185;
            iParts[1]=iParts[0]+185;
            iParts[2]=-1;
            SendDlgItemMessage(hDlg,IDC_SBR1,SB_SETPARTS,ARRAYSIZE(iParts),(LPARAM)iParts);
            SendDlgItemMessage(hDlg,IDC_SBR1,SB_SETTEXT,1,(LPARAM)_T("DB updated: never"));
            SendDlgItemMessage(hDlg,IDC_SBR1,SB_SETTEXT,2,(LPARAM)_T("Uptime: 0s."));

            Backup_UpdateNextBackupTime(hDlg);
            Backup_UpdateMenuState(hMenu,-1);

            Checker_UpdateMenuState(hMenu,-1);

            DebugLog_UpdateMenuState(hMenu,-1);

            hMainDlg=hDlg;

            dwStartedAt=Now();
            SetTimer(hDlg,IDT_UPDATE_UPTIME,MILLISECONDS_PER_SECOND,NULL);

            hHook=SetWindowsHookExW(WH_GETMESSAGE,HookProc,hInstance,GetCurrentThreadId());
            DB_UpdateMenuState(hMenu,-1);
            break;
        }
        case WM_NCDESTROY:
        {
            UnhookWindowsHookEx(hHook);
            break;
        }
        case_WM_SICKY:
        {
            HandleStickyMsg(hDlg,uMsg,wParam,lParam);
            break;
        }
        case WM_NOTIFY:
        {
            LPNMHDR pnmh=(LPNMHDR)lParam;
            switch (pnmh->code)
            {
                case NM_CUSTOMDRAW:
                {
                    SetWindowLongPtr(hDlg,DWLP_MSGRESULT,(LONG_PTR)GridView_ProcessCustomDraw(lParam,false));
                    dwRet=true;
                    break;
                }
                case TCN_SELCHANGING:
                {
                    TCITEM tci={0};
                    tci.mask=TCIF_PARAM;
                    TabCtrl_GetItem(pnmh->hwndFrom,TabCtrl_GetCurSel(pnmh->hwndFrom),&tci);
                    ShowWindow((HWND)tci.lParam,SW_HIDE);
                    break;
                }
                case TCN_SELCHANGE:
                {
                    DWORD dwTabsCount=TabCtrl_GetItemCount(pnmh->hwndFrom)-1;

                    TabCtrl_HighlightItem(pnmh->hwndFrom,TabCtrl_GetCurSel(pnmh->hwndFrom),FALSE);

                    TCITEM tci={0};
                    tci.mask=TCIF_PARAM;
                    TabCtrl_GetItem(pnmh->hwndFrom,TabCtrl_GetCurSel(pnmh->hwndFrom),&tci);

                    HWND hTab=(HWND)tci.lParam;

                    dwCurTab=TYPE_NONE;
                    PCOMM_TAB lpTab=(PCOMM_TAB)GetWindowLongPtr(hTab,GWLP_USERDATA);
                    if (lpTab)
                        dwCurTab=lpTab->dwConnectionType;

                    ShowWindow(hTab,SW_SHOW);
                    SetFocus(hTab);

                    HTREEITEM hItem=(HTREEITEM)SendDlgItemMessage(hTab,IDC_TAB,TVM_GETNEXTITEM,TVGN_CARET,NULL);
                    if (!hItem)
                        break;

                    PCONNECTION lpConnection=lpConnections;
                    while (lpConnection)
                    {
                        if (lpConnection->hItem == hItem)
                        {
                            UpdateStats(lpConnection);
                            break;
                        }
                        lpConnection=lpConnection->lpNext;
                    }
                    break;
                }
                case LVN_ITEMCHANGED:
                {
                    LVITEM lvi;
                    lvi.stateMask=LVIS_SELECTED|LVIS_FOCUSED;
                    lvi.state=0;
                    SendDlgItemMessage(hDlg,IDC_STAT,LVM_SETITEMSTATE,-1,(LPARAM)&lvi);
                    break;
                }
            }
            break;
        }
        case WM_TIMER:
        {
            switch (wParam)
            {
                case IDT_UPDATE_UPTIME:
                {
                    char szTime[MAX_PATH];
                    CalcDHMS(dwStartedAt,szTime);

                    char szUptime[MAX_PATH];
                    wsprintfA(szUptime,"Uptime: %s",szTime);

                    SendDlgItemMessageA(hDlg,IDC_SBR1,SB_SETTEXTA,2,(LPARAM)szUptime);
                    break;
                }
                case IDT_WAIT_RESULTS:
                {
                    PCONNECTION lpConnection=lpConnections;
                    while (lpConnection)
                    {
                        if (lpConnection->Socket.bConnected)
                            break;

                        lpConnection=lpConnection->lpNext;
                    }

                    if (lpConnection)
                        break;

                    lpConnection=lpConnections;
                    while (lpConnection)
                    {
                        PostThreadMessage(lpConnection->dwCmdParsingThreadId,WM_QUIT,NULL,NULL);

                        lpConnection=lpConnection->lpNext;
                    }

                    KillTimer(hDlg,IDT_WAIT_RESULTS);

                    DWORD dwIems=SendDlgItemMessage(hDlg,IDC_TAB,TCM_GETITEMCOUNT,NULL,NULL);
                    for (DWORD i=0; i < dwIems; i++)
                    {
                        TCITEM tci={0};
                        tci.mask=TCIF_PARAM;
                        SendDlgItemMessage(hDlg,IDC_TAB,TCM_GETITEM,i,(LPARAM)&tci);

                        SendMessage((HWND)tci.lParam,WM_CLOSE,NULL,NULL);
                    }

                    hMainDlg=NULL;
                    SetEvent(hShutdownEvent);
                    EndDialog(hDlg,0);
                    break;
                }
            }
            break;
        }
        case WM_COMMAND:
        {
            HWND hTab=GetDlgItem(hDlg,IDC_TAB);
            DWORD dwCurTab=TabCtrl_GetCurSel(hTab);

            switch (LOWORD(wParam))
            {
                case IDM_NEXT_TAB:
                {
                    SwitchToTab(hDlg,-1);
                    break;
                }
                case IDM_TAB_1:
                case IDM_TAB_2:
                case IDM_TAB_3:
                case IDM_TAB_4:
                {
                    SwitchToTab(hDlg,LOWORD(wParam)-IDM_TAB_1);
                    break;
                }
                case IDM_RESULT_COPY_ADDRESS:
                case IDM_RESULT_COPY_PASSWORD:
                case IDM_RESULT_COPY_DESKTOP:
                {
                    if (dwCurTab != 2)
                        break;

                    TCITEM tci={0};
                    tci.mask=TCIF_PARAM;
                    TabCtrl_GetItem(hTab,dwCurTab,&tci);

                    PostMessage((HWND)tci.lParam,WM_COMMAND,wParam,lParam);
                    break;
                }
                case IDM_SHOW_ALL:
                case IDM_LOG_CLEAR:
                {
                    if (dwCurTab == 2)
                        break;

                    TCITEM tci={0};
                    tci.mask=TCIF_PARAM;
                    TabCtrl_GetItem(hTab,dwCurTab,&tci);

                    PostMessage((HWND)tci.lParam,WM_COMMAND,wParam,lParam);
                    break;
                }
                case IDM_EXIT:
                {
                    PostMessage(hDlg,WM_CLOSE,NULL,NULL);
                    break;
                }
                case IDM_BACKUP_NOW:
                {
                    Backup_BackupNow();
                    break;
                }
                case IDM_RESCAN_IGNORED:
                {
                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Rescan ignored"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    DoDBMainance(wParam);
                    break;
                }
                case IDM_REBUILD_BAD:
                {
                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Rebuild bad"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    DoDBMainance(wParam);
                    break;
                }
                case IDM_REBUID_IGNORED:
                {
                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Rebuild ignored list"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    DoDBMainance(wParam);
                    break;
                }
                case IDM_REBUID_GOOD:
                {
                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Rebuild good"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    DoDBMainance(wParam);
                    break;
                }
                case IDM_REBUILD_IGNORE:
                {
                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Rebuild ignore masks"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    DoDBMainance(wParam);
                    break;
                }
                case IDM_RESCAN_UNSUPPORTED:
                {
                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Rescan unsupported RFBs"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    DoDBMainance(wParam);
                    break;
                }
                case IDM_REBUILD_TOTAL_IPS:
                {
                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Rebuid total IP's"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    DoDBMainance(wParam);
                    break;
                }
                case IDM_REBUILD_CHECKERS_ON_EXIT:
                case IDM_CLEAR_UNSUPPORTED_ON_EXIT:
                {
                    DB_UpdateMenuState(hMenu,wParam);
                    break;
                }
                case IDM_CHECKER_PURGE_DB:
                {
                    if (!InterlockedExchangeAdd(&dwItemsToCheck,0))
                        break;

                    if (Checker_IsReadingNew())
                    {
                        DebugLog_AddItem2(NULL,LOG_LEVEL_WARNING,"Cant purge checker DB while reading new items");
                        break;
                    }

                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Checker DB purge"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    DoDBMainance(wParam);
                    break;
                }
                case IDM_CHECKER_REMEMBER_LAST_ID:
                {
                    Checker_UpdateMenuState(hMenu,wParam);
                    break;
                }
                case IDM_LEVEL_INFO:
                case IDM_LEVEL_WARNING:
                case IDM_LEVEL_ERROR:
                case IDM_LEVEL_CONNECTION:
                {
                    DebugLog_UpdateMenuState(hMenu,wParam);
                    break;
                }
                case IDM_BACKUP_ONLOAD:
                case IDM_BACKUP_ONUNLOAD:
                {
                    Backup_UpdateMenuState(hMenu,wParam);
                    break;
                }
                case IDCANCEL:
                {
                    TCITEM tci={0};
                    tci.mask=TCIF_PARAM;
                    TabCtrl_GetItem(hTab,dwCurTab,&tci);

                    bool bSplitting=false;
                    SendMessage((HWND)tci.lParam,WM_COMMAND,CMD_IS_SPLITTING,(LPARAM)&bSplitting);
                    if (bSplitting)
                    {
                        SendMessage((HWND)tci.lParam,WM_COMMAND,CMD_CANCEL_SPLIT,NULL);
                        break;
                    }

                    PostMessage(hDlg,WM_CLOSE,NULL,NULL);
                    break;
                }
                case CMD_NOTIFY_TAB:
                {
                    if (dwCurTab == lParam)
                        break;

                    TabCtrl_HighlightItem(hTab,lParam,TRUE);
                    break;
                }
            }
            break;
        }
        case WM_SIZE:
        {
            SendDlgItemMessageW(hDlg,IDC_SBR1,WM_SIZE,NULL,NULL);

            RECT rc;
            GetClientRect(hDlg,&rc);

            MoveWindow(GetDlgItem(hDlg,IDC_TAB),14,10,rc.right-28,21,true);
            MoveWindow(GetDlgItem(hDlg,IDC_TABS),14,10+28,rc.right-28,rc.bottom-(10+28+30),true);

            DWORD dwIems=SendDlgItemMessage(hDlg,IDC_TAB,TCM_GETITEMCOUNT,NULL,NULL);
            for (DWORD i=0; i < dwIems; i++)
            {
                TCITEM tci={0};
                tci.mask=TCIF_PARAM;
                SendDlgItemMessage(hDlg,IDC_TAB,TCM_GETITEM,i,(LPARAM)&tci);

                PostMessage((HWND)tci.lParam,WM_SIZE,NULL,NULL);
            }
            break;
        }
        case WM_GETMINMAXINFO:
        {
            LPMINMAXINFO lpMinInfo=(LPMINMAXINFO)lParam;
            lpMinInfo->ptMinTrackSize.x=450;
            lpMinInfo->ptMinTrackSize.y=324;
            return 0;
        }
        case WM_SYSCOMMAND:
        {
            if (wParam != SYSMENU_TOPMOST_ID)
                break;

            DWORD dwExStyle=GetWindowLongPtr(hDlg,GWL_EXSTYLE);
            HWND hTopMost;
            if (!(GetMenuState(hSysMenu,SYSMENU_TOPMOST_ID,MF_BYCOMMAND) & MF_CHECKED))
            {
                CheckMenuItem(hSysMenu,SYSMENU_TOPMOST_ID,MF_BYCOMMAND|MF_CHECKED);
                dwExStyle|=WS_EX_TOPMOST;
                hTopMost=HWND_TOPMOST;
            }
            else
            {
                CheckMenuItem(hSysMenu,SYSMENU_TOPMOST_ID,MF_BYCOMMAND|MF_UNCHECKED);
                dwExStyle&=~WS_EX_TOPMOST;
                hTopMost=HWND_NOTOPMOST;
            }

            SetWindowLongPtr(hDlg,GWL_EXSTYLE,dwExStyle);
            SetWindowPos(hDlg,hTopMost,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE|SWP_FRAMECHANGED);

            BringWindowToTop(hDlg);
            SetForegroundWindow(hDlg);
            break;
        }
        case WM_CLOSE:
        {
            dwRet=TRUE;

            if (bStopping)
                break;

            if (MessageBox(hDlg,_T("Are you sure?"),_T("Stop work"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                break;

            if (Checker_IsReadingNew())
            {
                if (MessageBox(hDlg,_T("Checker is reading new items. Still want to stop?"),_T("Stop work"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                    break;
            }

            if (IPList_IsReading())
            {
                if (MessageBox(hDlg,_T("Manager is reading new items from list. Still want to stop?"),_T("Stop work"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                    break;
            }

            StoreDlgPos();

            EnableMenuItem(GetSystemMenu(hDlg,false),SC_CLOSE,MF_BYCOMMAND|MF_DISABLED|MF_GRAYED);
            bStopping=true;

            DWORD dwIems=SendDlgItemMessage(hDlg,IDC_TAB,TCM_GETITEMCOUNT,NULL,NULL);
            for (DWORD i=0; i < dwIems; i++)
            {
                TCITEM tci={0};
                tci.mask=TCIF_PARAM;
                SendDlgItemMessage(hDlg,IDC_TAB,TCM_GETITEM,i,(LPARAM)&tci);

                PostMessage((HWND)tci.lParam,WM_COMMAND,CMD_EXIT,NULL);
            }

            SetTimer(hDlg,IDT_WAIT_RESULTS,10,NULL);
            CreateDialogParamW(hInstance,MAKEINTRESOURCE(IDD_STOPPING),hDlg,StoppingDlgProc,NULL);
            break;
        }
    }
    return dwRet;
}

static void Server_InitInt(cJSON *jsServer,LPCSTR lpType,PSERVER_INFO lpItem)
{
    lpItem->hSock=INVALID_SOCKET;

    do
    {
        cJSON *jsItem=cJSON_GetObjectItem(jsServer,lpType);
        if (!jsItem)
            break;

        WORD wPort=(WORD)cJSON_GetIntFromObject(jsItem,"port");
        if (!wPort)
            break;

        DWORD dwInterface=0;
        LPCSTR lpInterface=cJSON_GetStringFromObject(jsServer,"interface");
        if (lpInterface)
            dwInterface=NetResolveAddress(lpInterface);

        lpItem->hSock=SocketBindOnPort(wPort,dwInterface);
        lpItem->jsServer=jsItem;
    }
    while (false);
    return;
}

static bool Server_Init(cJSON *jsConfig)
{
    bool bRet=false;

    do
    {
        cJSON *jsServer=cJSON_GetObjectItem(jsConfig,"server");
        if (!jsServer)
        {
            MessageBoxA(GetForegroundWindow(),"Server_Init() failed!",NULL,MB_ICONEXCLAMATION);
            break;
        }

        Server_InitInt(jsServer,"worker",&siWorker);
        if (siWorker.hSock == INVALID_SOCKET)
        {
            MessageBoxA(GetForegroundWindow(),"SocketListenOnPort(\"worker\") failed!",NULL,MB_ICONEXCLAMATION);
            break;
        }

        Server_InitInt(jsServer,"checker",&siChecker);
        if (siChecker.hSock == INVALID_SOCKET)
        {
            MessageBoxA(GetForegroundWindow(),"SocketListenOnPort(\"checker\") failed!",NULL,MB_ICONEXCLAMATION);
            break;
        }

        bRet=true;
    }
    while (false);
    return bRet;
}

void main()
{
    do
    {
        CreateMutex(NULL,false,_T("rfb.brute.ng.manager.protection.mutex"));
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            MessageBox(NULL,_T("Already running!"),NULL,MB_ICONEXCLAMATION);
            break;
        }

        cJSON *jsConfig=Config_Parse();
        if (!jsConfig)
            break;

        hInstance=GetModuleHandle(NULL);

        hShutdownEvent=CreateEvent(NULL,true,false,NULL);
        hThreadsGroup=ThreadsGroup_Create();

        if (!DB_Init(jsConfig))
        {
            MessageBox(NULL,_T("DB_Init() failed!"),NULL,MB_ICONEXCLAMATION);
            break;
        }

        if (!LoadLibrary(_T("riched20.dll")))
        {
            MessageBox(NULL,_T("LoadLibrary(\"riched20.dll\") failed!"),NULL,MB_ICONEXCLAMATION);
            break;
        }

        WSADATA wsa;
        WSAStartup(MAKEWORD(2,2),&wsa);

        if (!Server_Init(jsConfig))
            break;

        Connections_Init();
        Splash_Start();
        DebugLog_Init(jsConfig);
        Backup_Init(jsConfig);
        Notify_Init(jsConfig);
        Checker_Init(jsConfig);
        Worker_Init(siWorker.jsServer);
        Splash_Stop();

        SetThreadExecutionState(ES_CONTINUOUS|ES_SYSTEM_REQUIRED|ES_DISPLAY_REQUIRED);

        bHelloStat=cJSON_GetBoolFromObject(jsConfig,"hello_stat");

        jsSettings=Settings_Init(jsConfig);
        hAccel=LoadAcceleratorsW(hInstance,MAKEINTRESOURCE(IDR_ACCEL));
        DialogBoxParamW(hInstance,MAKEINTRESOURCE(IDD_MAIN),NULL,(DLGPROC)MainDlgProc,NULL);

        Config_Update();
        Splash_Start();
        Splash_SetText(_T("Waiting for stop common threads..."));

        ThreadsGroup_WaitForAllExit(hThreadsGroup,INFINITE);
        ThreadsGroup_CloseGroup(hThreadsGroup);

        Backup_Cleanup();
        DB_Cleanup();
        Splash_Stop();

        cJSON_Delete(jsConfig);
    }
    while (false);
    ExitProcess(0);
}

