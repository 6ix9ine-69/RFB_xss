#include "includes.h"
#include <ntdll.h>
#include <stdio.h>

#include <syslib\files.h>
#include <syslib\str.h>
#include <minizip.h>

#include "backup.h"
#include "mergesort.h"

static bool Backup_Cmp(PDOUBLY_LINKED_LIST lpFirst,PDOUBLY_LINKED_LIST lpSecond)
{
    LARGE_INTEGER li1;
    li1.LowPart=((PBACKUP_FILE)lpFirst)->ftCreation.dwLowDateTime;
    li1.HighPart=((PBACKUP_FILE)lpFirst)->ftCreation.dwHighDateTime;

    LARGE_INTEGER li2;
    li2.LowPart=((PBACKUP_FILE)lpSecond)->ftCreation.dwLowDateTime;
    li2.HighPart=((PBACKUP_FILE)lpSecond)->ftCreation.dwHighDateTime;

    return (li1.QuadPart > li2.QuadPart);
}

static LPCSTR lpBackupDirA;
static DWORD dwCopies;
static void Backup_RotateBackups()
{
    PBACKUP_FILE lpList=NULL;
    do
    {
        WCHAR szBackupDir[MAX_PATH];
        wsprintfW(szBackupDir,L"%S\\*.zip",lpBackupDirA);

        WIN32_FIND_DATAW wfd;
        HANDLE hFind=FindFirstFileW(szBackupDir,&wfd);
        if (hFind == INVALID_HANDLE_VALUE)
            break;

        DWORD dwCount=0;
        PBACKUP_FILE lpLastItem=NULL;
        do
        {
            if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            PBACKUP_FILE lpItem=(PBACKUP_FILE)MemQuickAlloc(sizeof(*lpItem));
            if (!lpItem)
                break;

            lstrcpyW(lpItem->szName,wfd.cFileName);
            lpItem->ftCreation=wfd.ftCreationTime;

            lpItem->lpNext=NULL;
            lpItem->lpJump=NULL;

            if (!lpList)
            {
                lpList=lpItem;
                lpLastItem=lpItem;
                lpItem->lpPrev=NULL;
            }
            else
            {
                lpLastItem->lpNext=lpItem;
                lpItem->lpPrev=lpLastItem;

                lpLastItem=lpItem;
            }

            dwCount++;
        }
        while (FindNextFileW(hFind,&wfd));
        FindClose(hFind);

        if (dwCount <= dwCopies)
            break;

        MergeSort((PDOUBLY_LINKED_LIST*)&lpList,(PDOUBLY_LINKED_LIST*)&lpLastItem,Backup_Cmp);

        for (DWORD i=dwCopies; i < dwCount; i++)
        {
            WCHAR szOldFile[MAX_PATH];
            wsprintfW(szOldFile,L"%S\\%s",lpBackupDirA,lpLastItem->szName);
            DeleteFile(szOldFile);

            lpLastItem=(PBACKUP_FILE)lpLastItem->lpPrev;
        }
    }
    while (false);

    while (lpList)
    {
        PBACKUP_FILE lpNext=(PBACKUP_FILE)lpList->lpNext;
        MemFree(lpList);

        lpList=lpNext;
    }
	return;
}

static void Backup_BackupDB(sqlite3 *hDB,LPCSTR lpPath)
{
    char szBackupFile[MAX_PATH];
    wsprintfA(szBackupFile,"%s\\%s",lpPath,PathFindFileNameA(sqlite3_db_filename(hDB,"main")));

    sqlite3 *hBackupDB;
    if (sqlite3_open(szBackupFile,&hBackupDB) == SQLITE_OK)
    {
        sqlite3_backup *hBackup=sqlite3_backup_init(hBackupDB,"main",hDB,"main");
        if (hBackup)
        {
            sqlite3_backup_step(hBackup,-1);
            sqlite3_backup_finish(hBackup);
        }
        sqlite3_close(hBackupDB);
    }
    return;
}

static char szConfigFile[MAX_PATH];
static void Backup_BackupConfig(LPCSTR lpPath)
{
    char szBackupFile[MAX_PATH];
    wsprintfA(szBackupFile,"%s\\%s",lpPath,szConfigFile);

    Config_Dump(szBackupFile);
    return;
}

static DWORD dwInterval,dwIntervalInSecs;
static volatile DWORD dwLastBackupTime,dwBackupInProgress;
void Backup_UpdateNextBackupTime(HWND hDlg)
{
    TCHAR szTime[MAX_PATH];
    if (dwIntervalInSecs)
    {
        LARGE_INTEGER liTime;
        RtlSecondsSince1980ToTime(InterlockedExchangeAdd(&dwLastBackupTime,0)+dwIntervalInSecs,&liTime);

        FILETIME ft;
        ft.dwHighDateTime=liTime.HighPart;
        ft.dwLowDateTime=liTime.LowPart;

        FILETIME ftLocal;
        FileTimeToLocalFileTime(&ft,&ftLocal);

        SYSTEMTIME st;
        FileTimeToSystemTime(&ftLocal,&st);

        wsprintf(szTime,_T("Next backup: %.4d/%.2d/%.2d %.2d:%.2d:%.2d"),st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    }
    else
        lstrcpy(szTime,_T("Next backup: never"));

    SendDlgItemMessage(hDlg,IDC_SBR1,SB_SETTEXT,0,(LPARAM)szTime);
    return;
}

static bool bOnLoad,bOnUnload;
static cJSON *jsBackup;
void Backup_UpdateMenuState(HMENU hMenu,DWORD dwItem)
{
    switch (dwItem)
    {
        case -1:
        {
            UpdateMenuState(hMenu,IDM_BACKUP_ONLOAD,bOnLoad);
            UpdateMenuState(hMenu,IDM_BACKUP_ONUNLOAD,bOnUnload);
            break;
        }
        case IDM_BACKUP_ONLOAD:
        {
            bOnLoad=!bOnLoad;
            UpdateMenuState(hMenu,dwItem,bOnLoad);

            cJSON *jsScheduler=cJSON_GetObjectItem(jsBackup,"scheduler");
            if (jsScheduler)
            {
                cJSON *jsOnLoad=cJSON_GetObjectItem(jsScheduler,"on_load");
                if (jsOnLoad)
                    cJSON_SetBoolValue(jsOnLoad,bOnLoad);
            }

            Config_Update();
            break;
        }
        case IDM_BACKUP_ONUNLOAD:
        {
            bOnUnload=!bOnUnload;
            UpdateMenuState(hMenu,dwItem,bOnUnload);

            cJSON *jsScheduler=cJSON_GetObjectItem(jsBackup,"scheduler");
            if (jsScheduler)
            {
                cJSON *jsOnUnload=cJSON_GetObjectItem(jsScheduler,"on_unload");
                if (jsOnUnload)
                    cJSON_SetBoolValue(jsOnUnload,bOnUnload);
            }

            Config_Update();
            break;
        }
    }
    return;
}

static void EnableBackups(bool bEnable)
{
    do
    {
        if (!hMainDlg)
            break;

        HMENU hMenu=GetMenu(hMainDlg);
        if (!hMenu)
            break;

        DWORD dwEnable=MF_BYCOMMAND;
        if (!bEnable)
        {
            dwEnable|=MF_DISABLED|MF_GRAYED;
            SendDlgItemMessage(hMainDlg,IDC_SBR1,SB_SETTEXT,0,(LPARAM)_T("Next backup: In progress..."));
        }
        else
            Backup_UpdateNextBackupTime(hMainDlg);

        EnableMenuItem(hMenu,IDM_BACKUP_NOW,dwEnable);
    }
    while (false);
    return;
}

static DWORD dwCompress=5;
static void Backup_Compress(PBACKUP_COMPRESS_PARAMS lpParams)
{
    char szZipFile[MAX_PATH];
    wsprintfA(szZipFile,"%s\\%s.zip",lpBackupDirA,PathFindFileNameA(lpParams->szBackupPath));

    HZIP hZip=CreateArchiveA(szZipFile,NULL,0,dwCompress);
    if (hZip)
    {
        LPCSTR lpMask="*.*";
        ArchiveFolderA(hZip,lpParams->szBackupPath,&lpMask,1,CFF_DELETE);

        WORD wYear,wMonth,wDay,wHour,wMinute,wSecond;
        sscanf(PathFindFileNameA(lpParams->szBackupPath),"%d.%d.%d %d-%d-%d",&wYear,&wMonth,&wDay,&wHour,&wMinute,&wSecond);

        LPCSTR lpType="Regular Backup";

        switch (lpParams->dwType)
        {
            case BACKUP_ONLOAD:
            {
                lpType="On_Load Backup";
                break;
            }
            case BACKUP_ONUNLOAD:
            {
                lpType="On_UnLoad Backup";
                break;
            }
            case BACKUP_MANUAL:
            {
                lpType="Manual Backup";
                break;
            }
        }

        char szComment[512];
        wsprintfA(szComment,"%.4d/%.2d/%.2d %.2d:%.2d:%.2d\r\n\r\nXSS.is RFB Brute / %s",wYear,wMonth,wDay,wHour,wMinute,wSecond,lpType);

        ArchSetComment(hZip,szComment);
        ArchClose(hZip);
    }
    RemoveDirectoryTreeA(lpParams->szBackupPath);

    Backup_RotateBackups();

    InterlockedExchange(&dwLastBackupTime,Now());
    InterlockedExchange(&dwBackupInProgress,0);
    return;
}

static void WINAPI Backup_OnLoadThread(PBACKUP_COMPRESS_PARAMS lpBCP)
{
    while (true)
    {
        if (WaitForSingleObject(hShutdownEvent,1) == WAIT_OBJECT_0)
            break;

        if (hMainDlg)
            break;
    }

    EnableDBMainanceMenu(false);
    EnableBackups(false);
    {
        Backup_Compress(lpBCP);
        MemFree(lpBCP);
    }
    EnableBackups(true);
    EnableDBMainanceMenu(true);
    return;
}

static void Backup_BackupNowInt(LPSTR lpBackupPath)
{
    EnableDBMainanceMenu(false);
    {
        SYSTEMTIME st;
        GetLocalTime(&st);

        wsprintfA(lpBackupPath,"%s\\%.4d.%.2d.%.2d %.2d-%.2d-%.2d",lpBackupDirA,st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
        CreateDirectoryTreeA(lpBackupPath);

        if (hWorkerDB)
        {
            EnterCriticalSection(&csWorkerAccess);
                Backup_BackupDB(hWorkerDB,lpBackupPath);
            LeaveCriticalSection(&csWorkerAccess);
        }

        if (hCheckerDB)
        {
            EnterCriticalSection(&csMasscanAccess);
                Backup_BackupDB(hCheckerDB,lpBackupPath);
            LeaveCriticalSection(&csMasscanAccess);
        }

        Backup_BackupConfig(lpBackupPath);
    }
    EnableDBMainanceMenu(true);
    return;
}

static DWORD dwBackupThreadId;
static void WINAPI Backup_BackupThread(HANDLE hEvent)
{
    MSG msg;
    PeekMessage(&msg,NULL,WM_USER,WM_USER,PM_NOREMOVE);
    SetEvent(hEvent);

    BACKUP_COMPRESS_PARAMS bcp;
    while ((int)GetMessage(&msg,NULL,0,0) > 0)
    {
        if (msg.message != TM_BACKUP_NOW)
            continue;

        bcp.dwType=(BACKUP_TYPE)msg.wParam;
        EnableBackups(false);
        {
            Backup_BackupNowInt(bcp.szBackupPath);
            Backup_Compress(&bcp);
        }
        EnableBackups(true);
    }
    return;
}

void Backup_BackupNow()
{
    InterlockedExchange(&dwBackupInProgress,1);
    PostThreadMessage(dwBackupThreadId,TM_BACKUP_NOW,BACKUP_MANUAL,NULL);
    return;
}

static void WINAPI Backup_SchedulerThread(LPVOID)
{
    while (WaitForSingleObject(hShutdownEvent,MILLISECONDS_PER_SECOND) == WAIT_TIMEOUT)
    {
        if (InterlockedExchangeAdd(&dwBackupInProgress,0))
            continue;

        if ((Now()-InterlockedExchangeAdd(&dwLastBackupTime,0)) < dwIntervalInSecs)
            continue;

        InterlockedExchange(&dwBackupInProgress,1);
        PostThreadMessage(dwBackupThreadId,TM_BACKUP_NOW,BACKUP_REGULAR,NULL);
    }
    return;
}

void Backup_BackupOnExit()
{
    if (bOnUnload)
    {
        Splash_SetText(_T("Backup..."));

        InterlockedExchange(&dwBackupInProgress,1);

        BACKUP_COMPRESS_PARAMS bcp;
        bcp.dwType=BACKUP_ONUNLOAD;
        Backup_BackupNowInt(bcp.szBackupPath);
        Backup_Compress(&bcp);
    }
    return;
}

static HANDLE hBackupThreadsGroup;
void Backup_Cleanup()
{
    Splash_SetText(_T("Waiting for backup..."));

    if (hBackupThreadsGroup)
    {
        PostThreadMessage(dwBackupThreadId,WM_QUIT,NULL,NULL);
        ThreadsGroup_WaitForAllExit(hBackupThreadsGroup,INFINITE);
        ThreadsGroup_CloseGroup(hBackupThreadsGroup);
        hBackupThreadsGroup=NULL;
    }
    return;
}

void Backup_Init(cJSON *jsConfig)
{
    do
    {
        Splash_SetText(_T("Initializing backups..."));

        jsBackup=cJSON_GetObjectItem(jsConfig,"backup");
        if (!jsBackup)
            break;

        lpBackupDirA=cJSON_GetStringFromObject(jsBackup,"path");
        if (!lpBackupDirA)
            break;

        HANDLE hEvent=CreateEvent(NULL,false,false,NULL);
        if (!hEvent)
            break;

        hBackupThreadsGroup=ThreadsGroup_Create();
        if (!hBackupThreadsGroup)
        {
            CloseHandle(hEvent);
            break;
        }

        ThreadsGroup_CreateThread(hBackupThreadsGroup,0,(LPTHREAD_START_ROUTINE)Backup_BackupThread,hEvent,&dwBackupThreadId,NULL);
        WaitForSingleObject(hEvent,INFINITE);
        CloseHandle(hEvent);

        CreateDirectoryTreeA(lpBackupDirA);

        dwCompress=(DWORD)cJSON_GetIntFromObject(jsBackup,"compress");
        if ((dwCompress < 0) || (dwCompress > 9))
            dwCompress=5;

        dwCopies=(DWORD)cJSON_GetIntFromObject(jsBackup,"copies");

        cJSON *jsScheduler=cJSON_GetObjectItem(jsBackup,"scheduler");
        if (!jsScheduler)
            break;

        dwInterval=(DWORD)cJSON_GetIntFromObject(jsScheduler,"interval");
        if (!dwInterval)
            dwInterval=720;

        dwIntervalInSecs=dwInterval*60;

        bOnLoad=cJSON_GetBoolFromObject(jsScheduler,"on_load");
        bOnUnload=cJSON_GetBoolFromObject(jsScheduler,"on_unload");

        int argc=0;
        LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        if ((argc == 3) && (!lstrcmpiW(argv[1],L"-c")))
            wsprintfA(szConfigFile,"%S",PathFindFileNameW(argv[2]));
        else
            lstrcpyA(szConfigFile,"config.json");

        LocalFree(argv);

        InterlockedExchange(&dwLastBackupTime,Now());
        ThreadsGroup_CreateThread(hBackupThreadsGroup,0,(LPTHREAD_START_ROUTINE)Backup_SchedulerThread,NULL,NULL,NULL);

        if (!bOnLoad)
            break;

        PBACKUP_COMPRESS_PARAMS lpBCP=(PBACKUP_COMPRESS_PARAMS)MemQuickAlloc(sizeof(*lpBCP));
        if (!lpBCP)
            break;

        InterlockedExchange(&dwBackupInProgress,1);

        lpBCP->dwType=BACKUP_ONLOAD;
        Backup_BackupNowInt(lpBCP->szBackupPath);
        ThreadsGroup_CreateThread(hBackupThreadsGroup,0,(LPTHREAD_START_ROUTINE)Backup_OnLoadThread,lpBCP,NULL,NULL);
    }
    while (false);
    return;
}

