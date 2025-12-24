#include "common.h"

// TODO (Гость#1#): сделать ArchLastError потоко-независимой
static int ArchLastError=0;

void ArchSetLastError(int dwError)
{
    ArchLastError=dwError;
    return;
}

extern "C" int ArchGetLastError()
{
    return ArchLastError;
}

void *_alloc(ULONG size)
{
    void *r=MemAlloc(size);
    if (!r)
        ArchSetLastError(ARCH_NO_MEM);
    return r;
}

bool _feof(HANDLE hFile)
{
    DWORD lowSize=GetFileSize(hFile,0);
    DWORD lowPos=SetFilePointer(hFile,0,0,FILE_CURRENT);
    bool r=true;
    if (lowPos!=lowSize)
        r=false;
    return r;
}

uLong filetime(const WCHAR *filename,tm_zip *tmzip, uLong *dt)
{
    int ret=0;
    {
        FILETIME ftLocal;
        HANDLE hFind;
        WIN32_FIND_DATAW ff32;

        hFind=FindFirstFileW(filename,&ff32);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            FileTimeToLocalFileTime(&(ff32.ftLastWriteTime),&ftLocal);
            FileTimeToDosDateTime(&ftLocal,((LPWORD)dt)+1,((LPWORD)dt)+0);
            FindClose(hFind);
            ret=1;
        }
    }
    return ret;
}

void change_file_date(WCHAR *filename,uLong dosdate,tm_unz tmu_date)
{
    FILETIME ftm,ftLocal,ftCreate,ftLastAcc,ftLastWrite;

    HANDLE hFile=CreateFileW(filename,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
    GetFileTime(hFile,&ftCreate,&ftLastAcc,&ftLastWrite);
    DosDateTimeToFileTime((WORD)(dosdate>>16),(WORD)dosdate,&ftLocal);
    LocalFileTimeToFileTime(&ftLocal,&ftm);
    SetFileTime(hFile,&ftm,&ftLastAcc,&ftm);
    CloseHandle(hFile);
    return;
}

int getFileCrc(const WCHAR* filenameinzip,void*buf,unsigned long size_buf,unsigned long* result_crc)
{
    unsigned long calculate_crc=0;
    int err=ZIP_OK;
    HANDLE fin=CreateFileW(filenameinzip,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
    unsigned long size_read=0;
    unsigned long total_read=0;
    if (!fin)
        err=ZIP_ERRNO;
    if (err == ZIP_OK)
    {
        do
        {
            err=ZIP_OK;
            ReadFile(fin,buf,size_buf,&size_read,0);

            if (size_read < size_buf)
            {
                if (!_feof(fin))
                    err=ZIP_ERRNO;
            }

            if (size_read>0)
                calculate_crc=crc32(calculate_crc,(byte*)buf,size_read);
            total_read += size_read;
        }
        while ((err == ZIP_OK) && (size_read>0));
    }

    if (fin)
        CloseHandle(fin);

    *result_crc=calculate_crc;
    return err;
}
