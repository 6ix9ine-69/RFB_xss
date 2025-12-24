#ifndef COMMON_DLG_H_INCLUDED
#define COMMON_DLG_H_INCLUDED

#include <cjson\cjson.h>

#include "socket.h"

#define WM_SOCKET WM_USER+1
#define WM_PWD_FOUND WM_USER+2
#define TM_PARSE_PACKET WM_USER+11

#define WM_TRAYMSG WM_APP

enum TIMERS
{
    IDT_GET_ITEMS=1,
    IDT_UPDATE_STATS,
    IDT_RESUME,
    IDT_RECONNECT
};

#define RECONNECT_TIMEOUT 10*MILLISECONDS_PER_SECOND

enum WM_CMD
{
    CMD_SAY_HELLO=100,
    CMD_INIT_ITEMS,
    CMD_SEND_RESULT,
    CMD_SEND_GETITEMS_CMD,
    CMD_PING,
    CMD_SEND_STATUS
};

enum
{
    IDM_CLEAN=10001,
    IDM_DELETE,
};

extern SOCKET_INFO siManager;
extern HINSTANCE hInstance;
extern HWND hCommonDlg;
extern HFONT hBoldFont;
extern HBRUSH hBrush;
INT_PTR CALLBACK CommonDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);

DWORD ParseGetItemsResults(cJSON *jsItems);

void SendResults(cJSON *jsResult);
void GetItems();
bool IsActiveTasks();

cJSON *GetUnsentResults();
cJSON *GetStats();
void UpdateStats();

enum ICON_ID
{
    II_ONLINE=0,
    II_OFFLINE,
    II_N_A,
    II_IDLE,
    II_UNK,
    II_PAUSED
};

void InitDlg();

void Log_Clean();
void StoreDlgPos();
void ResizeDlg();

LRESULT GridView_ProcessCustomDraw(LPARAM lParam);

#define SYSMENU_TOPMOST_ID 1050

bool IsOnline();

#endif // COMMON_DLG_H_INCLUDED
