#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <windows.h>
#include <shlwapi.h>

#include "minizip.h"
#include "minizip\zip.h"
#include "minizip\unzip.h"
#include "compress_wrap.h"
#include "decompress_wrap.h"

#include <syslib\mem.h>
#include <syslib\files.h>
#include <syslib\str.h>

extern "C" void fill_memory_filefunc (zlib_filefunc64_def* pzlib_filefunc_def);

#define INT_BUF_SIZE 5*1024

uLong filetime(const WCHAR *filename, tm_zip *tmzip, uLong *dt);
bool _feof(HANDLE hFile);
int getFileCrc(const WCHAR* filenameinzip,void*buf,unsigned long size_buf,unsigned long* result_crc);

#define HT_COMPRESSOR 1
#define HT_DECOMPRESSOR 2

typedef struct
{
    LPWSTR fileName;
    HANDLE handle;
    int oflags;
} CFDATA;

#define CFF_RECURSE 0x1
#define CFF_DELETE 0x2

void *_alloc(ULONG size);
void change_file_date(WCHAR *filename,uLong dosdate,tm_unz tmu_date);


void ArchSetLastError(int dwError);
int ArchGetLastError();

#define CASESENSITIVITY 0

#endif // COMMON_H_INCLUDED
