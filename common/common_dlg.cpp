#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <tchar.h>

#include <syslib\mem.h>
#include <syslib\net.h>
#include <syslib\time.h>
#include <syslib\threadsgroup.h>

#include "res\res.h"
#include "socket.h"
#include "dbg_log.h"
#include "stopping_dlg.h"
#include "sticky.h"
#include "common_dlg.h"
#include "json_packet.h"
#include "iocp.h"
#include "connection_check.h"

SOCKET_INFO siManager={0};
HINSTANCE hInstance;
HWND hCommonDlg;

static void StopWork(HWND hDlg)
{
    if (!IOCP_IsStopped())
    {
        KillTimer(hDlg,IDT_GET_ITEMS);
        InterlockedExchange(&bIOCP_StopWork,1);
    }
    return;
}

static bool bResumming;
static void ResumeWork(HWND hDlg)
{
    do
    {
        if (bResumming)
            break;

        if (!IOCP_IsStopped())
            break;

        bResumming=true;
        SetTimer(hDlg,IDT_RESUME,100,NULL);
    }
    while (false);
    return;
}

static void PostThreadMessageEx(DWORD idThread,UINT Msg,WPARAM wParam,LPARAM lParam)
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

static DWORD dwItemsRequested;
static CRITICAL_SECTION csGetItems;
void GetItems()
{
    DWORD dwMsg=CMD_PING,dwCount=0;

    if (IsOnline())
    {
        EnterCriticalSection(&csGetItems);
        {
            do
            {
                if (InterlockedExchangeAdd(&dwActiveTasks,0) >= dwTotalActiveTasks)
                    break;

                dwCount=dwTotalActiveTasks-InterlockedExchangeAdd(&dwActiveTasks,0);
                if (dwCount <= dwItemsRequested)
                    break;

                dwCount-=dwItemsRequested;
                dwItemsRequested+=dwCount;
                dwMsg=CMD_SEND_GETITEMS_CMD;
            }
            while (false);
        }
        LeaveCriticalSection(&csGetItems);
    }

    PostMessage(hCommonDlg,WM_COMMAND,dwMsg,dwCount);
    return;
}

bool IsActiveTasks()
{
    return (InterlockedExchangeAdd(&dwActiveTasks,0) != 0);
}

void SendResults(cJSON *jsResult)
{
    PostMessage(hCommonDlg,WM_COMMAND,CMD_SEND_RESULT,(LPARAM)jsResult);
    return;
}

static bool bMinimized;
static HICON hIcons[6];
static DWORD dwCurIcon=-1;
static void ChangeIcon(DWORD dwIdx)
{
    if (dwCurIcon != dwIdx)
    {
        SendMessage(hCommonDlg,WM_SETICON,ICON_BIG,(LPARAM)hIcons[dwIdx]);
        SendMessage(hCommonDlg,WM_SETICON,ICON_SMALL,(LPARAM)hIcons[dwIdx]);
        if (bMinimized)
        {
            NOTIFYICONDATAA nid={sizeof(nid)};
            nid.uFlags=NIF_ICON;
            nid.hIcon=hIcons[dwIdx];
            nid.hWnd=hCommonDlg;
            nid.uID=1;
            Shell_NotifyIconA(NIM_MODIFY,&nid);
        }

        dwCurIcon=dwIdx;
    }
    return;
}

void Disconnect(HWND hDlg,SOCKET hSocket)
{
    WSAAsyncSelect(hSocket,hDlg,0,0);
    NetCloseSocket(hSocket);

    if (hSocket == siManager.hSock)
    {
        siManager.bConnected=false;
        siManager.hSock=INVALID_SOCKET;
    }

    StopWork(hDlg);

    ChangeIcon(II_OFFLINE);

    SetTimer(hDlg,IDT_RECONNECT,RECONNECT_TIMEOUT,NULL);
    return;
}

static DWORD dwCmdParsingThreadId;
static void CloseDlg(HWND hDlg)
{
    PostThreadMessageEx(dwCmdParsingThreadId,WM_QUIT,NULL,NULL);
    InetCheck_Stop();
    EndDialog(hDlg,0);
    return;
}

static DWORD GetStatusIcon()
{
    DWORD dwIcon;
    do
    {
        if (IOCP_IsStopped())
        {
            dwIcon=II_PAUSED;
            break;
        }

        if (!IsOnline())
        {
            dwIcon=II_N_A;
            break;
        }

        if (IsActiveTasks())
        {
            dwIcon=II_ONLINE;
            break;
        }

        dwIcon=II_IDLE;
    }
    while (false);
    return dwIcon;
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

        cJSON *jsCmd=(cJSON *)msg.lParam;
        if (!jsCmd)
            continue;

        if (!lstrcmpiA(cJSON_GetStringFromObject(jsCmd,"type"),"cmd"))
        {
            LPCSTR lpCmd=cJSON_GetStringFromObject(jsCmd,"cmd");
            if (!lstrcmpiA(lpCmd,"stop"))
                StopWork(hCommonDlg);
            else if (!lstrcmpiA(lpCmd,"resume"))
                ResumeWork(hCommonDlg);
            else if (!lstrcmpiA(lpCmd,"ping"))
            {
                cJSON *jsCmdPong=cJSON_CreateObject();
                if (jsCmdPong)
                {
                    cJSON_AddStringToObject(jsCmdPong,"type","cmd");
                    cJSON_AddStringToObject(jsCmdPong,"cmd","pong");

                    JSON_SendPacketAndFree(&siManager,jsCmdPong);
                }
            }
        }
        else if (!lstrcmpiA(cJSON_GetStringFromObject(jsCmd,"type"),"result"))
        {
            if (!lstrcmpiA(cJSON_GetStringFromObject(jsCmd,"cmd"),"get_items"))
            {
                DWORD dwReceived=ParseGetItemsResults(cJSON_GetObjectItem(jsCmd,"result"));

                EnterCriticalSection(&csGetItems);
                    dwItemsRequested-=cJSON_GetIntFromObject(jsCmd,"requested");
                LeaveCriticalSection(&csGetItems);
            }
        }
        cJSON_Delete(jsCmd);
    }
    return;
}

static HMENU hSysMenu;
static bool bClossing=false;
INT_PTR CALLBACK CommonDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    INT_PTR dwRet=FALSE;

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            InitializeCriticalSection(&siManager.csSocket);

            hCommonDlg=hDlg;
            InitializeCriticalSection(&csGetItems);

            hSysMenu=GetSystemMenu(hDlg,false);
            InsertMenu(hSysMenu,5,MF_BYPOSITION|MF_SEPARATOR,0,NULL);
            InsertMenu(hSysMenu,6,MF_BYPOSITION|MF_STRING,SYSMENU_TOPMOST_ID,_T("Always on top"));

            HANDLE hEvent=CreateEvent(NULL,false,false,NULL);
            CloseHandle(CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CmdParsingThread,hEvent,0,&dwCmdParsingThreadId));
            WaitForSingleObject(hEvent,INFINITE);
            CloseHandle(hEvent);

            HIMAGELIST hImgList=ImageList_LoadImage(hInstance,(LPCTSTR)101,16,NULL,0xFF00FF,IMAGE_BITMAP,LR_CREATEDIBSECTION);
            if (hImgList)
            {
                for (DWORD i=0; i < ARRAYSIZE(hIcons); i++)
                    hIcons[i]=ImageList_GetIcon(hImgList,i,0);

                ImageList_Destroy(hImgList);
            }

            ChangeIcon(II_OFFLINE);

            SetTimer(hDlg,IDT_RECONNECT,RECONNECT_TIMEOUT,NULL);
            PostMessage(hDlg,WM_TIMER,IDT_RECONNECT,NULL);

            InitDlg();

            UpdateStats();
            SetTimer(hDlg,IDT_UPDATE_STATS,MILLISECONDS_PER_SECOND,NULL);

            InetCheck_Start(hDlg);
            break;
        }
        case WM_MOUSEMOVE:
        {
            if (wParam == MK_LBUTTON)
            {
                ReleaseCapture();
                SendMessage(hDlg,WM_NCLBUTTONDOWN,HTCAPTION,0);
            }
            break;
        }
        case WM_TIMER:
        {
            switch (wParam)
            {
                case IDT_RECONNECT:
                {
                    if (siManager.hSock != INVALID_SOCKET)
                        break;

                    siManager.hSock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
                    if (siManager.hSock == INVALID_SOCKET)
                        break;

                    WSAAsyncSelect(siManager.hSock,hDlg,WM_SOCKET,FD_CONNECT|FD_READ|FD_CLOSE);

                    if (connect(siManager.hSock,(sockaddr *)&siManager.si,(sizeof(siManager.si))) != SOCKET_ERROR)
                        break;

                    if (WSAGetLastError() == WSAEWOULDBLOCK)
                        break;

                    Disconnect(hDlg,siManager.hSock);
                    break;
                }
                case IDT_RESUME:
                {
                    if (IsActiveTasks())
                        break;

                    bResumming=false;
                    KillTimer(hDlg,IDT_RESUME);
                    InterlockedExchange(&bIOCP_StopWork,0);
                    PostMessage(hDlg,WM_COMMAND,CMD_INIT_ITEMS,NULL);
                    break;
                }
                case IDT_UPDATE_STATS:
                {
                    UpdateStats();

                    if (!siManager.bConnected)
                        break;

                    ChangeIcon(GetStatusIcon());
                    break;
                }
                case IDT_GET_ITEMS:
                {
                    GetItems();
                    break;
                }
            }
            break;
        }
        case WM_SOCKET:
        {
            if (WSAGETSELECTERROR(lParam))
            {
                Disconnect(hDlg,wParam);
                break;
            }

            switch (WSAGETSELECTEVENT(lParam))
            {
                case FD_CONNECT:
                {
                    KillTimer(hDlg,IDT_RECONNECT);

                    siManager.bConnected=true;
                    siManager.dwReceived=0;
                    siManager.dwNeedToRecv=0;

                    SendMessage(hDlg,WM_COMMAND,CMD_SAY_HELLO,NULL);
                    ResumeWork(hDlg);

                    ChangeIcon(II_IDLE);
                    break;
                }
                case FD_READ:
                {
                    if (!siManager.dwNeedToRecv)
                    {
                        int iRecv=recv(wParam,(LPSTR)&siManager.dwNeedToRecv,sizeof(siManager.dwNeedToRecv),0);
                        if ((!iRecv) || (iRecv == SOCKET_ERROR))
                            break;
                    }

                    if (siManager.dwRecvBufSize < siManager.dwNeedToRecv)
                    {
                        siManager.lpRecvBuf=(PCHAR)MemRealloc(siManager.lpRecvBuf,siManager.dwNeedToRecv);
                        if (siManager.lpRecvBuf)
                            siManager.dwRecvBufSize=siManager.dwNeedToRecv;
                        else
                            DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"MemRealloc(%d) failed",siManager.dwNeedToRecv);
                    }

                    PCHAR lpPtr=siManager.lpRecvBuf+siManager.dwReceived;
                    DWORD dwNeedToRecv=siManager.dwNeedToRecv-siManager.dwReceived;
                    int iRecv=recv(wParam,lpPtr,dwNeedToRecv,0);
                    if ((!iRecv) || (iRecv == SOCKET_ERROR))
                        break;

                    siManager.dwReceived+=iRecv;
                    if (siManager.dwReceived < siManager.dwNeedToRecv)
                        break;

                    PostThreadMessageEx(dwCmdParsingThreadId,TM_PARSE_PACKET,NULL,(LPARAM)cJSON_ParseWithLength(siManager.lpRecvBuf,siManager.dwNeedToRecv));

                    siManager.dwReceived=0;
                    siManager.dwNeedToRecv=0;
                    break;
                }
                case FD_CLOSE:
                {
                    Disconnect(hDlg,wParam);
                    break;
                }
            }
            break;
        }
        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code)
            {
#ifdef WORKER
                case HDN_ITEMCHANGED:
                {
                    StoreDlgPos();
                    break;
                }
                case LVN_ITEMCHANGED:
                {
                    bool bEnable=(SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED) != -1);
                    SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_DELETE,bEnable);
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
                            PostMessage(hDlg,WM_COMMAND,IDM_CLEAN,0);

                        break;
                    }
                    break;
                }
#else
                case NM_CUSTOMDRAW:
                {
                    SetWindowLongPtr(hDlg,DWLP_MSGRESULT,(LONG_PTR)GridView_ProcessCustomDraw(lParam));
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
#endif
            }
            break;
        }
        case WM_COMMAND:
        {
            switch (wParam)
            {
                case IDCANCEL:
                {
                    PostMessage(hDlg,WM_CLOSE,NULL,NULL);
                    break;
                }
                case CMD_SEND_STATUS:
                {
                    cJSON *jsStatus=cJSON_CreateObject();
                    if (jsStatus)
                    {
                        cJSON_AddStringToObject(jsStatus,"type","status");
                        cJSON_AddStringToObject(jsStatus,"status",(LPCSTR)lParam);
                        JSON_SendPacketAndFree(&siManager,jsStatus);
                    }
                    break;
                }
                case CMD_SAY_HELLO:
                {
                    cJSON *jsHello=cJSON_CreateObject();
                    if (jsHello)
                    {
                        cJSON_AddStringToObject(jsHello,"type","hello");
                        cJSON_AddItemToObject(jsHello,"result",GetUnsentResults());
                        cJSON_AddItemToObject(jsHello,"stat",GetStats());

                        if (IsOnline())
                            cJSON_AddStringToObject(jsHello,"status","online");
                        else
                            cJSON_AddStringToObject(jsHello,"status","n/a");

                        JSON_SendPacketAndFree(&siManager,jsHello);
                    }
                    break;
                }
                case CMD_INIT_ITEMS:
                {
                    EnterCriticalSection(&csGetItems);
                        InterlockedExchange(&dwItemsRequested,0);
                    LeaveCriticalSection(&csGetItems);

                    GetItems();

                    SetTimer(hDlg,IDT_GET_ITEMS,MILLISECONDS_PER_MINUTE,NULL);
                    break;
                }
                case CMD_SEND_GETITEMS_CMD:
                {
                    cJSON *jsGetItems=cJSON_CreateObject();
                    if (jsGetItems)
                    {
                        cJSON_AddStringToObject(jsGetItems,"type","cmd");
                        cJSON_AddStringToObject(jsGetItems,"cmd","get_items");

                        if (lParam > 1)
                            cJSON_AddIntToObject(jsGetItems,"items",lParam);

                        JSON_SendPacketAndFree(&siManager,jsGetItems);
                    }
                    break;
                }
                case CMD_SEND_RESULT:
                {
                    cJSON *jsResultData=(cJSON *)lParam;
                    if (jsResultData)
                    {
                        cJSON *jsResult=cJSON_CreateObject();
                        if (jsResult)
                        {
                            cJSON_AddStringToObject(jsResult,"type","result");
                            cJSON_AddItemToObject(jsResult,"result",jsResultData);
                            cJSON_AddIntToObject(jsResult,"active_tasks",InterlockedExchangeAdd(&dwActiveTasks,0));

                            JSON_SendPacketAndFree(&siManager,jsResult);
                        }
                        else
                            cJSON_Delete(jsResultData);
                    }

                    if ((bClossing) && (!IsActiveTasks()))
                    {
                        CloseDlg(hDlg);
                        break;
                    }
                    break;
                }
                case CMD_PING:
                {
                    cJSON *jsCmdPing=cJSON_CreateObject();
                    if (jsCmdPing)
                    {
                        cJSON_AddStringToObject(jsCmdPing,"type","cmd");
                        cJSON_AddStringToObject(jsCmdPing,"cmd","ping");

                        JSON_SendPacketAndFree(&siManager,jsCmdPing);
                    }
                    break;
                }
#ifdef WORKER
                case IDM_DELETE:
                {
                    DWORD dwSelected=SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETNEXTITEM,-1,LVNI_SELECTED);
                    if (dwSelected == -1)
                    {
                        SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_DELETE,false);
                        break;
                    }

                    SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_DELETEITEM,dwSelected,NULL);

                    if (SendDlgItemMessage(hDlg,IDC_FOUND_LOG,LVM_GETITEMCOUNT,NULL,NULL))
                        break;

                    SendDlgItemMessage(hDlg,IDC_TBR1,TB_ENABLEBUTTON,IDM_CLEAN,false);
                    break;
                }
                case IDM_CLEAN:
                {
                    Log_Clean();
                    break;
                }
#endif
            }
            break;
        }
#ifdef WORKER
        case WM_PWD_FOUND:
        {
            if (!bMinimized)
                break;

            NOTIFYICONDATAA nid={sizeof(nid)};
            nid.uFlags=NIF_INFO;
            nid.hWnd=hDlg;
            nid.uID=1;
            lstrcpyA(nid.szInfo,"New password found!");
            lstrcpyA(nid.szInfoTitle,FILE_DESCRIPTON);
            nid.dwInfoFlags=NIIF_INFO;
            nid.uTimeout=-1;
            Shell_NotifyIconA(NIM_MODIFY,&nid);
            break;
        }
#endif
        case_WM_SICKY:
        {
            HandleStickyMsg(hDlg,uMsg,wParam,lParam);
            break;
        }
        case WM_TRAYMSG:
        {
            switch (lParam)
            {
                case WM_LBUTTONUP:
                {
                    ShowWindow(hDlg,SW_RESTORE);

                    BringWindowToTop(hDlg);
                    SetForegroundWindow(hDlg);
                    break;
                }
                case WM_RBUTTONDOWN:
                case WM_CONTEXTMENU:
                {
                    ///ShowContextMenu(hWnd);
                    break;
                }
            }
            break;
        }
        case WM_SIZE:
        {
            if (wParam == SIZE_MINIMIZED)
            {
                NOTIFYICONDATAA nid={sizeof(nid)};
                nid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
                nid.hIcon=hIcons[dwCurIcon];
                nid.hWnd=hDlg;
                nid.uID=1;
                nid.uCallbackMessage=WM_TRAYMSG;
                lstrcpyA(nid.szTip,FILE_DESCRIPTON);
                Shell_NotifyIconA(NIM_ADD,&nid);

                ShowWindow(hDlg,SW_HIDE);
                bMinimized=true;
                break;
            }

            if (wParam == SIZE_RESTORED)
            {
                if (bMinimized)
                {
                    NOTIFYICONDATAA nid={sizeof(nid)};
                    nid.hWnd=hDlg;
                    nid.uID=1;
                    Shell_NotifyIconA(NIM_DELETE,&nid);

                    bMinimized=false;
                    break;
                }
            }

            ResizeDlg();
            break;
        }
        case WM_GETMINMAXINFO:
        {
            LPMINMAXINFO lpMinInfo=(LPMINMAXINFO)lParam;

            lpMinInfo->ptMinTrackSize.x=318*1.5;
#ifdef WORKER
            lpMinInfo->ptMinTrackSize.y=204*1.5;
#else
            lpMinInfo->ptMinTrackSize.y=6*24+12+45;
#endif
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

            if (bClossing)
                break;

            if (MessageBox(hDlg,_T("Are you sure?"),_T("Stop work"),MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
                break;

            StoreDlgPos();

            EnableMenuItem(GetSystemMenu(hDlg,false),SC_CLOSE,MF_BYCOMMAND|MF_DISABLED|MF_GRAYED);

            StopWork(hDlg);

            if ((!siManager.bConnected) || (!IsActiveTasks()))
            {
                CloseDlg(hDlg);
                break;
            }

            bClossing=true;

            CreateDialogParam(hInstance,MAKEINTRESOURCE(IDD_STOPPING),hDlg,StoppingDlgProc,NULL);
            break;
        }
    }
    return dwRet;
}

