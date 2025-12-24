#ifndef DEBUG_LOG_DLG_H_INCLUDED
#define DEBUG_LOG_DLG_H_INCLUDED

#include "common\dbg_log.h"

#define ADD_LOG_MESSAGE WM_USER+100
#define CLEAR_LOG_MESSAGE WM_USER+101

typedef struct _LOG_ITEM
{
    PCONNECTION lpConnection;
    DWORD dwTime;
    LOG_LEVEL dwLevel;
    LPSTR lpFile;
    DWORD dwLine;
    LPSTR lpFunc;
    LPSTR lpBody;
} LOG_ITEM, *PLOG_ITEM;

void DebugLog_AddItemEx(PCONNECTION lpConnection,DWORD dwTime,LOG_LEVEL dwLevel,LPCSTR lpFile,DWORD dwLine,LPCSTR lpFunc,LPCSTR lpFormat,...);
void DebugLog_AddItem(PCONNECTION lpConnection,cJSON *jsLog);

#define DebugLog_AddItem2(lpConnection,dwLevel,...) DebugLog_AddItemEx(lpConnection,Now(),dwLevel,__FILE__,__LINE__,__FUNCTION__,__VA_ARGS__)

INT_PTR CALLBACK DebugLogDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);

void DebugLog_UpdateMenuState(HMENU hMenu,DWORD dwItem);
void DebugLog_Init(cJSON *jsConfig);

#endif // DEBUG_LOG_DLG_H_INCLUDED
