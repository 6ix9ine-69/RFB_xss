#ifndef INCLUDES_H_INCLUDED
#define INCLUDES_H_INCLUDED

#include <ws2tcpip.h>
#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>
#include <commctrl.h>

#include "common\config.h"
#include <sqlite\sqlite3.h>

#include <syslib\net.h>
#include <syslib\mem.h>
#include <syslib\threadsgroup.h>
#include <syslib\time.h>
#include <syslib\str.h>

#include "connections.h"
#include "worker.h"
#include "checker.h"
#include "pushed.h"
#include "db.h"
#include "common_dlg.h"
#include "splash.h"
#include "debug_log_dlg.h"

#include "res\res.h"

extern HANDLE hShutdownEvent,hThreadsGroup;
extern HINSTANCE hInstance;

void UpdateGlobalStats();

void EnableDBMainanceMenu(bool bEnable);

void PostThreadMessageEx(DWORD idThread,UINT Msg,WPARAM wParam,LPARAM lParam);

#define SYSMENU_TOPMOST_ID 1050

void CalcDHMS(DWORD dwSecs,LPSTR lpTime);

void UpdateMenuState(HMENU hMenu,DWORD dwItem,bool bChecked);

#endif // INCLUDES_H_INCLUDED
