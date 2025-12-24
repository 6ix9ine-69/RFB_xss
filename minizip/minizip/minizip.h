#ifndef MINIZIP_H
#define MINIZIP_H

#define HZIP LPVOID

#ifdef __cplusplus
 extern "C" {
#endif

/// API для компрессии
HZIP CreateArchiveW(LPCWSTR lpstrZipFile,LPCSTR lpPassword,int dwPasswordLen,int dwCompLevel);
HZIP CreateArchiveA(LPCSTR lpstrZipFile,LPCSTR lpPassword,int dwPasswordLen,int dwCompLevel);

bool ArchAddFileW(HZIP hZip,LPCWSTR pstrSourceFile,LPCWSTR pstrDestFile);
bool ArchAddFileA(HZIP hZip,LPCSTR pstrSourceFile,LPCSTR pstrDestFile);

bool ArchCompressMemoryW(HZIP hZip,LPVOID lpMem,int dwSize,LPCWSTR pstrFile);
bool ArchCompressMemoryA(HZIP hZip,LPVOID lpMem,int dwSize,LPCSTR pstrFile);

#define CFF_RECURSE 0x1
#define CFF_DELETE 0x2

bool ArchiveFolderW(HZIP hZip,LPCWSTR sourceFolder,LPCWSTR *fileMask,DWORD fileMaskCount,DWORD flags);
bool ArchiveFolderA(HZIP hZip,LPCSTR sourceFolder,LPCSTR *fileMask,DWORD fileMaskCount,DWORD flags);

bool ArchCreateFromFolderW(LPCWSTR lpstrZipFile,LPCWSTR sourceFolder,LPCWSTR *fileMask,DWORD fileMaskCount,DWORD flags,LPCSTR lpPassword,int dwPasswordLen,int dwCompLevel);
bool ArchCreateFromFolderA(LPCSTR lpstrZipFile,LPCSTR sourceFolder,LPCSTR *fileMask,DWORD fileMaskCount,DWORD flags,LPCSTR lpPassword,int dwPasswordLen,int dwCompLevel);

#ifdef UNICODE
#define CreateArchive        CreateArchiveW
#define ArchAddFile          ArchAddFileW
#define ArchCompressMemory   ArchCompressMemoryW
#define ArchiveFolder        ArchiveFolderW
#define ArchCreateFromFolder ArchCreateFromFolderW
#else
#define CreateArchive        CreateArchiveA
#define ArchAddFile          ArchAddFileA
#define ArchCompressMemory   ArchCompressMemoryA
#define ArchiveFolder        ArchiveFolderA
#define ArchCreateFromFolder ArchCreateFromFolderA
#endif


/// API для декомпрессии

typedef struct _FILE_IN_ARCH_INFO
{
    DWORD dwCrc32;
    DWORD dwDosDate;
    ULONGLONG dwCompressedSize;
    ULONGLONG dwDecompressedSize;
} FILE_IN_ARCH_INFO, *PFILE_IN_ARCH_INFO;

typedef BOOL WINAPI ARCHENUMNAMESCALLBACKW(LPCWSTR lpstrFile,const PFILE_IN_ARCH_INFO lpInfo);
typedef BOOL WINAPI ARCHENUMNAMESCALLBACKA(LPCSTR lpstrFile,const PFILE_IN_ARCH_INFO lpInfo);

#ifdef __cplusplus
HZIP OpenArchiveW(LPCWSTR lpstrZipFile,LPCSTR lpPassword,int dwPasswordLen,bool bZipInMem=false,int dwFileSize=0);
#else
HZIP OpenArchiveW(LPCWSTR lpstrZipFile,LPCSTR lpPassword,int dwPasswordLen,bool bZipInMem,int dwFileSize);
#endif

#ifdef __cplusplus
HZIP OpenArchiveA(LPCSTR lpstrZipFile,LPCSTR lpPassword,int dwPasswordLen,bool bZipInMem=false,int dwFileSize=0);
#else
HZIP OpenArchiveA(LPCSTR lpstrZipFile,LPCSTR lpPassword,int dwPasswordLen,bool bZipInMem,int dwFileSize);
#endif

bool ArchExtractFileW(HZIP hZip,LPCWSTR pstrPath,LPCWSTR lpstrFile);
bool ArchExtractFileA(HZIP hZip,LPCSTR pstrPath,LPCSTR lpstrFile);

bool ArchExtractFilesW(HZIP hZip,LPCWSTR pstrPath);
bool ArchExtractFilesA(HZIP hZip,LPCSTR pstrPath);

bool ArchGetFileW(HZIP hZip,LPCWSTR pstrFile,LPBYTE *lpMem,int *dwSize);
bool ArchGetFileA(HZIP hZip,LPCSTR pstrFile,LPBYTE *lpMem,int *dwSize);

bool ArchEnumFilesW(HZIP hZip,ARCHENUMNAMESCALLBACKW *lpCallback);
bool ArchEnumFilesA(HZIP hZip,ARCHENUMNAMESCALLBACKA *lpCallback);

bool ArchGetFileInfoW(HZIP hZip,LPCWSTR pstrFile,PFILE_IN_ARCH_INFO lpInfo);
bool ArchGetFileInfoA(HZIP hZip,LPCSTR pstrFile,PFILE_IN_ARCH_INFO lpInfo);

#ifdef UNICODE
#define OpenArchive      OpenArchiveW
#define ArchExtractFile  ArchExtractFileW
#define ArchExtractFiles ArchExtractFilesW
#define ArchGetFile      ArchGetFileW
#define ArchEnumFiles    ArchEnumFilesW
#define ArchGetFileInfo  ArchGetFileInfoW
#define ARCHENUMNAMESCALLBACK ARCHENUMNAMESCALLBACKW
#else
#define OpenArchive      OpenArchiveA
#define ArchExtractFiles ArchExtractFilesA
#define ArchExtractFile  ArchExtractFileA
#define ArchGetFile      ArchGetFileA
#define ArchEnumFiles    ArchEnumFilesA
#define ArchGetFileInfo  ArchGetFileInfoA
#define ARCHENUMNAMESCALLBACK ARCHENUMNAMESCALLBACKA
#endif

void ArchClose(HZIP hZip);
int ArchGetLastError();

#define ARCH_ZIP_IS_ENCRYPTED   2
#define ARCH_ZIP_NOT_FOUND      3
#define ARCH_FILE_NOT_FOUND     4
#define ARCH_INVALID_PARAMETER  5
#define ARCH_NO_MEM             6
#define ARCH_FILE_TOO_BIG       7

void ArchSetComment(HZIP hZip,char *lpComment);
bool ArchGetComment(HZIP hZip,char *lpComment,DWORD dwCommentSize);

#ifdef __cplusplus
 }
#endif

#define ZIP_NO_CRYPT 2
bool ArchCompressMemoryEx(HZIP hZip,void *lpMem,int dwSize,LPCWSTR pstrFile,DWORD flags);
bool ArchAddFileEx(HZIP hZip,LPCWSTR pstrSourceFile,LPCWSTR pstrDestFile,DWORD flags);

#endif /* MINIZIP_H */
