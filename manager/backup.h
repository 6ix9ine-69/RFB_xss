#ifndef BACKUP_H_INCLUDED
#define BACKUP_H_INCLUDED

void Backup_Init(cJSON *jsConfig);
void Backup_Cleanup();
void Backup_BackupNow();

void Backup_BackupOnExit();

typedef struct _BACKUP_FILE:_DOUBLY_LINKED_LIST
{
	WCHAR szName[MAX_PATH];
	FILETIME ftCreation;
} BACKUP_FILE, *PBACKUP_FILE;

#define TM_BACKUP_NOW WM_USER+10

enum BACKUP_TYPE
{
    BACKUP_REGULAR,
    BACKUP_ONLOAD,
    BACKUP_ONUNLOAD,
    BACKUP_MANUAL,
};

typedef struct _BACKUP_COMPRESS_PARAMS
{
    char szBackupPath[MAX_PATH];
    BACKUP_TYPE dwType;
} BACKUP_COMPRESS_PARAMS, *PBACKUP_COMPRESS_PARAMS;

void Backup_UpdateNextBackupTime(HWND hDlg);
void Backup_UpdateMenuState(HMENU hMenu,DWORD dwItem);

#endif // BACKUP_H_INCLUDED
