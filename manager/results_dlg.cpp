#include "includes.h"
#include <stdio.h>

#include "common\settings.h"
#include "results_dlg.h"
#include "iplist.h"

void Log_Clean(HWND hDlg)
{
    SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_DELETEALLITEMS,NULL,NULL);
    SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_RESULT_CLEAR,false);
    return;
}

static HWND hResultsDlg;
void Results_Append(LPCSTR lpTime,LPCSTR lpAddress,LPCSTR lpDesktop,DWORD dwIdx,LPCSTR lpPassword)
{
    LVITEMA lvi={0};
    lvi.mask=LVIF_TEXT;

    char szItem[513];
    lvi.pszText=szItem;
    lvi.cchTextMax=lstrlenA(lstrcpyA(szItem,lpTime));
    SendDlgItemMessageA(hResultsDlg,IDC_FOUND_LOG,LVM_INSERTITEMA,NULL,(LPARAM)&lvi);

    lvi.iSubItem++;
    lvi.cchTextMax=lstrlenA(lstrcpyA(szItem,lpAddress));
    SendDlgItemMessageA(hResultsDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

    lvi.iSubItem++;
    LPCSTR lpDesk="[empty]";
    if ((lpDesktop) && (lpDesktop[0]))
        lpDesk=lpDesktop;

    lvi.cchTextMax=lstrlenA(lstrcpyA(szItem,lpDesk));
    SendDlgItemMessageA(hResultsDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

    lvi.iSubItem++;
    lvi.cchTextMax=wsprintfA(szItem,"%d",dwIdx);
    SendDlgItemMessageA(hResultsDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

    lvi.iSubItem++;
    lvi.cchTextMax=lstrlenA(lstrcpyA(szItem,lpPassword));
    SendDlgItemMessageA(hResultsDlg,IDC_FOUND_LOG,LVM_SETITEMA,NULL,(LPARAM)&lvi);

    SendDlgItemMessage(hResultsDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_RESULT_CLEAR,true);
    PostMessage(hResultsDlg,WM_COMMAND,CMD_NOTIFY_TAB,0);
    return;
}

static LRESULT CALLBACK NewEditProc(HWND hEdit,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    WNDPROC lpOldWndProc=(WNDPROC)GetWindowLongPtr(hEdit,GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_GETDLGCODE:
        {
            if ((wParam == VK_RETURN) || (wParam == VK_ESCAPE))
                return CallWindowProc(lpOldWndProc,hEdit,uMsg,wParam,lParam)|DLGC_WANTALLKEYS;
        }
        case WM_KEYDOWN:
        {
            if (wParam == VK_RETURN)
            {
                PostMessage(GetParent(hEdit),WM_COMMAND,IDC_ADD,NULL);
                return 0;
            }
            else if (wParam == VK_ESCAPE)
            {
                PostMessage(GetParent(hEdit),WM_CLOSE,NULL,NULL);
                return 0;
            }
            break;
        }
    }
    return CallWindowProc(lpOldWndProc,hEdit,uMsg,wParam,lParam);
}

static HICON hRescanIcon;
static INT_PTR CALLBACK RescanDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            SendMessage(hDlg,WM_SETICON,ICON_BIG,(LPARAM)hRescanIcon);

            HWND hEdit=GetDlgItem(hDlg,IDC_EDIT_BOX);
            SetWindowLongPtr(hEdit,GWLP_USERDATA,(LONG_PTR)SetWindowLongPtr(hEdit,GWLP_WNDPROC,(LONG_PTR)NewEditProc));

            cJSON *jsTab=cJSON_GetObjectItem(jsSettings,"tab_results");
            if (!jsTab)
                break;

            cJSON *jsDlg=cJSON_GetObjectItem(jsTab,"rescan_dlg");
            if (!jsDlg)
                break;

            SetWindowPos(hDlg,NULL,cJSON_GetIntFromObject(jsDlg,"x"),
                                   cJSON_GetIntFromObject(jsDlg,"y"),
                                   0,0,SWP_NOSIZE);
            break;
        }
        case WM_CLOSE:
        {
            cJSON *jsTab=cJSON_GetObjectItem(jsSettings,"tab_results");
            if (!jsTab)
            {
                jsTab=cJSON_CreateObject();
                cJSON_AddItemToObject(jsSettings,"tab_results",jsTab);
            }

            cJSON *jsDlg=cJSON_GetObjectItem(jsTab,"rescan_dlg");
            if (!jsDlg)
            {
                jsDlg=cJSON_CreateObject();
                cJSON_AddItemToObject(jsTab,"rescan_dlg",jsDlg);
            }

            WINDOWPLACEMENT wp;
            wp.length=sizeof(wp);

            if (GetWindowPlacement(hDlg,&wp))
            {
                Setting_ChangeValue(jsDlg,"x",wp.rcNormalPosition.left);
                Setting_ChangeValue(jsDlg,"y",wp.rcNormalPosition.top);

                StoreDlgPos();
            }

            EndDialog(hDlg,0);
            break;
        }
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDC_ADD:
                {
                    char szAddress[1024];
                    if (!GetDlgItemTextA(hDlg,IDC_EDIT_BOX,szAddress,ARRAYSIZE(szAddress)))
                        break;

                    char szIP[100];
                    WORD wPort=5900;
                    sscanf(szAddress,"%[^:]:%d",szIP,&wPort);

                    sockaddr_in sa;
                    if (inet_pton(AF_INET,szIP,&sa.sin_addr) != 1)
                    {
                        MessageBox(hDlg,_T("Bad address!"),NULL,MB_ICONEXCLAMATION);
                        break;
                    }

                    if ((wPort != 5900) || (StrChrA(szAddress,':')))
                        wsprintfA(szIP,"%s:%d",NetNtoA(sa.sin_addr.s_addr),wPort);
                    else
                        wsprintfA(szIP,"%s",NetNtoA(sa.sin_addr.s_addr));

                    if (lstrcmpA(szAddress,szIP))
                    {
                        MessageBox(hDlg,_T("Bad address!"),NULL,MB_ICONEXCLAMATION);
                        break;
                    }

                    if (Worker_FindServer(lpVoidConnection,sa.sin_addr.s_addr,wPort))
                    {
                        MessageBox(hDlg,_T("Already scanning"),NULL,MB_ICONINFORMATION);
                        break;
                    }

                    DWORD dwIdx=0;
                    if (DB_Worker_IsGoodAddressPresent(szAddress,&dwIdx))
                    {
                        DWORD dwRes=MessageBox(hDlg,_T("Password already found, scan from last position?"),NULL,MB_ICONQUESTION|MB_YESNOCANCEL);
                        if (dwRes == IDCANCEL)
                            break;

                        if (dwRes == IDNO)
                            dwIdx=0;
                    }

                    if (!DB_TotalIPs_IsPresent(sa.sin_addr.s_addr,wPort))
                        DB_TotalIPs_Append(sa.sin_addr.s_addr,wPort);

                    DB_Worker_Rescan(szAddress,dwIdx);
                    PostMessage(hDlg,WM_CLOSE,NULL,NULL);
                    break;
                }
            }
            break;
        }
    }
    return FALSE;
}

static HICON hFilterIcon;
static INT_PTR CALLBACK FilterDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            SendMessage(hDlg,WM_SETICON,ICON_BIG,(LPARAM)hFilterIcon);

            char szDesktop[1024];

            LVITEMA lvi={0};
            lvi.mask=LVIF_TEXT;
            lvi.cchTextMax=ARRAYSIZE(szDesktop);
            lvi.pszText=szDesktop;
            lvi.iItem=lParam;
            lvi.iSubItem=2;
            SendDlgItemMessageA(hResultsDlg,IDC_FOUND_LOG,LVM_GETITEMA,NULL,(LPARAM)&lvi);

            HWND hEdit=GetDlgItem(hDlg,IDC_EDIT_BOX);
            SetWindowTextA(hEdit,szDesktop);
            SetWindowLongPtr(hEdit,GWLP_USERDATA,(LONG_PTR)SetWindowLongPtr(hEdit,GWLP_WNDPROC,(LONG_PTR)NewEditProc));

            cJSON *jsTab=cJSON_GetObjectItem(jsSettings,"tab_results");
            if (!jsTab)
                break;

            cJSON *jsDlg=cJSON_GetObjectItem(jsTab,"filter_dlg");
            if (!jsDlg)
                break;

            SetWindowPos(hDlg,NULL,cJSON_GetIntFromObject(jsDlg,"x"),
                                   cJSON_GetIntFromObject(jsDlg,"y"),
                                   0,0,SWP_NOSIZE);
            break;
        }
        case WM_CLOSE:
        {
            cJSON *jsTab=cJSON_GetObjectItem(jsSettings,"tab_results");
            if (!jsTab)
            {
                jsTab=cJSON_CreateObject();
                cJSON_AddItemToObject(jsSettings,"tab_results",jsTab);
            }

            cJSON *jsDlg=cJSON_GetObjectItem(jsTab,"filter_dlg");
            if (!jsDlg)
            {
                jsDlg=cJSON_CreateObject();
                cJSON_AddItemToObject(jsTab,"filter_dlg",jsDlg);
            }

            WINDOWPLACEMENT wp;
            wp.length=sizeof(wp);

            if (GetWindowPlacement(hDlg,&wp))
            {
                Setting_ChangeValue(jsDlg,"x",wp.rcNormalPosition.left);
                Setting_ChangeValue(jsDlg,"y",wp.rcNormalPosition.top);

                StoreDlgPos();
            }

            EndDialog(hDlg,0);
            break;
        }
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDC_ADD:
                {
                    char szFilter[1024];
                    if (!GetDlgItemTextA(hDlg,IDC_EDIT_BOX,szFilter,ARRAYSIZE(szFilter)))
                        break;

                    if (DB_Worker_IsIgnored(szFilter))
                    {
                        MessageBox(hDlg,_T("Filter present."),NULL,0);
                        break;
                    }

                    DB_Worker_AddIgnoreFilter(szFilter);
                    PostMessage(hDlg,WM_CLOSE,NULL,NULL);
                    PostMessage(hResultsDlg,WM_COMMAND,IDM_RESCAN_IGNORED,NULL);
                    break;
                }
            }
            break;
        }
    }
    return FALSE;
}

void CreateFoundLog(HWND hDlg,LPCSTR lpGroup,LPCSTR lpValues)
{
    HWND hFoundLog=GetDlgItem(hDlg,IDC_FOUND_LOG);
    if (hFoundLog)
    {
        LOG_COLUMN lc[]=
        {
            {_T("Time"),119},
            {_T("Address"),78},
            {_T("Desktop"),135},
            {_T("Idx"),70},
            {_T("Password"),70},
        };

        cJSON *jsGroup=cJSON_GetObjectItem(jsSettings,lpGroup);
        if (!jsGroup)
        {
            jsGroup=cJSON_CreateObject();
            cJSON_AddItemToObject(jsSettings,lpGroup,jsGroup);
        }

        cJSON *jsColumns=cJSON_GetObjectItem(jsGroup,lpValues);
        if ((jsColumns) && (cJSON_GetArraySize(jsColumns) == ARRAYSIZE(lc)))
        {
            for (DWORD i=0; i < ARRAYSIZE(lc); i++)
                lc[i].dwCx=(DWORD)cJSON_GetNumberValue(cJSON_GetArrayItem(jsColumns,i));
        }
        else
        {
            cJSON_DeleteItemFromObject(jsGroup,lpValues);

            jsColumns=cJSON_AddArrayToObject(jsGroup,lpValues);
            for (DWORD i=0; i < ARRAYSIZE(lc); i++)
                cJSON_AddItemToArray(jsColumns,cJSON_CreateNumber(lc[i].dwCx));
        }

        LVCOLUMN lvc={0};
        lvc.mask=LVCF_TEXT|LVCF_WIDTH;
        lvc.cchTextMax=100;
        for (DWORD i=0; i < ARRAYSIZE(lc); i++)
        {
            lvc.cx=lc[i].dwCx;
            lvc.pszText=lc[i].lpLable;
            SendMessage(hFoundLog,LVM_INSERTCOLUMN,i,(LPARAM)&lvc);
        }

        SendMessage(hFoundLog,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_FLATSB|LVS_EX_LABELTIP|LVS_EX_DOUBLEBUFFER);
        SendMessage(hFoundLog,LVM_SETIMAGELIST,LVSIL_SMALL,(LPARAM)ImageList_Create(1,24,0,1,1));
    }
    return;
}

static void DisableTbIfNeeded(HWND hDlg)
{
    if (!SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETITEMCOUNT,NULL,NULL))
        SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_RESULT_CLEAR,false);
    return;
}

static void ChangeComment(HWND hDlg,DWORD dwSelected,LPCSTR lpComment)
{
    char szAddress[MAX_PATH];
    LVITEMA lvi={0};
    lvi.mask=LVIF_TEXT;
    lvi.cchTextMax=ARRAYSIZE(szAddress);
    lvi.pszText=szAddress;
    lvi.iItem=dwSelected;
    lvi.iSubItem=1;
    SendDlgItemMessageA(hDlg,IDC_FOUND_LOG,LVM_GETITEMA,NULL,(LPARAM)&lvi);

    DB_Worker_ChangeComment(szAddress,lpComment);

    SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_DELETEITEM,dwSelected,NULL);

    DisableTbIfNeeded(hDlg);
    return;
}

static DWORD GetResult(HWND hDlg,DWORD dwItem,DWORD dwSubItem,LPSTR lpData,DWORD dwDataLen)
{
    lpData[0]=0;

    LVITEMA lvi={0};
    lvi.mask=LVIF_TEXT;
    lvi.cchTextMax=dwDataLen;
    lvi.pszText=lpData;
    lvi.iItem=dwItem;
    lvi.iSubItem=dwSubItem;
    SendDlgItemMessageA(hDlg,IDC_FOUND_LOG,LVM_GETITEMA,NULL,(LPARAM)&lvi);

    DWORD dwLen=lstrlenA(lpData);
    if (dwLen)
    {
        if (!lstrcmpiA(lpData,"[empty]"))
            dwLen=0;
    }
    return dwLen;
}

static void CopyResult(HWND hDlg,DWORD dwItem,DWORD dwSubItem)
{
    HGLOBAL hData=NULL;
    bool bCopied=false;
    do
    {
        char szData[MAX_PATH];
        DWORD dwDataLen=GetResult(hDlg,dwItem,dwSubItem,szData,ARRAYSIZE(szData));
        if (!dwDataLen)
            break;

        hData=GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE,dwDataLen+1);
        if (!hData)
            break;

        if (!OpenClipboard(GetParent(hDlg)))
            break;

        LPSTR lpData=(LPSTR)GlobalLock(hData);
        if (!lpData)
        {
            CloseClipboard();
            break;
        }

        lstrcpyA(lpData,szData);
        GlobalUnlock(hData);

        EmptyClipboard();
        bCopied=(SetClipboardData(CF_TEXT,hData) != false);
        CloseClipboard();
    }
    while (false);

    if (!bCopied)
    {
        if (hData)
            GlobalFree(hData);
    }
    return;
}

static void WINAPI ListReadThread(HWND hDlg)
{
    TCHAR szFileName[MAX_PATH]={0};
    OPENFILENAME ofn={0};
    ofn.lStructSize=sizeof(ofn);
    ofn.hwndOwner=hDlg;
    ofn.lpstrFile=szFileName;
    ofn.nMaxFile=ARRAYSIZE(szFileName);
    ofn.lpstrFilter=_T("All files\0*.*\0\0");
    ofn.Flags=OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn))
        IPList_Parse(szFileName);

    SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_RESCAN_ADDRESS,true);
    return;
}

SYSTEM_INFO siInfo;
static HMENU hContextMenu,hDropMenu;
INT_PTR CALLBACK ResultsTabDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    PCOMM_TAB lpParam=(PCOMM_TAB)GetWindowLongPtr(hDlg,GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            lpParam=(PCOMM_TAB)lParam;
            SetWindowLongPtr(hDlg,GWLP_USERDATA,(LONG_PTR)lpParam);

            hResultsDlg=hDlg;

            CreateFoundLog(hDlg,"tab_results","columns");

            HWND hTB=GetDlgItem(hDlg,IDC_TBR1);

            SendMessage(hTB,TB_SETEXTENDEDSTYLE,0,(DWORD)SendMessage(hTB,TB_GETEXTENDEDSTYLE,0,0)|TBSTYLE_EX_DRAWDDARROWS);

            TBBUTTON tbb[]=
            {
                {4,IDM_RESCAN_ADDRESS,TBSTATE_ENABLED,TBSTYLE_BUTTON|TBSTYLE_DROPDOWN,0,0},
                {0,0,0,TBSTYLE_SEP,0,0},
                {6,IDM_MARK_INTERESTING,0,BTNS_BUTTON,0,0},
                {5,IDM_MARK_LOCKED,0,BTNS_BUTTON,0,0},
                {0,0,0,TBSTYLE_SEP,0,0},
                {3,IDM_DELETE,0,BTNS_BUTTON,0,0},
                {1,IDM_RESCAN,0,BTNS_BUTTON,0,0},
                {0,IDM_IGNORE,0,BTNS_BUTTON,0,0},
                {0,0,0,TBSTYLE_SEP,0,0},
                {2,IDM_RESULT_CLEAR,0,BTNS_BUTTON,0,0},
            };

            tbb[0].iString=SendMessage(hTB,TB_ADDSTRING,NULL,(LPARAM)_T("Rescan address\0"));
            tbb[2].iString=SendMessage(hTB,TB_ADDSTRING,NULL,(LPARAM)_T("Mark item as interesting\0"));
            tbb[3].iString=SendMessage(hTB,TB_ADDSTRING,NULL,(LPARAM)_T("Mark item as locked\0"));
            tbb[5].iString=SendMessage(hTB,TB_ADDSTRING,NULL,(LPARAM)_T("Delete item\0"));
            tbb[6].iString=SendMessage(hTB,TB_ADDSTRING,NULL,(LPARAM)_T("Scan further\0"));
            tbb[7].iString=SendMessage(hTB,TB_ADDSTRING,NULL,(LPARAM)_T("Ignore desktop\0"));
            tbb[9].iString=SendMessage(hTB,TB_ADDSTRING,NULL,(LPARAM)_T("Clean log\0"));

            HIMAGELIST hImgList=ImageList_LoadImage(hInstance,(LPCTSTR)102,16,NULL,0xFF00FF,IMAGE_BITMAP,LR_CREATEDIBSECTION);
            SendMessage(hTB,TB_SETIMAGELIST,NULL,(LPARAM)hImgList);
            hFilterIcon=ImageList_GetIcon(hImgList,0,0);
            hRescanIcon=ImageList_GetIcon(hImgList,4,0);

            HIMAGELIST hGrayImgList=ImageList_LoadImage(hInstance,(LPCTSTR)103,16,NULL,0xFF00FF,IMAGE_BITMAP,LR_CREATEDIBSECTION);
            SendMessage(hTB,TB_SETDISABLEDIMAGELIST,NULL,(LPARAM)hGrayImgList);

            SendMessage(hTB,TB_SETMAXTEXTROWS,0,0);
            SendMessage(hTB,TB_BUTTONSTRUCTSIZE,(WPARAM)sizeof(TBBUTTON),NULL);
            SendMessage(hTB,TB_ADDBUTTONS,ARRAYSIZE(tbb),(LPARAM)tbb);
            SendMessage(hTB,TB_AUTOSIZE,NULL,NULL);

            hContextMenu=GetSubMenu(LoadMenu(hInstance,MAKEINTRESOURCE(IDR_CONTEXT_RESULTS)),0);
            hDropMenu=GetSubMenu(LoadMenu(hInstance,MAKEINTRESOURCE(IDR_DROP_RESCAN)),0);

            GetSystemInfo(&siInfo);
            break;
        }
        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code)
            {
                case TBN_DROPDOWN:
                {
                    HWND hTB=((LPNMHDR)lParam)->hwndFrom;

                    RECT rc;
                    SendMessage(hTB,TB_GETRECT,((LPNMTOOLBAR)lParam)->iItem,(LPARAM)&rc);

                    POINT pt={0};
                    ClientToScreen(hTB,&pt);
                    TrackPopupMenuEx(hDropMenu,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_VERTICAL,rc.left+pt.x,rc.bottom+pt.y,hDlg,NULL);
                    break;
                }
                case HDN_ITEMCHANGED:
                {
                    cJSON *jsTab=cJSON_GetObjectItem(jsSettings,"tab_results");
                    if (!jsTab)
                        break;

                    cJSON *jsColumns=cJSON_GetObjectItem(jsTab,"columns");
                    if (!jsColumns)
                        break;

                    for (DWORD i=0; i < 5; i++)
                    {
                        DWORD dwWidth=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETCOLUMNWIDTH,i,NULL);
                        if (!dwWidth)
                            continue;

                        cJSON *jsItem=cJSON_GetArrayItem(jsColumns,i);
                        if (!jsItem)
                            continue;

                        cJSON_SetNumberValue(jsItem,dwWidth);
                    }

                    StoreDlgPos();
                    break;
                }
                case LVN_ITEMCHANGED:
                {
                    bool bEnable=(SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED) != -1);
                    SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_DELETE,bEnable);
                    SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_RESCAN,bEnable);
                    SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_IGNORE,bEnable);
                    SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_MARK_INTERESTING,bEnable);
                    SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_MARK_LOCKED,bEnable);
                    break;
                }
                case LVN_KEYDOWN:
                {
                    LPNMLVKEYDOWN nmlvk=(LPNMLVKEYDOWN)lParam;

                    if (nmlvk->wVKey == VK_DELETE)
                    {
                        DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                        if (dwSelected != -1)
                            PostMessage(hDlg,WM_COMMAND,IDM_DELETE,0);
                        else
                            PostMessage(hDlg,WM_COMMAND,IDM_RESULT_CLEAR,0);

                        break;
                    }
                    break;
                }
                case NM_RCLICK:
                {
                    DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                    if (dwSelected == -1)
                        break;

                    DWORD dwEnable=MF_BYCOMMAND;

                    char szData[MAX_PATH];
                    if (!GetResult(hDlg,dwSelected,2,szData,sizeof(szData)))
                        dwEnable|=MF_DISABLED|MF_GRAYED;

                    EnableMenuItem(hContextMenu,IDM_RESULT_COPY_DESKTOP,dwEnable);

                    dwEnable=MF_BYCOMMAND;
                    if (!GetResult(hDlg,dwSelected,4,szData,sizeof(szData)))
                        dwEnable|=MF_DISABLED|MF_GRAYED;

                    EnableMenuItem(hContextMenu,IDM_RESULT_COPY_PASSWORD,dwEnable);

                    POINT pt;
                    GetCursorPos(&pt);

                    TrackPopupMenu(hContextMenu,TPM_TOPALIGN|TPM_LEFTALIGN,pt.x,pt.y,0,hDlg,NULL);
                    break;
                }
            }
            break;
        }
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case CMD_NOTIFY_TAB:
                {
                    SendMessage(hMainDlg,WM_COMMAND,CMD_NOTIFY_TAB,lpParam->dwTabIdx);
                    break;
                }
                case IDM_MARK_INTERESTING:
                {
                    DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                    if (dwSelected == -1)
                    {
                        SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_MARK_INTERESTING,false);
                        break;
                    }

                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Mark item as interesting"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    ChangeComment(hDlg,dwSelected,"interesting");
                    break;
                }
                case IDM_MARK_LOCKED:
                {
                    DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                    if (dwSelected == -1)
                    {
                        SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_MARK_INTERESTING,false);
                        break;
                    }

                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Mark item as locked"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    ChangeComment(hDlg,dwSelected,"locked");
                    break;
                }
                case IDM_RESCAN_IGNORED:
                {
                    DWORD dwCount=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETITEMCOUNT,NULL,NULL);
                    if (!dwCount)
                        break;

                    for (DWORD i=dwCount; i > 0; i--)
                    {
                        char szDesktop[1024];

                        LVITEMA lvi={0};
                        lvi.mask=LVIF_TEXT;
                        lvi.cchTextMax=ARRAYSIZE(szDesktop);
                        lvi.pszText=szDesktop;
                        lvi.iItem=i-1;
                        lvi.iSubItem=2;
                        SendDlgItemMessageA(hDlg,IDC_FOUND_LOG,LVM_GETITEMA,NULL,(LPARAM)&lvi);

                        if (!DB_Worker_IsIgnored(szDesktop))
                            continue;

                        char szAddress[MAX_PATH];
                        lvi.cchTextMax=ARRAYSIZE(szAddress);
                        lvi.pszText=szAddress;
                        lvi.iSubItem=1;
                        SendDlgItemMessageA(hDlg,IDC_FOUND_LOG,LVM_GETITEMA,NULL,(LPARAM)&lvi);

                        DB_Worker_MoveToIgnored(szAddress);
                        SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_DELETEITEM,lvi.iItem,NULL);

                        PostMessage(hMainDlg,WM_COMMAND,IDM_RESCAN_IGNORED,NULL);

                        DisableTbIfNeeded(hDlg);
                    }
                    break;
                }
                case IDM_RESCAN_ADDRESS:
                {
                    DialogBoxParamW(hInstance,MAKEINTRESOURCE(IDD_RESCAN),hDlg,(DLGPROC)RescanDlgProc,NULL);
                    break;
                }
                case IDM_RESCAN_ADDRESS_LIST:
                {
                    SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_RESCAN_ADDRESS,false);
                    ThreadsGroup_CreateThread(hThreadsGroup,0,(LPTHREAD_START_ROUTINE)ListReadThread,hDlg,0,NULL);
                    break;
                }
                case IDM_IGNORE:
                {
                    DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                    if (dwSelected == -1)
                    {
                        SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_IGNORE,false);
                        break;
                    }

                    DialogBoxParamW(hInstance,MAKEINTRESOURCE(IDD_FILTER),hDlg,(DLGPROC)FilterDlgProc,dwSelected);

                    DisableTbIfNeeded(hDlg);
                    break;
                }
                case IDM_RESCAN:
                {
                    DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                    if (dwSelected == -1)
                    {
                        SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_RESCAN,false);
                        break;
                    }

                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Rescan"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    char szAddress[MAX_PATH];
                    LVITEMA lvi={0};
                    lvi.mask=LVIF_TEXT;
                    lvi.cchTextMax=ARRAYSIZE(szAddress);
                    lvi.pszText=szAddress;
                    lvi.iItem=dwSelected;
                    lvi.iSubItem=1;
                    SendDlgItemMessageA(hDlg,IDC_FOUND_LOG,LVM_GETITEMA,NULL,(LPARAM)&lvi);

                    char szIdx[MAX_PATH];
                    lvi.cchTextMax=ARRAYSIZE(szIdx);
                    lvi.pszText=szIdx;
                    lvi.iSubItem=3;
                    SendDlgItemMessageA(hDlg,IDC_FOUND_LOG,LVM_GETITEMA,NULL,(LPARAM)&lvi);

                    LONGLONG llIdx;
                    StrToInt64ExA(szIdx,0,&llIdx);
                    DB_Worker_Rescan(szAddress,llIdx);

                    SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_DELETEITEM,dwSelected,NULL);

                    DisableTbIfNeeded(hDlg);
                    break;
                }
                case IDM_DELETE:
                {
                    DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                    if (dwSelected == -1)
                    {
                        SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_DELETE,false);

                        PostMessage(hDlg,WM_COMMAND,IDM_RESULT_CLEAR,0);
                        break;
                    }

                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Delete"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_DELETEITEM,dwSelected,NULL);

                    DisableTbIfNeeded(hDlg);
                    break;
                }
                case IDM_RESULT_CLEAR:
                {
                    if (!SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETITEMCOUNT,NULL,NULL))
                    {
                        SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_RESULT_CLEAR,false);
                        break;
                    }

                    if (MessageBox(hDlg,_T("Are you sure?"),_T("Clean"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                        break;

                    Log_Clean(hDlg);
                    break;
                }
                case IDM_RESULT_COPY_ADDRESS:
                {
                    DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                    if (dwSelected == -1)
                        break;

                    CopyResult(hDlg,dwSelected,1);
                    break;
                }
                case IDM_RESULT_COPY_PASSWORD:
                {
                    DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                    if (dwSelected == -1)
                        break;

                    CopyResult(hDlg,dwSelected,4);
                    break;
                }
                case IDM_RESULT_COPY_DESKTOP:
                {
                    DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                    if (dwSelected == -1)
                        break;

                    CopyResult(hDlg,dwSelected,2);
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

            SendDlgItemMessage(hDlg,IDC_TBR1,TB_AUTOSIZE,NULL,NULL);

            MoveWindow(GetDlgItem(hDlg,IDC_FOUND_LOG),0,26,rc.right,rc.bottom-26,true);
            break;
        }
    }
    return FALSE;
}

