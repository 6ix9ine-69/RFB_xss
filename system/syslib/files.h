#ifndef SYSLIB_FILES_H_INCLUDED
#define SYSLIB_FILES_H_INCLUDED

#include "syslib_exp.h"

SYSLIBEXP(BOOL) RemoveFileW(LPCWSTR lpFile);
SYSLIBEXP(BOOL) RemoveFileA(LPCSTR lpFile);

#ifdef UNICODE
    #define RemoveFile RemoveFileW
#else
    #define RemoveFile RemoveFileA
#endif

#define FFF_RECURSIVE 1
#define FFF_SEARCH_FOLDERS 2
#define FFF_SEARCH_FILES 4

typedef struct
{
    BOOL bFirstFileInDir;
    WIN32_FIND_DATAW wfd;
} FILE_INFOW, *PFILE_INFOW;

typedef BOOL (__cdecl FINDFILEPROCW)(LPCWSTR lpPath,PFILE_INFOW lpFileInfo,LPVOID lpData);
SYSLIBEXP(void) FindFilesW(LPCWSTR lpPath,LPCWSTR *lppFileMasks,DWORD dwFileMasksCount,DWORD dwFlags,FINDFILEPROCW *lpFindFileProc,LPVOID lpData,DWORD dwSubfolderDelay,DWORD dwFoundedDelay);

typedef struct
{
    BOOL bFirstFileInDir;
    WIN32_FIND_DATAA wfd;
} FILE_INFOA, *PFILE_INFOA;

typedef BOOL (__cdecl FINDFILEPROCA)(LPCSTR lpPath,PFILE_INFOA lpFileInfo,LPVOID lpData);
SYSLIBEXP(void) FindFilesA(LPCSTR lpPath,LPCSTR *lppFileMasks,DWORD dwFileMasksCount,DWORD dwFlags,FINDFILEPROCA *lpFindFileProc,LPVOID lpData,DWORD dwSubfolderDelay,DWORD dwFoundedDelay);

#ifdef UNICODE
    #define FILE_INFO FILE_INFOW
    #define PFILE_INFO PFILE_INFOW
    #define FINDFILEPROC FINDFILEPROCW
    #define FindFiles FindFilesW
#else
    #define FILE_INFO FILE_INFOA
    #define PFILE_INFO PFILE_INFOA
    #define FINDFILEPROC FINDFILEPROCA
    #define FindFiles FindFilesA
#endif

SYSLIBEXP(BOOL) CreateDirectoryTreeW(LPCWSTR lpPath);
SYSLIBEXP(BOOL) CreateDirectoryTreeA(LPCSTR lpPath);

#ifdef UNICODE
    #define CreateDirectoryTree CreateDirectoryTreeW
#else
    #define CreateDirectoryTree CreateDirectoryTreeA
#endif

SYSLIBEXP(BOOL) RemoveDirectoryTreeW(LPCWSTR lpDir);
SYSLIBEXP(BOOL) RemoveDirectoryTreeA(LPCSTR lpDir);

#ifdef UNICODE
    #define RemoveDirectoryTree RemoveDirectoryTreeW
#else
    #define RemoveDirectoryTree RemoveDirectoryTreeA
#endif

#endif // SYSLIB_FILES_H_INCLUDED
