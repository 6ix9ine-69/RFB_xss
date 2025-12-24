#ifndef COMMON_DLG_H_INCLUDED
#define COMMON_DLG_H_INCLUDED

typedef struct _SERVER_INFO
{
    SOCKET hSock;
    cJSON *jsServer;
} SERVER_INFO, *PSERVER_INFO;

extern SERVER_INFO siWorker,siChecker;

#define WM_SOCKET WM_USER+1
#define TM_PARSE_PACKET WM_USER+11

enum WM_CMD
{
    CMD_EXIT=200,
    CMD_NOTIFY_TAB,
    CMD_IS_SPLITTING,
    CMD_CANCEL_SPLIT,
    CMD_UPDATE_TABS,
};

typedef struct _COMM_TAB
{
    CONNECTION_TYPE dwConnectionType;

    DWORD dwItems;
    DWORD dwOnline;
    DWORD dwPrevOnline;

    DWORD dwTabIdx;
    cJSON *jsConfig;

    PSERVER_INFO lpServerInfo;
} COMM_TAB, *PCOMM_TAB;

INT_PTR CALLBACK CommonTabDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);

void Disconnect(PCONNECTION lpConnection);
void UpdateStats(PCONNECTION lpConnection);

enum CONNECTION_STATUS
{
    CS_ONLINE=0,
    CS_OFFLINE,
    CS_N_A,
    CS_IDLE,
    CS_ROOT,
    CS_PAUSED
};

extern HWND hMainDlg;

LRESULT GridView_ProcessCustomDraw(LPARAM lParam,bool bWorker,bool bRoot=false);
void GridView_AddItem(HWND hList,LPLVITEMA lpLVI,LPSTR lpLabel,LPSTR lpValue,DWORD dwRowId=0);
void GridView_Init(HWND hList);

enum TIMERS
{
    IDT_UPDATE_STATS=1,
    IDT_WAIT_RESULTS,
    IDT_PING,
    IDT_UPDATE_UPTIME,
};

extern CONNECTION_TYPE dwCurTab;

typedef struct _GLOBAL_STATS
{
    DWORD dwItemsToBrute;
    DWORD dwWorkersTasks;
    DWORD dwBrutted;
    DWORD dwUnsupported;
    DWORD dwWorkerProgress;

    DWORD dwItemsToCheck;
    DWORD dwCheckersTasks;
    DWORD dwChecked;
    DWORD dwRFB;
    DWORD dwNotRFB;
    DWORD dwUnavailble;
    DWORD dwCheckerProgress;
} GLOBAL_STATS, *PGLOBAL_STATS;

extern HMENU hContextMenuRoot,hContextMenuItem;

void SetItemBold(PCONNECTION lpConnection);
void UpdateConnectionStatus(PCONNECTION lpConnection,DWORD dwStatus);

#endif // COMMON_DLG_H_INCLUDED
