#ifndef CONTROL_DLG_H_INCLUDED
#define CONTROL_DLG_H_INCLUDED

enum
{
    IDM_RESULT_CLEAR=10001,
    IDM_DELETE,
    IDM_RESCAN,
    IDM_IGNORE,
    IDM_MARK_INTERESTING,
    IDM_MARK_LOCKED
};

void Results_Append(LPCSTR lpTime,LPCSTR lpAddress,LPCSTR lpDesktop,DWORD dwIdx,LPCSTR lpPassword);

INT_PTR CALLBACK ResultsTabDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);

void CreateFoundLog(HWND hDlg,LPCSTR lpGroup,LPCSTR lpValues);

typedef struct _LOG_COLUMN
{
    LPTSTR lpLable;
    DWORD dwCx;
} LOG_COLUMN, *PLOG_COLUMN;

extern cJSON *jsSettings;

void StoreDlgPos();

#endif // CONTROL_DLG_H_INCLUDED
