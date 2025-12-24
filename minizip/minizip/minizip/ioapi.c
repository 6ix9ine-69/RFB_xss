/* iowin32.c -- IO base function header for compress/uncompress .zip
   Version 1.1, February 14h, 2010
   part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

   Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

   Modifications for Zip64 support
   Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )

   For more info read MiniZip_info.txt
*/

#if (defined(_WIN32))
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS
    #endif
    #include <tchar.h>
//    #define snprintf _snprintf
#endif

#include <windows.h>
#include <shlwapi.h>

#include "zlib.h"
#include "ioapi.h"
#include "iowin32.h"

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE (0xFFFFFFFF)
#endif

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif

#include <syslib\str.h>
#include <syslib\mem.h>

#define malloc(x) MemAlloc(x)
#define free(x) MemFree(x)

voidpf  ZCALLBACK win32_open_file_func  OF((voidpf opaque, const char* filename, int mode));
uLong   ZCALLBACK win32_read_file_func  OF((voidpf opaque, voidpf stream, void* buf, uLong size));
uLong   ZCALLBACK win32_write_file_func OF((voidpf opaque, voidpf stream, const void* buf, uLong size));
ZPOS64_T ZCALLBACK win32_tell64_file_func  OF((voidpf opaque, voidpf stream));
long    ZCALLBACK win32_seek64_file_func  OF((voidpf opaque, voidpf stream, ZPOS64_T offset, int origin));
int     ZCALLBACK win32_close_file_func OF((voidpf opaque, voidpf stream));
int     ZCALLBACK win32_error_file_func OF((voidpf opaque, voidpf stream));

typedef struct
{
    HANDLE hf;
    int error;
    void *filename;
} WIN32FILE_IOWIN;

voidpf call_zopen64 (const zlib_filefunc64_32_def* pfilefunc,const void*filename,int mode)
{
    if (pfilefunc->zfile_func64.zopen64_file != NULL)
        return (*(pfilefunc->zfile_func64.zopen64_file)) (pfilefunc->zfile_func64.opaque,filename,mode);
    return (*(pfilefunc->zopen32_file))(pfilefunc->zfile_func64.opaque,(const char*)filename,mode);
}

voidpf call_zopendisk64 OF((const zlib_filefunc64_32_def* pfilefunc, voidpf filestream, int number_disk, int mode))
{
    if (pfilefunc->zfile_func64.zopendisk64_file != NULL)
        return (*(pfilefunc->zfile_func64.zopendisk64_file)) (pfilefunc->zfile_func64.opaque,filestream,number_disk,mode);
    return (*(pfilefunc->zopendisk32_file))(pfilefunc->zfile_func64.opaque,filestream,number_disk,mode);
}

long call_zseek64 (const zlib_filefunc64_32_def* pfilefunc,voidpf filestream, ZPOS64_T offset, int origin)
{
    long offsetTruncated;
    if (pfilefunc->zfile_func64.zseek64_file != NULL)
        return (*(pfilefunc->zfile_func64.zseek64_file)) (pfilefunc->zfile_func64.opaque,filestream,offset,origin);
    offsetTruncated = (long)offset;
    if (offsetTruncated != offset)
        return -1;
    return (*(pfilefunc->zseek32_file))(pfilefunc->zfile_func64.opaque,filestream,offsetTruncated,origin);
}

ZPOS64_T call_ztell64 (const zlib_filefunc64_32_def* pfilefunc,voidpf filestream)
{
    uLong tell_uLong;
    if (pfilefunc->zfile_func64.zseek64_file != NULL)
        return (*(pfilefunc->zfile_func64.ztell64_file)) (pfilefunc->zfile_func64.opaque,filestream);
    tell_uLong = (*(pfilefunc->ztell32_file))(pfilefunc->zfile_func64.opaque,filestream);
    if ((tell_uLong) == 0xffffffff)
        return (ZPOS64_T)-1;
    return tell_uLong;
}

static void win32_translate_open_mode(int mode,
                                      DWORD* lpdwDesiredAccess,
                                      DWORD* lpdwCreationDisposition,
                                      DWORD* lpdwShareMode,
                                      DWORD* lpdwFlagsAndAttributes)
{
    *lpdwDesiredAccess = *lpdwShareMode = *lpdwFlagsAndAttributes = *lpdwCreationDisposition = 0;

    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
    {
        *lpdwDesiredAccess = GENERIC_READ;
        *lpdwCreationDisposition = OPEN_EXISTING;
        *lpdwShareMode = FILE_SHARE_READ;
    }
    else if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
    {
        *lpdwDesiredAccess = GENERIC_WRITE | GENERIC_READ;
        *lpdwCreationDisposition = OPEN_EXISTING;
    }
    else if (mode & ZLIB_FILEFUNC_MODE_CREATE)
    {
        *lpdwDesiredAccess = GENERIC_WRITE | GENERIC_READ;
        *lpdwCreationDisposition = CREATE_ALWAYS;
    }
}

static voidpf win32_build_iowin(HANDLE hFile)
{
    WIN32FILE_IOWIN *iowin = NULL;

    if ((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE))
    {
        iowin = (WIN32FILE_IOWIN *)malloc(sizeof(WIN32FILE_IOWIN));
        if (iowin==NULL)
        {
            CloseHandle(hFile);
            return NULL;
        }
        memset(iowin, 0, sizeof(WIN32FILE_IOWIN));
        iowin->hf = hFile;
    }
    return (voidpf)iowin;
}

voidpf ZCALLBACK win32_open64_file_func (voidpf opaque,const void* filename,int mode)
{
    const char* mode_fopen = NULL;
    DWORD dwDesiredAccess,dwCreationDisposition,dwShareMode,dwFlagsAndAttributes ;
    HANDLE hFile = NULL;
    WIN32FILE_IOWIN *iowin = NULL;

    win32_translate_open_mode(mode,&dwDesiredAccess,&dwCreationDisposition,&dwShareMode,&dwFlagsAndAttributes);

    if ((filename!=NULL) && (dwDesiredAccess != 0))
        hFile = CreateFile((LPCTSTR)filename, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwFlagsAndAttributes, NULL);

    iowin = win32_build_iowin(hFile);
    if (iowin == NULL)
        return NULL;
    iowin->filename = (void*)StrDuplicate((LPCTSTR)filename,0);
    return iowin;
}


voidpf ZCALLBACK win32_open64_file_funcA (voidpf opaque,const void* filename,int mode)
{
    const char* mode_fopen = NULL;
    DWORD dwDesiredAccess,dwCreationDisposition,dwShareMode,dwFlagsAndAttributes ;
    HANDLE hFile = NULL;
    WIN32FILE_IOWIN *iowin = NULL;

    win32_translate_open_mode(mode,&dwDesiredAccess,&dwCreationDisposition,&dwShareMode,&dwFlagsAndAttributes);

    if ((filename!=NULL) && (dwDesiredAccess != 0))
        hFile = CreateFileA((LPCSTR)filename, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwFlagsAndAttributes, NULL);

    iowin = win32_build_iowin(hFile);
    if (iowin == NULL)
        return NULL;
    iowin->filename = (void*)StrDuplicateA((LPCSTR)filename,0);
    return iowin;
}


voidpf ZCALLBACK win32_open64_file_funcW (voidpf opaque,const void* filename,int mode)
{
    const char* mode_fopen = NULL;
    DWORD dwDesiredAccess,dwCreationDisposition,dwShareMode,dwFlagsAndAttributes ;
    HANDLE hFile = NULL;
    WIN32FILE_IOWIN *iowin = NULL;

    win32_translate_open_mode(mode,&dwDesiredAccess,&dwCreationDisposition,&dwShareMode,&dwFlagsAndAttributes);

    if ((filename!=NULL) && (dwDesiredAccess != 0))
    {
        hFile = CreateFileW((LPCWSTR)filename, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwFlagsAndAttributes, NULL);
    }
    iowin = win32_build_iowin(hFile);
    if (iowin == NULL)
        return NULL;
    if (iowin->filename == NULL)
        iowin->filename = (void*)StrDuplicateW((LPCWSTR)filename,0);
    return iowin;
}

voidpf ZCALLBACK win32_open_file_func (voidpf opaque,const char* filename,int mode)
{
    const char* mode_fopen = NULL;
    DWORD dwDesiredAccess,dwCreationDisposition,dwShareMode,dwFlagsAndAttributes ;
    HANDLE hFile = NULL;
    WIN32FILE_IOWIN *iowin = NULL;

    win32_translate_open_mode(mode,&dwDesiredAccess,&dwCreationDisposition,&dwShareMode,&dwFlagsAndAttributes);

    if ((filename!=NULL) && (dwDesiredAccess != 0))
        hFile = CreateFile((LPCTSTR)filename, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwFlagsAndAttributes, NULL);

    iowin = win32_build_iowin(hFile);
    if (iowin == NULL)
        return NULL;
    iowin->filename = (void*)StrDuplicate((LPCTSTR)filename,0);
    return iowin;
}

voidpf ZCALLBACK win32_opendisk64_file_func (voidpf opaque, voidpf stream, int number_disk, int mode)
{
    WIN32FILE_IOWIN *iowin = NULL;
    TCHAR *diskFilename = NULL;
    voidpf ret = NULL;
    int i = 0;

    if (stream == NULL)
        return NULL;
    iowin = (WIN32FILE_IOWIN*)stream;
    if (StrFormatEx(&diskFilename,_T("%s.z%02d"),iowin->filename,number_disk + 1))
    {
        ret = win32_open64_file_func(opaque, diskFilename, mode);
        free(diskFilename);
    }
    return ret;
}

voidpf ZCALLBACK win32_opendisk64_file_funcW (voidpf opaque, voidpf stream, int number_disk, int mode)
{
    WIN32FILE_IOWIN *iowin = NULL;
    WCHAR *diskFilename = NULL;
    voidpf ret = NULL;
    int i = 0;

    if (stream == NULL)
        return NULL;
    iowin = (WIN32FILE_IOWIN*)stream;
    if (StrFormatExW(&diskFilename,L"%s.z%02d",iowin->filename,number_disk + 1))
    {
        ret = win32_open64_file_funcW(opaque, diskFilename, mode);
        free(diskFilename);
    }
    return ret;
}

voidpf ZCALLBACK win32_opendisk64_file_funcA (voidpf opaque, voidpf stream, int number_disk, int mode)
{
    WIN32FILE_IOWIN *iowin = NULL;
    char *diskFilename = NULL;
    voidpf ret = NULL;
    int i = 0;

    if (stream == NULL)
        return NULL;
    iowin = (WIN32FILE_IOWIN*)stream;
    if (StrFormatExA(&diskFilename,"%s.z%02d",iowin->filename,number_disk + 1))
    {
        ret = win32_open64_file_funcA(opaque, diskFilename, mode);
        free(diskFilename);
    }
    return ret;
}

voidpf ZCALLBACK win32_opendisk_file_func (voidpf opaque, voidpf stream, int number_disk, int mode)
{
    WIN32FILE_IOWIN *iowin = NULL;
    char *diskFilename = NULL;
    voidpf ret = NULL;
    int i = 0;

    if (stream == NULL)
        return NULL;
    iowin = (WIN32FILE_IOWIN*)stream;
    if (StrFormatExA(&diskFilename,"%s.z%02d",iowin->filename,number_disk + 1))
    {
        ret = win32_open_file_func(opaque, diskFilename, mode);
        free(diskFilename);
    }
    return ret;
}

uLong ZCALLBACK win32_read_file_func (voidpf opaque, voidpf stream, void* buf,uLong size)
{
    uLong ret=0;
    HANDLE hFile = NULL;
    if (stream!=NULL)
        hFile = ((WIN32FILE_IOWIN*)stream) -> hf;

    if (hFile != NULL)
    {
        if (!ReadFile(hFile, buf, size, &ret, NULL))
        {
            DWORD dwErr = GetLastError();
            if (dwErr == ERROR_HANDLE_EOF)
                dwErr = 0;
            ((WIN32FILE_IOWIN*)stream) -> error=(int)dwErr;
        }
    }

    return ret;
}

uLong ZCALLBACK win32_write_file_func (voidpf opaque,voidpf stream,const void* buf,uLong size)
{
    uLong ret=0;
    HANDLE hFile = NULL;
    if (stream!=NULL)
        hFile = ((WIN32FILE_IOWIN*)stream) -> hf;

    if (hFile != NULL)
    {
        if (!WriteFile(hFile, buf, size, &ret, NULL))
        {
            DWORD dwErr = GetLastError();
            if (dwErr == ERROR_HANDLE_EOF)
                dwErr = 0;
            ((WIN32FILE_IOWIN*)stream) -> error=(int)dwErr;
        }
    }

    return ret;
}

long ZCALLBACK win32_tell_file_func (voidpf opaque,voidpf stream)
{
    long ret=-1;
    HANDLE hFile = NULL;
    if (stream!=NULL)
        hFile = ((WIN32FILE_IOWIN*)stream) -> hf;
    if (hFile != NULL)
    {
        DWORD dwSet = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
        if (dwSet == INVALID_SET_FILE_POINTER)
        {
            DWORD dwErr = GetLastError();
            ((WIN32FILE_IOWIN*)stream) -> error=(int)dwErr;
            ret = -1;
        }
        else
            ret=(long)dwSet;
    }
    return ret;
}

ZPOS64_T ZCALLBACK win32_tell64_file_func (voidpf opaque, voidpf stream)
{
    ZPOS64_T ret= (ZPOS64_T)-1;
    HANDLE hFile = NULL;
    if (stream!=NULL)
        hFile = ((WIN32FILE_IOWIN*)stream)->hf;

    if (hFile)
    {
        LARGE_INTEGER li;
        li.QuadPart = 0;
        li.u.LowPart = SetFilePointer(hFile, li.u.LowPart, &li.u.HighPart, FILE_CURRENT);
        if ( (li.LowPart == 0xFFFFFFFF) && (GetLastError() != NO_ERROR))
        {
            DWORD dwErr = GetLastError();
            ((WIN32FILE_IOWIN*)stream) -> error=(int)dwErr;
            ret = (ZPOS64_T)-1;
        }
        else
            ret=li.QuadPart;
    }
    return ret;
}


long ZCALLBACK win32_seek_file_func (voidpf opaque,voidpf stream,uLong offset,int origin)
{
    DWORD dwMoveMethod=0xFFFFFFFF;
    HANDLE hFile = NULL;

    long ret=-1;
    if (stream!=NULL)
        hFile = ((WIN32FILE_IOWIN*)stream) -> hf;
    switch (origin)
    {
    case ZLIB_FILEFUNC_SEEK_CUR :
        dwMoveMethod = FILE_CURRENT;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        dwMoveMethod = FILE_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        dwMoveMethod = FILE_BEGIN;
        break;
    default: return -1;
    }

    if (hFile != NULL)
    {
        DWORD dwSet = SetFilePointer(hFile, offset, NULL, dwMoveMethod);
        if (dwSet == INVALID_SET_FILE_POINTER)
        {
            DWORD dwErr = GetLastError();
            ((WIN32FILE_IOWIN*)stream) -> error=(int)dwErr;
            ret = -1;
        }
        else
            ret=0;
    }
    return ret;
}

long ZCALLBACK win32_seek64_file_func (voidpf opaque, voidpf stream,ZPOS64_T offset,int origin)
{
    DWORD dwMoveMethod=0xFFFFFFFF;
    HANDLE hFile = NULL;
    long ret=-1;

    if (stream!=NULL)
        hFile = ((WIN32FILE_IOWIN*)stream)->hf;

    switch (origin)
    {
        case ZLIB_FILEFUNC_SEEK_CUR :
            dwMoveMethod = FILE_CURRENT;
            break;
        case ZLIB_FILEFUNC_SEEK_END :
            dwMoveMethod = FILE_END;
            break;
        case ZLIB_FILEFUNC_SEEK_SET :
            dwMoveMethod = FILE_BEGIN;
            break;
        default: return -1;
    }

    if (hFile)
    {
        LARGE_INTEGER* li = (LARGE_INTEGER*)&offset;
        DWORD dwSet = SetFilePointer(hFile, li->u.LowPart, &li->u.HighPart, dwMoveMethod);
        if (dwSet == INVALID_SET_FILE_POINTER)
        {
            DWORD dwErr = GetLastError();
            ((WIN32FILE_IOWIN*)stream) -> error=(int)dwErr;
            ret = -1;
        }
        else
            ret=0;
    }
    return ret;
}

int ZCALLBACK win32_close_file_func (voidpf opaque, voidpf stream)
{
    WIN32FILE_IOWIN* iowin = NULL;
    int ret=-1;

    if (stream==NULL)
        return ret;
    iowin = ((WIN32FILE_IOWIN*)stream);
    if (iowin->filename != NULL)
    {
        free(iowin->filename);
    }
    if (iowin->hf != NULL)
    {
        FlushFileBuffers(iowin->hf);
        CloseHandle(iowin->hf);
        ret=0;
    }
    free(stream);
    return ret;
}

int ZCALLBACK win32_error_file_func (voidpf opaque,voidpf stream)
{
    int ret=-1;
    if (stream==NULL)
        return ret;
    ret = ((WIN32FILE_IOWIN*)stream) -> error;
    return ret;
}

void fill_win32_filefunc (zlib_filefunc_def* pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen_file = win32_open_file_func;
    pzlib_filefunc_def->zopendisk_file = win32_opendisk_file_func;
    pzlib_filefunc_def->zread_file = win32_read_file_func;
    pzlib_filefunc_def->zwrite_file = win32_write_file_func;
    pzlib_filefunc_def->ztell_file = win32_tell_file_func;
    pzlib_filefunc_def->zseek_file = win32_seek_file_func;
    pzlib_filefunc_def->zclose_file = win32_close_file_func;
    pzlib_filefunc_def->zerror_file = win32_error_file_func;
    pzlib_filefunc_def->opaque = NULL;
}

void fill_win32_filefunc64(zlib_filefunc64_def* pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen64_file = win32_open64_file_func;
    pzlib_filefunc_def->zopendisk64_file = win32_opendisk64_file_func;
    pzlib_filefunc_def->zread_file = win32_read_file_func;
    pzlib_filefunc_def->zwrite_file = win32_write_file_func;
    pzlib_filefunc_def->ztell64_file = win32_tell64_file_func;
    pzlib_filefunc_def->zseek64_file = win32_seek64_file_func;
    pzlib_filefunc_def->zclose_file = win32_close_file_func;
    pzlib_filefunc_def->zerror_file = win32_error_file_func;
    pzlib_filefunc_def->opaque = NULL;
}

void fill_win32_filefunc64A(zlib_filefunc64_def* pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen64_file = win32_open64_file_funcA;
    pzlib_filefunc_def->zopendisk64_file = win32_opendisk64_file_funcA;
    pzlib_filefunc_def->zread_file = win32_read_file_func;
    pzlib_filefunc_def->zwrite_file = win32_write_file_func;
    pzlib_filefunc_def->ztell64_file = win32_tell64_file_func;
    pzlib_filefunc_def->zseek64_file = win32_seek64_file_func;
    pzlib_filefunc_def->zclose_file = win32_close_file_func;
    pzlib_filefunc_def->zerror_file = win32_error_file_func;
    pzlib_filefunc_def->opaque = NULL;
}

void fill_win32_filefunc64W(zlib_filefunc64_def* pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen64_file = win32_open64_file_funcW;
    pzlib_filefunc_def->zopendisk64_file = win32_opendisk64_file_funcW;
    pzlib_filefunc_def->zread_file = win32_read_file_func;
    pzlib_filefunc_def->zwrite_file = win32_write_file_func;
    pzlib_filefunc_def->ztell64_file = win32_tell64_file_func;
    pzlib_filefunc_def->zseek64_file = win32_seek64_file_func;
    pzlib_filefunc_def->zclose_file = win32_close_file_func;
    pzlib_filefunc_def->zerror_file = win32_error_file_func;
    pzlib_filefunc_def->opaque = NULL;
}

void fill_zlib_filefunc64_32_def_from_filefunc32(zlib_filefunc64_32_def* p_filefunc64_32,const zlib_filefunc_def* p_filefunc32)
{
    p_filefunc64_32->zfile_func64.zopen64_file = NULL;
    p_filefunc64_32->zfile_func64.zopendisk64_file = NULL;
    p_filefunc64_32->zopen32_file = p_filefunc32->zopen_file;
    p_filefunc64_32->zopendisk32_file = p_filefunc32->zopendisk_file;
    p_filefunc64_32->zfile_func64.zerror_file = p_filefunc32->zerror_file;
    p_filefunc64_32->zfile_func64.zread_file = p_filefunc32->zread_file;
    p_filefunc64_32->zfile_func64.zwrite_file = p_filefunc32->zwrite_file;
    p_filefunc64_32->zfile_func64.ztell64_file = NULL;
    p_filefunc64_32->zfile_func64.zseek64_file = NULL;
    p_filefunc64_32->zfile_func64.zclose_file = p_filefunc32->zclose_file;
    p_filefunc64_32->zfile_func64.zerror_file = p_filefunc32->zerror_file;
    p_filefunc64_32->zfile_func64.opaque = p_filefunc32->opaque;
    p_filefunc64_32->zseek32_file = p_filefunc32->zseek_file;
    p_filefunc64_32->ztell32_file = p_filefunc32->ztell_file;
}

