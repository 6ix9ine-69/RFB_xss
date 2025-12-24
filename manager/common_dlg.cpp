#include "includes.h"

#include <syslib\str.h>

#include "common\settings.h"
#include "common\json_packet.h"

#include "results_dlg.h"
#include "common_dlg.h"
#include "commands.h"

static bool IsShowAll(cJSON *jsConfig)
{
    cJSON *jsShowAll=cJSON_GetObjectItem(jsConfig,"show_all");
    if (!jsShowAll)
    {
        jsShowAll=cJSON_CreateTrue();
        cJSON_AddItemToObject(jsConfig,"show_all",jsShowAll);
    }

    return cJSON_IsTrue(jsShowAll);
}

static HTREEITEM AddItem(HWND hTab,PCONNECTION lpConnection,HTREEITEM hParent,HTREEITEM hInsertAfter)
{
    char szAddr[MAX_PATH];

    TVINSERTSTRUCTA tvi={0};
    tvi.hInsertAfter=hInsertAfter;
    tvi.hParent=hParent;
    tvi.item.mask=TVIF_TEXT|TVIF_SELECTEDIMAGE|TVIF_IMAGE|TVIF_PARAM;
    tvi.item.lParam=(LPARAM)lpConnection;
    tvi.item.iSelectedImage=tvi.item.iImage=lpConnection->dwPrevStatus;
    tvi.item.pszText=szAddr;
    tvi.item.cchTextMax=MAX_PATH;

    if (lpConnection->szAlias[0])
        lstrcpyA(szAddr,lpConnection->szAlias);
    else
        lstrcpyA(szAddr,NetNtoA(lpConnection->Socket.si.sin_addr.s_addr));

    return (HTREEITEM)SendMessageA(hTab,TVM_INSERTITEMA,NULL,(LPARAM)&tvi);
}

static void ShowAll(HWND hTab,CONNECTION_TYPE dwType)
{
    PCONNECTION lpRootConnection=lpConnections;
    while (lpRootConnection)
    {
        if ((lpRootConnection->dwType == dwType) && (lpRootConnection->bRoot))
            break;

        lpRootConnection=lpRootConnection->lpNext;
    }

    HTREEITEM hPrevItem=TVI_FIRST;
    PCONNECTION lpConnection=lpConnections;
    while (lpConnection)
    {
        if ((lpConnection->dwType == dwType) && (!lpConnection->bRoot))
        {
            if (!lpConnection->hItem)
                hPrevItem=lpConnection->hItem=AddItem(hTab,lpConnection,lpRootConnection->hItem,hPrevItem);
            else
                hPrevItem=lpConnection->hItem;
        }
        lpConnection=lpConnection->lpNext;
    }

    SendMessage(hTab,TVM_EXPAND,TVE_EXPAND,(LPARAM)lpRootConnection->hItem);
    return;
}

static void RebuildTreeView(HWND hTab,CONNECTION_TYPE dwType)
{
    PCONNECTION lpRootConnection=lpConnections;
    while (lpRootConnection)
    {
        if ((lpRootConnection->dwType == dwType) && (lpRootConnection->bRoot))
            break;

        lpRootConnection=lpRootConnection->lpNext;
    }

    HTREEITEM hPrevItem=TVI_FIRST,hSelected=(HTREEITEM)SendMessage(hTab,TVM_GETNEXTITEM,TVGN_CARET,(LPARAM)TVI_ROOT);
    PCONNECTION lpConnection=lpConnections;
    while (lpConnection)
    {
        if ((lpConnection->dwType == dwType) && (!lpConnection->bRoot))
        {
            if (lpConnection->Socket.bConnected)
            {
                /// новый подключился
                if (!lpConnection->hItem)
                    hPrevItem=lpConnection->hItem=AddItem(hTab,lpConnection,lpRootConnection->hItem,hPrevItem);
                else
                    hPrevItem=lpConnection->hItem;
            }
            else
            {
                /// отключился старый
                if (lpConnection->hItem)
                {
                    if (hSelected == lpConnection->hItem)
                        SendMessage(hTab,TVM_SELECTITEM,TVGN_CARET,(LPARAM)lpRootConnection->hItem);

                    SendMessageW(hTab,TVM_DELETEITEM,NULL,(LPARAM)lpConnection->hItem);
                    lpConnection->hItem=NULL;
                }
            }
        }
        lpConnection=lpConnection->lpNext;
    }

    SendMessage(hTab,TVM_EXPAND,TVE_EXPAND,(LPARAM)lpRootConnection->hItem);
    return;
}

static void UpdateTreeView(HWND hDlg,PCOMM_TAB lpParam)
{
    HWND hTab=GetDlgItem(hDlg,IDC_TAB);

    if (!IsShowAll(lpParam->jsConfig))
        RebuildTreeView(hTab,lpParam->dwConnectionType);
    else
        ShowAll(hTab,lpParam->dwConnectionType);
    return;
}

static bool IsRoot(HWND hTab,HTREEITEM hItem)
{
    bool bRet=false;

    TVITEM tvi={0};
    tvi.mask=TVIF_PARAM;
    tvi.hItem=hItem;
    if (SendMessage(hTab,TVM_GETITEM,NULL,(LPARAM)&tvi))
    {
        if (tvi.lParam)
            bRet=((PCONNECTION)tvi.lParam)->bRoot;
    }
    return bRet;
}

void SetItemBold(PCONNECTION lpConnection)
{
    HWND hTab=GetDlgItem(lpConnection->hParent,IDC_TAB);

    HTREEITEM hItem=(HTREEITEM)SendMessage(hTab,TVM_GETNEXTITEM,TVGN_CARET,NULL);
    if (hItem != lpConnection->hItem)
    {
        if (!IsRoot(hTab,hItem))
        {
            TVITEM tvi={0};
            tvi.mask=TVIF_STATE;
            tvi.stateMask=TVIS_BOLD;
            tvi.state=TVIS_BOLD;
            tvi.hItem=lpConnection->hItem;

            if (tvi.hItem)
                SendMessage(hTab,TVM_SETITEM,NULL,(LPARAM)&tvi);
        }
    }
    PostMessage(lpConnection->hParent,WM_COMMAND,CMD_NOTIFY_TAB,0);
    return;
}

void UpdateConnectionStatus(PCONNECTION lpConnection,DWORD dwStatus)
{
    PCOMM_TAB lpParam=(PCOMM_TAB)GetWindowLongPtr(lpConnection->hParent,GWLP_USERDATA);
    if (!lpParam)
        return;

    if (dwStatus == CS_OFFLINE)
        lpParam->dwOnline--;
    else if (lpConnection->dwPrevStatus == CS_OFFLINE)
        lpParam->dwOnline++;

    TVITEM tvi={0};
    tvi.mask=TVIF_SELECTEDIMAGE|TVIF_IMAGE;
    lpConnection->dwPrevStatus=tvi.iSelectedImage=tvi.iImage=dwStatus;
    tvi.hItem=lpConnection->hItem;

    if (tvi.hItem)
        SendDlgItemMessage(lpConnection->hParent,IDC_TAB,TVM_SETITEM,NULL,(LPARAM)&tvi);

    SetItemBold(lpConnection);

    SendMessage(lpConnection->hParent,WM_COMMAND,CMD_UPDATE_TABS,0);
    return;
}

static void UpdateStatText(HWND hStat,DWORD dwId,LPSTR lpText)
{
    LVITEMA lvi={0};
    lvi.mask=LVIF_TEXT;
    lvi.iItem=dwId;
    lvi.iSubItem=1;
    lvi.cchTextMax=MAX_PATH;
    lvi.pszText=lpText;
    SendMessageA(hStat,LVM_SETITEMA,NULL,(LPARAM)&lvi);
    return;
}

static void UpdateStat(HWND hStat,DWORD dwId,DWORD dwData,LPDWORD lpdwPrevData)
{
    if (dwData != *lpdwPrevData)
    {
        *lpdwPrevData=dwData;

        char szDataFormatted[MAX_PATH];
        StrNumberFormatA(dwData,szDataFormatted,ARRAYSIZE(szDataFormatted),",");

        UpdateStatText(hStat,dwId,szDataFormatted);
    }
    return;
}

CONNECTION_TYPE dwCurTab=TYPE_NONE;
static bool IsConnectionActive(PCONNECTION lpConnection)
{
    bool bRet=false;
    do
    {
        if (!lpConnection->bActive)
            break;

        if (lpConnection->dwType != dwCurTab)
            break;

        bRet=true;
    }
    while (false);
    return bRet;
}

static GLOBAL_STATS gs={0};
void UpdateStats(PCONNECTION lpConnection)
{
    do
    {
        DWORD dwStatus=CS_ONLINE;
        if (!lpConnection->Sts.dwActiveTask)
        {
            switch (lpConnection->dwStatus)
            {
                case STATUS_STARTED:
                {
                    if (lpConnection->bNA)
                        dwStatus=CS_N_A;
                    else
                        dwStatus=CS_IDLE;

                    break;
                }
                case STATUS_STOPPING:
                case STATUS_STOPPED:
                {
                    dwStatus=CS_PAUSED;
                    break;
                }
            }
        }

        if ((lpConnection->Socket.bConnected) && (dwStatus != lpConnection->dwPrevStatus))
            UpdateConnectionStatus(lpConnection,dwStatus);

        if (!IsConnectionActive(lpConnection))
            break;

        HWND hStat=GetDlgItem(lpConnection->hDlg,IDC_STAT);
        if (!hStat)
            break;

        SendMessageW(hStat,WM_SETREDRAW,false,NULL);

        if (lpConnection->bRoot)
        {
            PCOMM_TAB lpParam=(PCOMM_TAB)GetWindowLongPtr(lpConnection->hParent,GWLP_USERDATA);
            if (!lpParam)
                break;

            if (lpConnection->dwType == TYPE_CHECKER)
            {
                UpdateStat(hStat,0,InterlockedExchangeAdd(&dwItemsToCheck,0),&gs.dwItemsToCheck);

                if (lpParam->dwPrevOnline != lpParam->dwOnline)
                {
                    char szOnline[MAX_PATH];
                    wsprintfA(szOnline,"%d/%d",lpParam->dwOnline,lpParam->dwItems);
                    UpdateStatText(hStat,1,szOnline);

                    lpParam->dwPrevOnline=lpParam->dwOnline;
                }

                UpdateStat(hStat,2,dwCheckersTasks,&gs.dwCheckersTasks);
                UpdateStat(hStat,3,dwChecked,&gs.dwChecked);
                UpdateStat(hStat,4,dwRFB,&gs.dwRFB);
                UpdateStat(hStat,5,dwNotRFB,&gs.dwNotRFB);
                UpdateStat(hStat,6,dwUnavailble,&gs.dwUnavailble);

                DWORD dwProgress=0;
                if (InterlockedExchangeAdd(&dwItemsToCheck,0))
                    dwProgress=InterlockedExchangeAdd(&dwCheckerItemsUsed,0)*100/InterlockedExchangeAdd(&dwItemsToCheck,0);

                if ((dwProgress != gs.dwCheckerProgress) || (bChecker_UpdatePercents))
                {
                    if (dwProgress > 100)
                        dwProgress=100;

                    char szProgress[MAX_PATH];
                    wsprintfA(szProgress,"%d%%",dwProgress);

                    if (bChecker_NewAddedItemsUsed)
                        lstrcatA(szProgress," *");

                    bChecker_UpdatePercents=false;

                    UpdateStatText(hStat,7,szProgress);

                    gs.dwCheckerProgress=dwProgress;
                }
                break;
            }

            UpdateStat(hStat,0,InterlockedExchangeAdd(&dwItemsToBrute,0),&gs.dwItemsToBrute);

            if (lpParam->dwPrevOnline != lpParam->dwOnline)
            {
                char szOnline[MAX_PATH];
                wsprintfA(szOnline,"%d/%d",lpParam->dwOnline,lpParam->dwItems);
                UpdateStatText(hStat,1,szOnline);

                lpParam->dwPrevOnline=lpParam->dwOnline;
            }

            UpdateStat(hStat,2,dwWorkersTasks,&gs.dwWorkersTasks);
            UpdateStat(hStat,3,dwBrutted,&gs.dwBrutted);
            UpdateStat(hStat,4,dwUnsupported,&gs.dwUnsupported);

            DWORD dwProgress=0;
            if (InterlockedExchangeAdd(&dwItemsToBrute,0))
                dwProgress=InterlockedExchangeAdd(&dwWorkerItemsUsed,0)*100/InterlockedExchangeAdd(&dwItemsToBrute,0);

            if (dwProgress != gs.dwWorkerProgress)
            {
                if (dwProgress > 100)
                    dwProgress=100;

                char szProgress[MAX_PATH];
                wsprintfA(szProgress,"%d%%",dwProgress);
                UpdateStatText(hStat,5,szProgress);

                gs.dwWorkerProgress=dwProgress;
            }
            break;
        }

        UpdateStat(hStat,1,lpConnection->Sts.dwActiveTask,&lpConnection->PrevSts.dwActiveTask);

        if (lpConnection->dwType == TYPE_CHECKER)
        {
            UpdateStat(hStat,2,lpConnection->Sts.cs.dwChecked,&lpConnection->PrevSts.cs.dwChecked);
            UpdateStat(hStat,3,lpConnection->Sts.cs.dwRFB,&lpConnection->PrevSts.cs.dwRFB);
            UpdateStat(hStat,4,lpConnection->Sts.cs.dwNotRFB,&lpConnection->PrevSts.cs.dwNotRFB);
            UpdateStat(hStat,5,lpConnection->Sts.cs.dwFailed,&lpConnection->PrevSts.cs.dwFailed);
            break;
        }

        UpdateStat(hStat,2,lpConnection->Sts.ws.dwBrutted,&lpConnection->PrevSts.ws.dwBrutted);
        UpdateStat(hStat,3,lpConnection->Sts.ws.dwUnsupported,&lpConnection->PrevSts.ws.dwUnsupported);
    }
    while (false);

    SendMessageW(GetDlgItem(lpConnection->hDlg,IDC_STAT),WM_SETREDRAW,true,NULL);
    return;
}

static void CloseSocket(PCONNECTION lpConnection)
{
    if (lpConnection->Socket.hSock != INVALID_SOCKET)
    {
        WSAAsyncSelect(lpConnection->Socket.hSock,lpConnection->hDlg,0,0);
        closesocket(lpConnection->Socket.hSock);

        lpConnection->Socket.hSock=INVALID_SOCKET;
        lpConnection->Socket.dwReceived=0;
        lpConnection->Socket.dwNeedToRecv=0;
    }
    return;
}

void Disconnect(PCONNECTION lpConnection)
{
    do
    {
        if (!lpConnection)
            break;

        CloseSocket(lpConnection);

        if (!lpConnection->Socket.bConnected)
            break;

        lpConnection->Socket.bConnected=false;
        lpConnection->dwStatus=STATUS_STOPPED;

        if (lpConnection->dwType == TYPE_CHECKER)
        {
            InterlockedDecrement(&dwCheckersCount);
            Checker_FreeConnection(lpConnection);
        }
        else if (lpConnection->dwType == TYPE_WORKER)
        {
            InterlockedDecrement(&dwWorkersCount);
            Worker_FreeConnection(lpConnection);

            Worker_ResortList();
        }

        lpConnection->bResultReceived=false;

        UpdateConnectionStatus(lpConnection,CS_OFFLINE);
        KillTimer(lpConnection->hDlg,IDT_PING);
    }
    while (false);
    return;
}

LRESULT GridView_ProcessCustomDraw(LPARAM lParam,bool bWorker,bool bRoot)
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

            DWORD dwItem=lplvcd->nmcd.dwItemSpec;
            if (bWorker)
                dwItem++;

            DWORD dwShift=1;
            if (bRoot)
                dwShift=2;

            if (dwItem == dwShift+2)
                lplvcd->clrText=RGB(0,153,0);
            else if (dwItem == dwShift+3)
                lplvcd->clrText=RGB(153,0,0);
            else if ((!bWorker) && (dwItem == dwShift+4))
                lplvcd->clrText=RGB(128,128,128);

            break;
        }
    }
    return dwRet;
}

void GridView_AddItem(HWND hList,LPLVITEMA lpLVI,LPSTR lpAlias,LPSTR lpValue,DWORD dwRowId)
{
    DWORD dwMsg=LVM_INSERTITEMA;
    if (dwRowId)
        dwMsg=LVM_SETITEMA;

    lpLVI->iSubItem=dwRowId;
    lpLVI->pszText=lpAlias;
    SendMessageA(hList,dwMsg,NULL,(LPARAM)lpLVI);

    lpLVI->iSubItem++;
    lpLVI->pszText=lpValue;
    SendMessageA(hList,LVM_SETITEMA,NULL,(LPARAM)lpLVI);

    lpLVI->iItem++;
    return;
}

void GridView_Init(HWND hList)
{
    SendMessage(hList,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_GRIDLINES|LVS_EX_FLATSB|LVS_EX_LABELTIP|LVS_EX_DOUBLEBUFFER);
    SendMessage(hList,LVM_SETIMAGELIST,LVSIL_SMALL,(LPARAM)ImageList_Create(1,24,0,1,1));
    SendMessage(hList,LVM_SETBKCOLOR,0,GetSysColor(COLOR_BTNFACE));

    LOGFONT lf;
    GetObject((HFONT)SendMessage(hList,WM_GETFONT,NULL,NULL),sizeof(lf),&lf);

    lf.lfWeight=FW_BOLD;;
    HFONT hBoldFont=CreateFontIndirect(&lf);
    SendMessage(hList,WM_SETFONT,(WPARAM)hBoldFont,true);
    return;
}

static void WINAPI CmdParsingThread(HANDLE hEvent)
{
    MSG msg;
    PeekMessage(&msg,NULL,WM_USER,WM_USER,PM_NOREMOVE);
    SetEvent(hEvent);

    while ((int)GetMessage(&msg,NULL,0,0) > 0)
    {
        if (msg.message != TM_PARSE_PACKET)
            continue;

        PPARSE_PACKET_PARAMS lpParams=(PPARSE_PACKET_PARAMS)msg.lParam;
        if (!lpParams)
            continue;

        cJSON *jsPacket=lpParams->jsPacket;
        PCONNECTION lpConnection=lpParams->lpConnection;
        MemFree(lpParams);

        if (!jsPacket)
            continue;

        Command_ParsePacket(lpConnection,jsPacket);
        cJSON_Delete(jsPacket);
    }
    return;
}

INT_PTR CALLBACK CommonTab2DlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    INT_PTR dwRet=FALSE;

    PCONNECTION lpConnection=(PCONNECTION)GetWindowLongPtr(hDlg,GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            lpConnection=(PCONNECTION)lParam;
            SetWindowLongPtr(hDlg,GWLP_USERDATA,(LONG_PTR)lpConnection);
            lpConnection->hParent=GetParent(GetParent(hDlg));

            if (!lpConnection->bRoot)
            {
                HANDLE hEvent=CreateEvent(NULL,false,false,NULL);
                ThreadsGroup_CreateThread(hThreadsGroup,0,(LPTHREAD_START_ROUTINE)CmdParsingThread,hEvent,&lpConnection->dwCmdParsingThreadId,NULL);
                WaitForSingleObject(hEvent,INFINITE);
                CloseHandle(hEvent);
            }

            HWND hStat=GetDlgItem(hDlg,IDC_STAT);
            GridView_Init(hStat);

            LVCOLUMN lvc={0};
            lvc.mask=LVCF_WIDTH;
            lvc.cx=100;
            SendMessage(hStat,LVM_INSERTCOLUMN,0,(LPARAM)&lvc);
            SendMessage(hStat,LVM_INSERTCOLUMN,1,(LPARAM)&lvc);

            LVITEMA lvi={0};
            lvi.mask=LVIF_TEXT;
            lvi.cchTextMax=MAX_PATH;

            if (lpConnection->bRoot)
            {
                if (lpConnection->dwType == TYPE_WORKER)
                {
                    GridView_AddItem(hStat,&lvi,"Items to brute","0");
                    GridView_AddItem(hStat,&lvi,"Workers","0/0");
                }
                else
                {
                    GridView_AddItem(hStat,&lvi,"Items to check","0");
                    GridView_AddItem(hStat,&lvi,"Checkers","0/0");
                }
            }
            else
            {
                char szAddr[MAX_PATH];
                lstrcpyA(szAddr,NetNtoA(lpConnection->Socket.si.sin_addr.s_addr));

                GridView_AddItem(hStat,&lvi,"Address",szAddr);
            }

            GridView_AddItem(hStat,&lvi,"Active tasks","0");

            if (lpConnection->dwType == TYPE_WORKER)
            {
                GridView_AddItem(hStat,&lvi,"Brutted","0");
                GridView_AddItem(hStat,&lvi,"Unsupported","0");

                if (lpConnection->bRoot)
                {
                    ShowWindow(GetDlgItem(hDlg,IDC_FOUND_LOG),SW_HIDE);
                    UpdateStats(lpConnection);
                }

                CreateFoundLog(hDlg,"tab_workers","columns");
            }
            else
            {
                GridView_AddItem(hStat,&lvi,"Total checked","0");
                GridView_AddItem(hStat,&lvi,"RFB","0");
                GridView_AddItem(hStat,&lvi,"Non-RFB","0");
                GridView_AddItem(hStat,&lvi,"Unavailble","0");
            }

            if (lpConnection->bRoot)
                GridView_AddItem(hStat,&lvi,"Progress","0%");

            SetTimer(hDlg,IDT_UPDATE_STATS,MILLISECONDS_PER_SECOND,NULL);
            break;
        }
        case WM_SIZE:
        {
            RECT rc;
            GetClientRect(GetParent(hDlg),&rc);
            MoveWindow(hDlg,0,0,rc.right,rc.bottom,true);

            DWORD dwItems=SendDlgItemMessage(hDlg,IDC_STAT,LVM_GETITEMCOUNT,NULL,NULL),
                  dwBottom=dwItems*24+dwItems*2.2;

            HWND hStat=GetDlgItem(hDlg,IDC_STAT);
            MoveWindow(hStat,0,0,rc.right,dwBottom,true);

            LVCOLUMN lvc={0};
            lvc.mask=LVCF_WIDTH;
            lvc.cx=rc.right-105;
            SendMessage(hStat,LVM_SETCOLUMN,1,(LPARAM)&lvc);

            if ((lpConnection->dwType == TYPE_WORKER) && (!lpConnection->bRoot))
                MoveWindow(GetDlgItem(hDlg,IDC_FOUND_LOG),0,dwBottom+10,rc.right,rc.bottom-dwBottom-10,true);

            break;
        }
        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code)
            {
                case NM_SETFOCUS:
                {
                    if (((LPNMHDR)lParam)->idFrom == IDC_FOUND_LOG)
                        break;

                    SetFocus(GetDlgItem(lpConnection->hParent,IDC_TAB));
                    break;
                }
                case LVN_KEYDOWN:
                {
                    if (((LPNMHDR)lParam)->idFrom != IDC_FOUND_LOG)
                        break;

                    LPNMLVKEYDOWN nmlvk=(LPNMLVKEYDOWN)lParam;

                    if (nmlvk->wVKey == VK_DELETE)
                    {
                        DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                        if (dwSelected != -1)
                            SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_DELETEITEM,dwSelected,NULL);
                        else
                            SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_DELETEALLITEMS,NULL,NULL);
                        break;
                    }
                    break;
                }
                case HDN_ITEMCHANGED:
                {
                    if (lpConnection->dwType == TYPE_CHECKER)
                        break;

                    if (lpConnection->bInsideItemChange)
                        break;

                    cJSON *jsTab=cJSON_GetObjectItem(jsSettings,"tab_workers");
                    if (!jsTab)
                        break;

                    cJSON *jsColumns=cJSON_GetObjectItem(jsTab,"columns");
                    if (!jsColumns)
                        break;

                    DWORD dwWidths[5];
                    for (DWORD i=0; i < ARRAYSIZE(dwWidths); i++)
                    {
                        DWORD dwWidth=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETCOLUMNWIDTH,i,NULL);
                        if (!dwWidth)
                            continue;

                        dwWidths[i]=dwWidth;

                        cJSON *jsItem=cJSON_GetArrayItem(jsColumns,i);
                        if (!jsItem)
                            continue;

                        cJSON_SetNumberValue(jsItem,dwWidth);
                    }

                    StoreDlgPos();

                    PCONNECTION lpCurConnection=lpConnections;
                    while (lpCurConnection)
                    {
                        if ((lpCurConnection->dwType == TYPE_WORKER) && (lpCurConnection != lpConnection))
                        {
                            lpCurConnection->bInsideItemChange=true;

                            for (DWORD i=0; i < ARRAYSIZE(dwWidths); i++)
                                SendDlgItemMessage(lpCurConnection->hDlg,IDC_FOUND_LOG,LVM_SETCOLUMNWIDTH,i,dwWidths[i]);

                            lpCurConnection->bInsideItemChange=false;
                        }

                        lpCurConnection=lpCurConnection->lpNext;
                    }
                    break;
                }
                case NM_CUSTOMDRAW:
                {
                    if (((LPNMHDR)lParam)->idFrom == IDC_FOUND_LOG)
                        break;

                    SetWindowLongPtr(hDlg,DWLP_MSGRESULT,(LONG_PTR)GridView_ProcessCustomDraw(lParam,(lpConnection->dwType == TYPE_WORKER),lpConnection->bRoot));
                    dwRet=true;
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
                case IDT_PING:
                {
                    if (!lpConnection->Socket.bConnected)
                        break;

                    if (Now()-lpConnection->dwLastActivityTime < 60)
                        break;

                    cJSON *jsCmdPing=cJSON_CreateObject();
                    if (jsCmdPing)
                    {
                        cJSON_AddStringToObject(jsCmdPing,"type","cmd");
                        cJSON_AddStringToObject(jsCmdPing,"cmd","ping");

                        JSON_SendPacketAndFree(&lpConnection->Socket,jsCmdPing);
                    }
                    break;
                }
                case IDT_UPDATE_STATS:
                {
                    UpdateStats(lpConnection);
                    break;
                }
            }
            break;
        }
        case WM_SOCKET:
        {
            if (WSAGETSELECTERROR(lParam))
            {
                if (lpConnection->dwType == TYPE_CHECKER)
                    InterlockedExchangeSubtract(&dwCheckersTasks,lpConnection->Sts.dwActiveTask);
                else
                    InterlockedExchangeSubtract(&dwWorkersTasks,lpConnection->Sts.dwActiveTask);

                lpConnection->Sts.dwActiveTask=0;
                UpdateStats(lpConnection);

                Disconnect(lpConnection);
                break;
            }

            lpConnection->dwLastActivityTime=Now();

            switch (WSAGETSELECTEVENT(lParam))
            {
                case FD_READ:
                {
                    if (!lpConnection->Socket.dwNeedToRecv)
                    {
                        int iRecv=recv(wParam,(PCHAR)&lpConnection->Socket.dwNeedToRecv,sizeof(lpConnection->Socket.dwNeedToRecv),0);
                        if ((!iRecv) || (iRecv == SOCKET_ERROR))
                            break;
                    }

                    if (lpConnection->Socket.dwRecvBufSize < lpConnection->Socket.dwNeedToRecv)
                    {
                        lpConnection->Socket.lpRecvBuf=(PCHAR)MemRealloc(lpConnection->Socket.lpRecvBuf,lpConnection->Socket.dwNeedToRecv);
                        if (lpConnection->Socket.lpRecvBuf)
                            lpConnection->Socket.dwRecvBufSize=lpConnection->Socket.dwNeedToRecv;
                        else
                            DebugLog_AddItem2(lpConnection,LOG_LEVEL_ERROR,"MemRealloc(%d) failed",lpConnection->Socket.dwNeedToRecv);
                    }

                    PCHAR lpPtr=lpConnection->Socket.lpRecvBuf+lpConnection->Socket.dwReceived;
                    DWORD dwNeedToRecv=lpConnection->Socket.dwNeedToRecv-lpConnection->Socket.dwReceived;
                    int iRecv=recv(wParam,lpPtr,dwNeedToRecv,0);
                    if ((!iRecv) || (iRecv == SOCKET_ERROR))
                        break;

                    lpConnection->Socket.dwReceived+=iRecv;
                    if (lpConnection->Socket.dwReceived < lpConnection->Socket.dwNeedToRecv)
                        break;

                    PPARSE_PACKET_PARAMS lpParams=(PPARSE_PACKET_PARAMS)MemQuickAlloc(sizeof(*lpParams));
                    if (!lpParams)
                        break;

                    lpParams->jsPacket=cJSON_ParseWithLength(lpConnection->Socket.lpRecvBuf,lpConnection->Socket.dwNeedToRecv);
                    lpParams->lpConnection=lpConnection;

                    PostThreadMessageEx(lpConnection->dwCmdParsingThreadId,TM_PARSE_PACKET,NULL,(LPARAM)lpParams);

                    lpConnection->Socket.dwReceived=0;
                    lpConnection->Socket.dwNeedToRecv=0;
                    break;
                }
                case FD_CLOSE:
                {
                    Disconnect(lpConnection);
                    break;
                }
            }
            break;
        }
    }
    return dwRet;
}

static bool ConnectionCanBeStarted(PCONNECTION lpConnection)
{
    bool bRet=false;
    do
    {
        if (lpConnection->dwType == TYPE_CHECKER)
        {
            if (Checker_IsPurging())
                break;
        }

        if (!lpConnection->Socket.bConnected)
            break;

        if (lpConnection->dwStatus != STATUS_STOPPED)
            break;

        bRet=true;
    }
    while (false);
    return bRet;
}

static bool ConnectionCanBeStopped(PCONNECTION lpConnection)
{
    bool bRet=false;
    do
    {
        if (lpConnection->dwType == TYPE_CHECKER)
        {
            if (Checker_IsPurging())
                break;
        }

        if (!lpConnection->Socket.bConnected)
            break;

        if (lpConnection->dwStatus != STATUS_STARTED)
            break;

        bRet=true;
    }
    while (false);
    return bRet;
}

HMENU hContextMenuRoot,hContextMenuItem;
static void ShowContextMenu(HWND hDlg,cJSON *jsConfig)
{
    do
    {
        POINT pt;
        GetCursorPos(&pt);

        HWND hTab=GetDlgItem(hDlg,IDC_TAB);
        if (!hTab)
            break;

        TVHITTESTINFO tvhti={0};
        tvhti.pt=pt;
        ScreenToClient(hTab,&tvhti.pt);

        SendMessage(hTab,TVM_HITTEST,NULL,(LPARAM)&tvhti);
        if (!(tvhti.flags & TVHT_ONITEM|TVHT_ONITEMICON|TVHT_ONITEMSTATEICON|TVHT_ONITEMLABEL))
            break;

        TVITEM tvi={0};
        tvi.mask=TVIF_PARAM;
        tvi.hItem=tvhti.hItem;
        if (!SendMessage(hTab,TVM_GETITEM,NULL,(LPARAM)&tvi))
            break;

        SendMessage(hTab,TVM_SELECTITEM,TVGN_CARET,(LPARAM)tvhti.hItem);

        PCONNECTION lpConnection=(PCONNECTION)tvi.lParam;
        if (!lpConnection)
            break;

        HMENU hContextMenu;

        DWORD dwStartState=MF_BYCOMMAND,
              dwStopState=MF_BYCOMMAND;
        if (!lpConnection->bRoot)
        {
            hContextMenu=hContextMenuItem;

            if (!ConnectionCanBeStarted(lpConnection))
                dwStartState|=MF_DISABLED|MF_GRAYED;

            if (!ConnectionCanBeStopped(lpConnection))
                dwStopState|=MF_DISABLED|MF_GRAYED;
        }
        else
        {
            hContextMenu=hContextMenuRoot;

            bool bHaveStarted=false,bHaveStopped=false;

            tvi.hItem=(HTREEITEM)SendMessage(hTab,TVM_GETNEXTITEM,TVGN_CHILD,(LPARAM)tvi.hItem);
            while (tvi.hItem)
            {
                SendMessage(hTab,TVM_GETITEM,NULL,(LPARAM)&tvi);

                PCONNECTION lpConnection=(PCONNECTION)tvi.lParam;
                if (lpConnection)
                {
                    if (ConnectionCanBeStarted(lpConnection))
                        bHaveStopped=true;

                    if (ConnectionCanBeStopped(lpConnection))
                        bHaveStarted=true;
                }

                tvi.hItem=(HTREEITEM)SendMessage(hTab,TVM_GETNEXTITEM,TVGN_NEXT,(LPARAM)tvi.hItem);
            }

            if (!bHaveStopped)
                dwStartState|=MF_DISABLED|MF_GRAYED;

            if (!bHaveStarted)
                dwStopState|=MF_DISABLED|MF_GRAYED;

            DWORD dwState=MF_BYCOMMAND;
            if (IsShowAll(jsConfig))
                dwState|=MF_CHECKED;

            CheckMenuItem(hContextMenu,IDM_SHOW_ALL,dwState);
        }

        EnableMenuItem(hContextMenu,IDM_START,dwStartState);
        EnableMenuItem(hContextMenu,IDM_STOP,dwStopState);

        TrackPopupMenu(hContextMenu,TPM_LEFTALIGN,pt.x,pt.y,NULL,hDlg,NULL);
    }
    while (false);
    return;
}

static PCONNECTION GetSelectedItem(HWND hDlg)
{
    PCONNECTION lpConnection=NULL;
    do
    {
        HTREEITEM hItem=(HTREEITEM)SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETNEXTITEM,TVGN_CARET,(LPARAM)TVI_ROOT);
        if (!hItem)
            break;

        TVITEM tvi={0};
        tvi.mask=TVIF_PARAM;
        tvi.hItem=hItem;
        if (!SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETITEM,NULL,(LPARAM)&tvi))
            break;

        lpConnection=(PCONNECTION)tvi.lParam;
    }
    while (false);
    return lpConnection;
}

static HBITMAP hBmp;
static  HBRUSH  hBrush;
static int iOldX=-4;
static bool bSplitting=false;
static void Splitter_DrawSplitBar(HDC hDC,int iX,int iHeight)
{
	SetBrushOrgEx(hDC,iX,4,0);
	HBRUSH hOldBrush=(HBRUSH)SelectObject(hDC,hBrush);
	PatBlt(hDC,iX,0,4,iHeight,PATINVERT);
	SelectObject(hDC,hOldBrush);
	return;
}

static void Splitter_GetRectAndPoint(HWND hDlg,LPARAM lParam,LPRECT lpRect,LPPOINT lpPt)
{
    POINT pt;
    pt.x=(WORD)LOWORD(lParam);
    pt.y=(WORD)HIWORD(lParam);

    RECT rc;
    GetWindowRect(hDlg,&rc);

    ClientToScreen(hDlg,&pt);
    pt.x-=rc.left;
    pt.y-=rc.top;
    OffsetRect(&rc,-rc.left,-rc.top);

    if (pt.x < 115)
        pt.x=115;
    else if (pt.x > (rc.right-rc.left)/2)
        pt.x=(rc.right-rc.left)/2;

    *lpPt=pt;
    *lpRect=rc;
    return;
}

static void Splitter_StorePos(HWND hDlg,PCOMM_TAB lpParam,DWORD dwX)
{
    cJSON *jsSplitSize=cJSON_GetObjectItem(lpParam->jsConfig,"split_size");
    if (jsSplitSize)
        cJSON_SetNumberValue(jsSplitSize,dwX);

    PostMessage(hDlg,WM_SIZE,NULL,NULL);
    StoreDlgPos();
    return;
}

static void UpdateAlias(PCONNECTION lpConnection,LPCSTR lpAlias)
{
    if ((lpAlias) && (lpAlias[0]))
        lstrcpyA(lpConnection->szAlias,lpAlias);
    else
        lpConnection->szAlias[0]=0;

    if (lpConnection->jsItem)
    {
        if (lpConnection->szAlias[0])
        {
            cJSON *jsAlias=cJSON_GetObjectItem(lpConnection->jsItem,"alias");
            if (!jsAlias)
                cJSON_AddItemToObject(lpConnection->jsItem,"alias",cJSON_CreateString(lpAlias));
            else
                cJSON_SetValuestring(jsAlias,lpAlias);
        }
        else
            cJSON_DetachItemFromObject(lpConnection->jsItem,"alias");

        Config_Update();
    }
    return;
}

static void OnConnect(PCONNECTION lpConnection)
{
    if (lpConnection)
    {
        WSAAsyncSelect(lpConnection->Socket.hSock,lpConnection->hDlg,WM_SOCKET,FD_READ|FD_CLOSE);

        lpConnection->Socket.bConnected=true;

        lpConnection->Socket.dwReceived=0;
        lpConnection->Socket.dwNeedToRecv=0;

        if (lpConnection->dwType == TYPE_CHECKER)
            InterlockedIncrement(&dwCheckersCount);
        else if (lpConnection->dwType == TYPE_WORKER)
            InterlockedIncrement(&dwWorkersCount);

        UpdateConnectionStatus(lpConnection,CS_IDLE);
        SetTimer(lpConnection->hDlg,IDT_PING,10*MILLISECONDS_PER_SECOND,NULL);
    }
    return;
}

static HCURSOR hSizeCursor;
INT_PTR CALLBACK CommonTabDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    PCOMM_TAB lpParam=(PCOMM_TAB)GetWindowLongPtr(hDlg,GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            lpParam=(PCOMM_TAB)lParam;
            SetWindowLongPtr(hDlg,GWLP_USERDATA,(LONG_PTR)lpParam);

            HIMAGELIST hImgList=ImageList_LoadImage(hInstance,(LPCTSTR)101,16,NULL,0xFF00FF,IMAGE_BITMAP,LR_CREATEDIBSECTION);
            SendDlgItemMessage(hDlg,IDC_TAB,TVM_SETIMAGELIST,LVSIL_NORMAL,(LPARAM)hImgList);
            SendDlgItemMessage(hDlg,IDC_TAB,TVM_SETIMAGELIST,LVSIL_STATE,(LPARAM)hImgList);
            SendDlgItemMessage(hDlg,IDC_TAB,TVM_SETEXTENDEDSTYLE,0,TVS_EX_DOUBLEBUFFER);

            LPCTSTR lpID;
            if (lpParam->dwConnectionType == TYPE_WORKER)
                lpID=MAKEINTRESOURCE(IDD_WORKER);
            else
                lpID=MAKEINTRESOURCE(IDD_CHECKER);

            HWND hTabs=GetDlgItem(hDlg,IDC_TABS);

            PCONNECTION lpRootConnection=lpConnections;
            while (lpRootConnection)
            {
                if ((lpRootConnection->dwType == lpParam->dwConnectionType) && (lpRootConnection->bRoot))
                    break;

                lpRootConnection=lpRootConnection->lpNext;
            }

            if (!lpRootConnection)
                break; /// ???

            LPCSTR lpTab="tab_workers";
            lpParam->lpServerInfo=&siWorker;

            if (lpRootConnection->dwType == TYPE_CHECKER)
            {
                lpTab="tab_checkers";
                lpParam->lpServerInfo=&siChecker;
            }

            cJSON *jsTab=cJSON_GetObjectItem(jsSettings,lpTab);
            if (!jsTab)
            {
                jsTab=cJSON_CreateObject();
                cJSON_AddItemToObject(jsSettings,lpTab,jsTab);
            }

            lpParam->jsConfig=jsTab;
            lpParam->dwPrevOnline=-1;

            DWORD dwSelected=cJSON_GetIntFromObject(jsTab,"item");

            TVINSERTSTRUCTW tvi={0};
            tvi.hInsertAfter=TVI_LAST;
            tvi.item.mask=TVIF_TEXT|TVIF_SELECTEDIMAGE|TVIF_IMAGE|TVIF_PARAM;
            tvi.item.iSelectedImage=tvi.item.iImage=CS_ROOT;
            tvi.item.pszText=L"All items";
            tvi.item.cchTextMax=MAX_PATH;
            tvi.item.lParam=(LPARAM)lpRootConnection;
            lpRootConnection->dwIdx=0;
            lpRootConnection->hDlg=CreateDialogParamW(hInstance,lpID,hTabs,CommonTab2DlgProc,(LPARAM)lpRootConnection);
            SendMessage(lpRootConnection->hDlg,WM_SIZE,NULL,NULL);

            HTREEITEM hSelected=lpRootConnection->hItem=(HTREEITEM)SendDlgItemMessageW(hDlg,IDC_TAB,TVM_INSERTITEMW,NULL,(LPARAM)&tvi);

            bool bShowAll=IsShowAll(jsTab);

            DWORD dwIdx=1;
            PCONNECTION lpConnection=lpConnections;
            while (lpConnection)
            {
                if ((lpConnection->dwType == lpParam->dwConnectionType) && (!lpConnection->bRoot))
                {
                    char szAddr[MAX_PATH];

                    TVINSERTSTRUCTA tvi={0};
                    tvi.hInsertAfter=TVI_LAST;
                    tvi.hParent=lpRootConnection->hItem;
                    tvi.item.mask=TVIF_TEXT|TVIF_SELECTEDIMAGE|TVIF_IMAGE|TVIF_PARAM;
                    tvi.item.lParam=(LPARAM)lpConnection;
                    lpConnection->dwPrevStatus=tvi.item.iSelectedImage=tvi.item.iImage=CS_OFFLINE;
                    lpConnection->dwIdx=dwIdx++;
                    lpConnection->hDlg=CreateDialogParamW(hInstance,lpID,hTabs,CommonTab2DlgProc,(LPARAM)lpConnection);
                    SendMessage(lpConnection->hDlg,WM_SIZE,NULL,NULL);

                    tvi.item.pszText=szAddr;
                    tvi.item.cchTextMax=MAX_PATH;

                    if (lpConnection->szAlias[0])
                        lstrcpyA(szAddr,lpConnection->szAlias);
                    else
                        lstrcpyA(szAddr,NetNtoA(lpConnection->Socket.si.sin_addr.s_addr));

                    if (bShowAll)
                    {
                        lpConnection->hItem=(HTREEITEM)SendDlgItemMessageA(hDlg,IDC_TAB,TVM_INSERTITEMA,NULL,(LPARAM)&tvi);

                        if (lpConnection->dwIdx == dwSelected)
                            hSelected=lpConnection->hItem;
                    }
                    lpParam->dwItems++;
                }
                lpConnection=lpConnection->lpNext;
            }

            SendDlgItemMessage(hDlg,IDC_TAB,TVM_EXPAND,TVE_EXPAND,(LPARAM)lpRootConnection->hItem);
            SendDlgItemMessage(hDlg,IDC_TAB,TVM_SELECTITEM,TVGN_CARET,(LPARAM)hSelected);

            hSizeCursor=LoadCursor(NULL,IDC_SIZEWE);

            WORD wDotPattern[]=
            {
                0x00AA,0x0055,0x00AA,0x0055,
                0x00AA,0x0055,0x00AA,0x0055,
            };

            hBmp=CreateBitmap(8,8,1,1,wDotPattern);
            hBrush=CreatePatternBrush(hBmp);

            WSAAsyncSelect(lpParam->lpServerInfo->hSock,hDlg,WM_SOCKET,FD_ACCEPT);
            listen(lpParam->lpServerInfo->hSock,SOMAXCONN);
            break;
        }
        case WM_SOCKET:
        {
            if (bStopping)
                break;

            if (WSAGETSELECTERROR(lParam))
                break;

            if (WSAGETSELECTEVENT(lParam) != FD_ACCEPT)
                break;

            SOCKET hSock=accept(wParam,NULL,NULL);
            if (hSock == INVALID_SOCKET)
                break;

            sockaddr_in saddr={0};
            int dwLen=sizeof(saddr);
            getpeername(hSock,(sockaddr*)&saddr,&dwLen);

            PCONNECTION lpRootConnection=lpConnections;
            while (lpRootConnection)
            {
                if ((lpRootConnection->dwType == lpParam->dwConnectionType) && (lpRootConnection->bRoot))
                    break;

                lpRootConnection=lpRootConnection->lpNext;
            }

            bool bFound=false;
            DWORD dwIdx=0;
            PCONNECTION lpConnection=lpConnections;
            while (lpConnection)
            {
                if ((lpConnection->dwType == lpParam->dwConnectionType) && (!lpConnection->bRoot))
                {
                    if (lpConnection->Socket.si.sin_addr.s_addr == saddr.sin_addr.s_addr)
                    {
                        bFound=true;

                        lpConnection->Socket.hSock=hSock;
                        OnConnect(lpConnection);
                        break;
                    }
                    dwIdx=lpConnection->dwIdx;
                }
                lpConnection=lpConnection->lpNext;
            }

            if (bFound)
                break;

            cJSON *jsItem=cJSON_CreateObject();
            if (jsItem)
                cJSON_AddStringToObject(jsItem,"address",NetNtoA(saddr.sin_addr.s_addr));

            lpConnection=Connection_Add(jsItem,NetNtoA(saddr.sin_addr.s_addr),NULL,lpParam->dwConnectionType);
            if (!lpConnection)
                break;

            LPCTSTR lpID;
            if (lpParam->dwConnectionType == TYPE_WORKER)
                lpID=MAKEINTRESOURCE(IDD_WORKER);
            else
                lpID=MAKEINTRESOURCE(IDD_CHECKER);

            lpConnection->dwPrevStatus=CS_OFFLINE;
            lpConnection->dwIdx=dwIdx+1;
            lpConnection->Socket.hSock=hSock;
            lpConnection->hDlg=CreateDialogParamW(hInstance,lpID,GetDlgItem(hDlg,IDC_TABS),CommonTab2DlgProc,(LPARAM)lpConnection);
            SendMessage(lpConnection->hDlg,WM_SIZE,NULL,NULL);

            OnConnect(lpConnection);

            lpParam->dwItems++;

            cJSON *jsItems=cJSON_GetObjectItem(lpParam->lpServerInfo->jsServer,"items");
            if (!jsItems)
            {
                jsItems=cJSON_CreateArray();
                cJSON_AddItemToObject(lpParam->lpServerInfo->jsServer,"items",jsItems);
            }

            cJSON_AddItemToArray(jsItems,jsItem);
            Config_Update();
            break;
        }
        case WM_LBUTTONDBLCLK:
        {
            HWND hTab=GetDlgItem(hDlg,IDC_TAB);
            if (!hTab)
                break;

            RECT rcArea={0};
            HTREEITEM hNode=TreeView_GetRoot(hTab);
            do
            {
                RECT rc;
                TreeView_GetItemRect(hTab,hNode,&rc,true);

                if (rc.left < rcArea.left)
                    rcArea.left=rc.left;

                if (rc.right > rcArea.right)
                    rcArea.right=rc.right;

                if (rc.top < rcArea.top)
                    rcArea.top=rc.top;

                if (rc.bottom > rcArea.bottom)
                    rcArea.bottom=rc.bottom;
            }
            while(hNode=TreeView_GetNextVisible(hTab,hNode));

            DWORD dwWidth=rcArea.right-rcArea.left+2;

            RECT rcClient;
            GetClientRect(hDlg,&rcClient);

            if (dwWidth < 115)
                dwWidth=115;
            else if (dwWidth > rcClient.right/2)
                dwWidth=rcClient.right/2;

            Splitter_StorePos(hDlg,lpParam,dwWidth);
            break;
        }
        case WM_LBUTTONDOWN:
        {
            RECT rc;
            POINT pt;
            Splitter_GetRectAndPoint(hDlg,lParam,&rc,&pt);

            bSplitting=true;

            SetCapture(hDlg);

            HDC hDC=GetWindowDC(hDlg);
            Splitter_DrawSplitBar(hDC,pt.x,rc.bottom);
            ReleaseDC(hDlg,hDC);

            iOldX=pt.x;
            break;
        }
        case WM_MOUSEMOVE:
        {
            SetCursor(hSizeCursor);

            if (!bSplitting)
                break;

            if (!(wParam & MK_LBUTTON))
                break;

            RECT rc;
            POINT pt;
            Splitter_GetRectAndPoint(hDlg,lParam,&rc,&pt);

            if (pt.x == iOldX)
                break;

            HDC hDC=GetWindowDC(hDlg);
            Splitter_DrawSplitBar(hDC,iOldX,rc.bottom);
            Splitter_DrawSplitBar(hDC,pt.x,rc.bottom);
            ReleaseDC(hDlg,hDC);

            iOldX=pt.x;
            break;
        }
        case WM_LBUTTONUP:
        {
            if (!bSplitting)
                break;

            RECT rc;
            POINT pt;
            Splitter_GetRectAndPoint(hDlg,lParam,&rc,&pt);

            HDC hDC=GetWindowDC(hDlg);
            Splitter_DrawSplitBar(hDC,iOldX,rc.bottom);
            ReleaseDC(hDlg,hDC);

            iOldX=pt.x;
            ReleaseCapture();

            bSplitting=false;

            RECT rcClient;
            GetWindowRect(hDlg,&rcClient);
            pt.x+=rcClient.left;
            pt.y+=rcClient.top;

            ScreenToClient(hDlg,&pt);

            Splitter_StorePos(hDlg,lpParam,pt.x);
            break;
        }
        case WM_SIZE:
        {
            RECT rc;
            GetClientRect(GetParent(hDlg),&rc);
            MoveWindow(hDlg,0,0,rc.right,rc.bottom,true);

            cJSON *jsSplitSize=cJSON_GetObjectItem(lpParam->jsConfig,"split_size");
            if (!jsSplitSize)
            {
                jsSplitSize=cJSON_CreateNumber(0);
                cJSON_AddItemToObject(lpParam->jsConfig,"split_size",jsSplitSize);
            }

            DWORD dwSplitSize=(DWORD)cJSON_GetNumberValue(jsSplitSize);
            if (dwSplitSize < 115)
            {
                dwSplitSize=115;
                cJSON_SetNumberValue(jsSplitSize,dwSplitSize);
            }
            else if (dwSplitSize > rc.right/2)
            {
                dwSplitSize=rc.right/2;
                cJSON_SetNumberValue(jsSplitSize,dwSplitSize);
            }

            MoveWindow(GetDlgItem(hDlg,IDC_TAB),0,0,dwSplitSize,rc.bottom,true);
            dwSplitSize+=4;
            MoveWindow(GetDlgItem(hDlg,IDC_TABS),dwSplitSize,0,rc.right-dwSplitSize,rc.bottom,true);

            PCONNECTION lpConnection=lpConnections;
            while (lpConnection)
            {
                if (lpConnection->dwType == lpParam->dwConnectionType)
                    PostMessage(lpConnection->hDlg,WM_SIZE,NULL,NULL);

                lpConnection=lpConnection->lpNext;
            }
            break;
        }
        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code)
            {
                case TVN_BEGINLABELEDITW:
                {
                    LPNMTVDISPINFOW nmh=(LPNMTVDISPINFOW)lParam;
                    if ((!nmh->item.lParam) || (((PCONNECTION)nmh->item.lParam)->bRoot))
                    {
                        SetWindowLongPtr(hDlg,DWLP_MSGRESULT,(LONG)TRUE);
                        return true;
                    }
                    break;
                }
                case TVN_ENDLABELEDITW:
                {
                    LPNMTVDISPINFOW nmh=(LPNMTVDISPINFOW)lParam;

                    if (!nmh->item.pszText)
                        break;

                    TVITEMA tvi={0};
                    tvi.mask=TVIF_PARAM;
                    tvi.hItem=nmh->item.hItem;
                    if (!SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETITEM,NULL,(LPARAM)&tvi))
                        break;

                    PCONNECTION lpConnection=(PCONNECTION)tvi.lParam;
                    if (!lpConnection)
                        break;

                    if (!nmh->item.pszText[0])
                    {
                        char szAddr[MAX_PATH];
                        lstrcpyA(szAddr,NetNtoA(lpConnection->Socket.si.sin_addr.s_addr));

                        tvi.mask=TVIF_TEXT;
                        tvi.pszText=szAddr;
                        tvi.cchTextMax=MAX_PATH;
                        SendDlgItemMessageA(hDlg,IDC_TAB,TVM_SETITEMA,NULL,(LPARAM)&tvi);

                        UpdateAlias(lpConnection,NULL);
                        break;
                    }

                    char szAlias[MAX_PATH];
                    StrUnicodeToAnsi(nmh->item.pszText,0,szAlias,0);
                    UpdateAlias(lpConnection,szAlias);

                    SetWindowLongPtr(hDlg,DWLP_MSGRESULT,(LONG)TRUE);
                    return true;
                }
                case NM_RCLICK:
                {
                    ShowContextMenu(hDlg,lpParam->jsConfig);
                    break;
                }
                case TVN_ITEMEXPANDING:
                {
                    LPNMTREEVIEWW nmh=(LPNMTREEVIEWW)lParam;
                    if ((nmh->action != TVE_EXPAND) && (nmh->action != TVE_EXPANDPARTIAL))
                    {
                        SetWindowLongPtr(hDlg,DWLP_MSGRESULT,(LONG)TRUE);
                        return true;
                    }
                    break;
                }
                case TVN_SELCHANGED:
                {
                    LPNMTREEVIEWW nmh=(LPNMTREEVIEWW)lParam;

                    TVITEM tvi={0};
                    tvi.mask=TVIF_PARAM;
                    tvi.hItem=nmh->itemOld.hItem;
                    if (SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETITEM,NULL,(LPARAM)&tvi))
                    {
                        PCONNECTION lpConnection=(PCONNECTION)tvi.lParam;
                        if (lpConnection)
                        {
                            lpConnection->bActive=false;
                            ShowWindow(lpConnection->hDlg,SW_HIDE);
                        }
                    }

                    tvi.hItem=nmh->itemNew.hItem;
                    if (!SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETITEM,NULL,(LPARAM)&tvi))
                        break;

                    PCONNECTION lpConnection=(PCONNECTION)tvi.lParam;
                    if (!lpConnection)
                        break;

                    Setting_ChangeValue(lpParam->jsConfig,"item",lpConnection->dwIdx);

                    tvi.mask=TVIF_STATE;
                    tvi.state=0;
                    tvi.stateMask=TVIS_BOLD;
                    SendDlgItemMessage(hDlg,IDC_TAB,TVM_SETITEM,NULL,(LPARAM)&tvi);

                    lpConnection->bActive=true;
                    UpdateStats(lpConnection);
                    ShowWindow(lpConnection->hDlg,SW_SHOW);

                    if (!lpConnection->bRoot)
                        break;

                    HTREEITEM hItem=(HTREEITEM)SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETNEXTITEM,TVGN_CHILD,(LPARAM)tvi.hItem);
                    while (hItem)
                    {
                        TVITEM tvi={0};
                        tvi.mask=TVIF_STATE;
                        tvi.stateMask=TVIS_BOLD;
                        tvi.hItem=hItem;
                        SendDlgItemMessage(hDlg,IDC_TAB,TVM_SETITEM,NULL,(LPARAM)&tvi);

                        hItem=(HTREEITEM)SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETNEXTITEM,TVGN_NEXT,(LPARAM)hItem);
                    }
                    break;
                }
            }
            break;
        }
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDM_SHOW_ALL:
                {
                    cJSON *jsShowAll=cJSON_GetObjectItem(lpParam->jsConfig,"show_all");

                    bool bShowAll=!cJSON_IsTrue(jsShowAll);

                    DWORD dwState=MF_BYCOMMAND;
                    if (bShowAll)
                        dwState|=MF_CHECKED;

                    CheckMenuItem(hContextMenuRoot,IDM_SHOW_ALL,dwState);

                    cJSON_SetBoolValue(jsShowAll,bShowAll);
                    Config_Update();

                    UpdateTreeView(hDlg,lpParam);
                    break;
                }
                case IDM_START:
                {
                    PCONNECTION lpConnection=GetSelectedItem(hDlg);
                    if (!lpConnection)
                        break;

                    if (!lpConnection->bRoot)
                    {
                        if (!ConnectionCanBeStarted(lpConnection))
                            break;

                        Command_ResumeCmd(lpConnection);
                        break;
                    }

                    TVITEM tvi={0};
                    tvi.mask=TVIF_PARAM;
                    tvi.hItem=(HTREEITEM)SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETNEXTITEM,TVGN_CHILD,(LPARAM)lpConnection->hItem);
                    while (tvi.hItem)
                    {
                        SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETITEM,NULL,(LPARAM)&tvi);

                        PCONNECTION lpConnection=(PCONNECTION)tvi.lParam;
                        if (lpConnection)
                        {
                            if (ConnectionCanBeStarted(lpConnection))
                                Command_ResumeCmd(lpConnection);
                        }

                        tvi.hItem=(HTREEITEM)SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETNEXTITEM,TVGN_NEXT,(LPARAM)tvi.hItem);
                    }
                    break;
                }
                case IDM_STOP:
                {
                    PCONNECTION lpConnection=GetSelectedItem(hDlg);
                    if (!lpConnection)
                        break;

                    if (!lpConnection->bRoot)
                    {
                        if (!ConnectionCanBeStopped(lpConnection))
                            break;

                        Command_StopCmd(lpConnection);
                        break;
                    }

                    TVITEM tvi={0};
                    tvi.mask=TVIF_PARAM;
                    tvi.hItem=(HTREEITEM)SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETNEXTITEM,TVGN_CHILD,(LPARAM)lpConnection->hItem);
                    while (tvi.hItem)
                    {
                        SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETITEM,NULL,(LPARAM)&tvi);

                        PCONNECTION lpConnection=(PCONNECTION)tvi.lParam;
                        if (lpConnection)
                        {
                            if (ConnectionCanBeStopped(lpConnection))
                                Command_StopCmd(lpConnection);
                        }

                        tvi.hItem=(HTREEITEM)SendDlgItemMessage(hDlg,IDC_TAB,TVM_GETNEXTITEM,TVGN_NEXT,(LPARAM)tvi.hItem);
                    }
                    break;
                }
                case CMD_EXIT:
                {
                    Command_StopAllCmd(hDlg,lpParam->dwConnectionType);
                    break;
                }
                case CMD_NOTIFY_TAB:
                {
                    SendMessage(hMainDlg,WM_COMMAND,CMD_NOTIFY_TAB,lpParam->dwTabIdx);
                    break;
                }
                case CMD_IS_SPLITTING:
                {
                    *((PBOOL)lParam)=bSplitting;
                    break;
                }
                case CMD_CANCEL_SPLIT:
                {
                    if (!bSplitting)
                        break;

                    RECT rc;
                    GetWindowRect(hDlg,&rc);
                    OffsetRect(&rc,-rc.left,-rc.top);

                    HDC hDC=GetWindowDC(hDlg);
                    Splitter_DrawSplitBar(hDC,iOldX,rc.bottom);
                    ReleaseDC(hDlg,hDC);

                    ReleaseCapture();
                    bSplitting=false;
                    break;
                }
                case CMD_UPDATE_TABS:
                {
                    UpdateTreeView(hDlg,lpParam);
                    break;
                }
            }
            break;
        }
    }
    return FALSE;
}

