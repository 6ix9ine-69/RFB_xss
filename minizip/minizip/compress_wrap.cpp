#include "common.h"
#include "minizip\iowin32.h"

extern "C" HZIP CreateArchiveW(LPCWSTR lpstrZipFile,LPCSTR lpPassword,int dwPasswordLen,int dwCompLevel)
{
    void *r=NULL;
    if (lpstrZipFile)
    {
        ZIPCOMPRESSION *p=(ZIPCOMPRESSION *)_alloc(sizeof(ZIPCOMPRESSION));
        if (p)
        {
            p->bHandleType=HT_COMPRESSOR;

            bool bPassword=((lpPassword) && (dwPasswordLen > 0));
            if (bPassword)
            {
                memcpy(p->szPassword,lpPassword,dwPasswordLen);
                p->bEncrypted=true;
            }

            p->dwCompLevel=dwCompLevel;

            zlib_filefunc64_def ffunc;
            fill_win32_filefunc64W(&ffunc);
            p->hZip=zipOpen2_64(lpstrZipFile,0,NULL,&ffunc);
            if (p->hZip)
                r=p;
        }
        if (!r)
            MemFree(p);
    }
    else
        ArchSetLastError(ARCH_INVALID_PARAMETER);
    return r;
}

extern "C" HZIP CreateArchiveA(LPCSTR lpstrZipFile,LPCSTR lpPassword,int dwPasswordLen,int dwCompLevel)
{
    HZIP hZip=0;
    if (lpstrZipFile)
    {
        LPWSTR pwstrPath=StrAnsiToUnicodeEx(lpstrZipFile,0,NULL);
        hZip=CreateArchiveW(pwstrPath,lpPassword,dwPasswordLen,dwCompLevel);
        MemFree(pwstrPath);
    }
    else
        ArchSetLastError(ARCH_INVALID_PARAMETER);
    return hZip;
}

bool ArchAddFileEx(HZIP hZip,LPCWSTR pstrSourceFile,LPCWSTR pstrDestFile,DWORD flags)
{
    bool r=false;
    if ((hZip) && (((ZIPCOMPRESSION *)hZip)->bHandleType == HT_COMPRESSOR) && (pstrSourceFile) && (pstrDestFile))
    {
        int lDest,lSrc;

        if (((lDest=lstrlenW(pstrDestFile)) < MAX_PATH-1) && ((lSrc=lstrlenW(pstrSourceFile)) < MAX_PATH-1))
        {
            ZIPCOMPRESSION *p=(ZIPCOMPRESSION *)hZip;
            LPSTR file=StrUnicodeToOemEx(pstrDestFile,lDest,NULL);

            if (file)
            {
                void *buf=_alloc(INT_BUF_SIZE);
                if (buf)
                {
                    p->bInMem=false;
                    zip_fileinfo zi={0};
                    filetime(pstrSourceFile,&zi.tmz_date,&zi.dosDate);
                    LPCSTR lpPassword=NULL;
                    unsigned long crcFile=0;
                    if ((p->bEncrypted) && (!(flags & ZIP_NO_CRYPT)))
                    {
                        getFileCrc(pstrSourceFile,buf,INT_BUF_SIZE,&crcFile);
                        lpPassword=p->szPassword;
                    }
                    int err=zipOpenNewFileInZip3_64(p->hZip,file,&zi,NULL,0,NULL,0,NULL,(p->dwCompLevel>0) ? Z_DEFLATED:0,p->dwCompLevel,0,-MAX_WBITS,DEF_MEM_LEVEL,Z_DEFAULT_STRATEGY,lpPassword,crcFile,1);
                    if (err == ZIP_OK)
                    {
                        HANDLE fin=CreateFileW(pstrSourceFile,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
                        if (fin != INVALID_HANDLE_VALUE)
                        {
                            unsigned long size_read = 0;
                            do
                            {
                                err=ZIP_OK;
                                ReadFile(fin,buf,INT_BUF_SIZE,&size_read,0);

                                if (size_read < INT_BUF_SIZE)
                                {
                                    if (!_feof(fin))
                                        err=ZIP_ERRNO;
                                }

                                if (size_read>0)
                                    err=zipWriteInFileInZip(p->hZip,buf,size_read);
                            }
                            while ((err == ZIP_OK) && (size_read>0));
                            CloseHandle(fin);

                            if (err<0)
                                err=ZIP_ERRNO;
                            else
                            {
                                err=zipCloseFileInZip(p->hZip);
                                r=true;
                            }
                        }
                    }
                    MemFree(buf);
                }
            }

            MemFree(file);
        }
    }
    else
        ArchSetLastError(ARCH_INVALID_PARAMETER);
    return r;
}

extern "C" bool ArchAddFileW(HZIP hZip,LPCWSTR pstrSourceFile,LPCWSTR pstrDestFile)
{
    return ArchAddFileEx(hZip,pstrSourceFile,pstrDestFile,0);
}

extern "C" bool ArchAddFileA(HZIP hZip,LPCSTR pstrSourceFile,LPCSTR pstrDestFile)
{
    bool bRet=false;
    if ((hZip) && (((ZIPCOMPRESSION *)hZip)->bHandleType == HT_COMPRESSOR) && (pstrSourceFile) && (pstrDestFile))
    {
        LPWSTR pwstrSourceFile=StrAnsiToUnicodeEx(pstrSourceFile,0,NULL),
               pwstrDestFile=StrAnsiToUnicodeEx(pstrDestFile,0,NULL);
        bRet=ArchAddFileW(hZip,pwstrSourceFile,pwstrDestFile);
        MemFree(pwstrSourceFile);
        MemFree(pwstrDestFile);
    }
    else
        ArchSetLastError(ARCH_INVALID_PARAMETER);
    return bRet;
}

extern "C" void ArchClose(HZIP hZip)
{
    if (!hZip)
    {
        ArchSetLastError(ARCH_INVALID_PARAMETER);
        return;
    }

    ZIPCOMPRESSION *p=(ZIPCOMPRESSION *)hZip;
    if (p->bHandleType == HT_COMPRESSOR)
    {
        zipClose(p->hZip,p->lpComment);
        MemFree(p->lpComment);
    }
    else
    {
        ZIPDECOMPRESSION *decomp=(ZIPDECOMPRESSION*)p;
        if (decomp->bInMem)
            VirtualFree(decomp->lpFileMem,0,MEM_RELEASE);
        unzClose(p->hZip);
    }
    MemFree(p);
    return;
}

extern "C" void ArchSetComment(HZIP hZip,char *lpComment)
{
    if ((!hZip) || ((hZip) && (((ZIPCOMPRESSION *)hZip)->bHandleType != HT_COMPRESSOR)))
    {
        ArchSetLastError(ARCH_INVALID_PARAMETER);
        return;
    }

    ZIPCOMPRESSION *p=(ZIPCOMPRESSION *)hZip;
    MemFree(p->lpComment);
    p->lpComment=StrDuplicateA(lpComment,0);
    return;
}

extern "C" bool ArchGetComment(HZIP hZip,char *lpComment,DWORD dwCommentSize)
{
    if ((!hZip) || (!lpComment) || (!dwCommentSize))
    {
        ArchSetLastError(ARCH_INVALID_PARAMETER);
        return false;
    }

    bool bRet=false;
    ZIPCOMPRESSION *p=(ZIPCOMPRESSION *)hZip;
    if (p->bHandleType == HT_COMPRESSOR)
    {
        if (p->lpComment)
        {
            lstrcpynA(lpComment,p->lpComment,dwCommentSize);
            bRet=true;
        }
    }
    else
    {
        ZIPDECOMPRESSION *decomp=(ZIPDECOMPRESSION*)p;
        bRet=(unzGetGlobalComment(decomp->hZip,lpComment,dwCommentSize) == UNZ_OK);
    }
    return bRet;
}

bool ArchCompressMemoryEx(HZIP hZip,void *lpMem,int dwSize,LPCWSTR pstrFile,DWORD flags)
{
    bool r=false;
    if ((hZip) && (((ZIPCOMPRESSION *)hZip)->bHandleType == HT_COMPRESSOR) && (lpMem) && (dwSize) && (pstrFile))
    {
        ZIPCOMPRESSION *p=(ZIPCOMPRESSION *)hZip;
        LPCSTR file=StrUnicodeToOemEx(pstrFile,0,NULL);
        if (file)
        {
            p->bInMem=true;
            p->lpMem=(byte*)lpMem;
            p->dwSize=dwSize;
            zip_fileinfo zi={0};
            ///filetime(pstrSourceFile,&zi.tmz_date,&zi.dosDate);
            LPCSTR lpPassword=NULL;
            unsigned long crcFile=0;
            if ((p->bEncrypted) && (!(flags & ZIP_NO_CRYPT)))
            {
                crcFile=crc32(0,(byte*)lpMem,dwSize);
                lpPassword=p->szPassword;
            }
            int err=zipOpenNewFileInZip3_64(p->hZip,file,&zi,NULL,0,NULL,0,NULL,(p->dwCompLevel>0) ? Z_DEFLATED:0,p->dwCompLevel,0,-MAX_WBITS,DEF_MEM_LEVEL,Z_DEFAULT_STRATEGY,lpPassword,crcFile,1);
            if (err == ZIP_OK)
            {
                if (zipWriteInFileInZip(p->hZip,lpMem,dwSize) == ZIP_OK)
                    r=true;
                err=zipCloseFileInZip(p->hZip);
            }
        }
    }
    else
        ArchSetLastError(ARCH_INVALID_PARAMETER);
    return r;
}

extern "C" bool ArchCompressMemoryW(HZIP hZip,void *lpMem,int dwSize,LPCWSTR pstrFile)
{
    return ArchCompressMemoryEx(hZip,lpMem,dwSize,pstrFile,0);
}

extern "C" bool ArchCompressMemoryA(HZIP hZip,void *lpMem,int dwSize,LPCSTR pstrFile)
{
    bool bRet=false;
    if ((hZip) && (((ZIPCOMPRESSION *)hZip)->bHandleType == HT_COMPRESSOR) && (lpMem) && (dwSize) && (pstrFile))
    {
        LPWSTR pwstrFile=StrAnsiToUnicodeEx(pstrFile,0,NULL);

        bRet=ArchCompressMemoryW(hZip,lpMem,dwSize,pwstrFile);
        MemFree(pwstrFile);
    }
    else
        ArchSetLastError(ARCH_INVALID_PARAMETER);
    return bRet;
}

static bool _PathCombine(LPWSTR dest,const LPWSTR dir,const LPWSTR file)
{
    LPWSTR p=(LPWSTR)file;
    if (p)
    {
        while ((*p == '\\') || (*p == '/'))
            p++;
    }
    return (!PathCombineW(dest,dir,p)) ? false:true;
}

static bool CreateFromFolderProc(LPWSTR path,FILE_INFOW *fileInfo,void *data)
{
    WCHAR filePath[MAX_PATH];
    if (_PathCombine(filePath,path,(LPWSTR)fileInfo->wfd.cFileName))
    {
        HANDLE hFile=CreateFileW(filePath,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hFile);
            CFFSTRUCT *cs=(CFFSTRUCT *)data;
            if (ArchAddFileW(cs->lpZipData,filePath,filePath+cs->cabPathOffset))
            {
                cs->filesCount++;
                if (cs->bDelete)
                    RemoveFileW(filePath);
            }
        }
    }
    return true;
}

extern "C" bool ArchiveFolderW(HZIP hZip,LPCWSTR sourceFolder,LPCWSTR *fileMask,DWORD fileMaskCount,DWORD flags)
{
    bool r=false;
    if ((hZip) && (((ZIPCOMPRESSION *)hZip)->bHandleType == HT_COMPRESSOR) && (sourceFolder) && (fileMask) && (fileMaskCount))
    {
        CFFSTRUCT cs;
        cs.lpZipData=(ZIPCOMPRESSION *)hZip;
        cs.zip=((ZIPCOMPRESSION *)hZip)->hZip;
        if (cs.zip)
        {
            cs.filesCount=0;
            cs.cabPathOffset=lstrlenW(sourceFolder);

            if ((cs.cabPathOffset > 0) && (sourceFolder[cs.cabPathOffset-1] != '\\'))
                cs.cabPathOffset++;

            cs.bDelete=(flags & CFF_DELETE ? true : false);
            FindFilesW(sourceFolder,(LPCWSTR*)fileMask,fileMaskCount,(flags & CFF_RECURSE ? FFF_RECURSIVE : 0) | FFF_SEARCH_FILES,(FINDFILEPROCW*) CreateFromFolderProc,&cs,0,0);

            if ((cs.filesCount > 0))
                r=true;
        }
    }
    else
        ArchSetLastError(ARCH_INVALID_PARAMETER);
    return r;
}

extern "C" bool ArchiveFolderA(HZIP hZip,LPCSTR sourceFolder,LPCSTR *fileMask,DWORD fileMaskCount,DWORD flags)
{
    bool bRet=false;
    if ((hZip) && (((ZIPCOMPRESSION *)hZip)->bHandleType == HT_COMPRESSOR) && (sourceFolder) && (fileMask) && (fileMaskCount))
    {
        LPWSTR wsourceFolder=StrAnsiToUnicodeEx(sourceFolder,0,NULL),
               *wfileMask=(LPWSTR*)_alloc(fileMaskCount*sizeof(LPWSTR));

        for (DWORD i=0; i<fileMaskCount; i++)
            wfileMask[i]=StrAnsiToUnicodeEx(fileMask[i],0,NULL);

        bool bRet=ArchiveFolderW(hZip,wsourceFolder,(LPCWSTR*)wfileMask,fileMaskCount,flags);

        MemFree(wsourceFolder);
        for (DWORD i=0; i<fileMaskCount; i++)
            MemFree(wfileMask[i]);
        MemFree (wfileMask);
    }
    else
        ArchSetLastError(ARCH_INVALID_PARAMETER);
    return bRet;
}

extern "C" bool ArchCreateFromFolderW(LPCWSTR lpstrZipFile,LPCWSTR sourceFolder,LPCWSTR *fileMask,DWORD fileMaskCount,DWORD flags,LPCSTR lpPassword,int dwPasswordLen,int dwCompLevel)
{
    HZIP hZip=CreateArchiveW(lpstrZipFile,lpPassword,dwPasswordLen,dwCompLevel);
    bool r=false;
    if (hZip)
    {
        r=ArchiveFolderW(hZip,sourceFolder,fileMask,fileMaskCount,flags);
        ArchClose(hZip);
    }
    return r;
}

extern "C" bool ArchCreateFromFolderA(LPCSTR lpstrZipFile,LPCSTR sourceFolder,LPCSTR *fileMask,DWORD fileMaskCount,DWORD flags,LPCSTR lpPassword,int dwPasswordLen,int dwCompLevel)
{
    HZIP hZip=CreateArchiveA(lpstrZipFile,lpPassword,dwPasswordLen,dwCompLevel);
    bool r=false;
    if (hZip)
    {
        r=ArchiveFolderA(hZip,sourceFolder,fileMask,fileMaskCount,flags);
        ArchClose(hZip);
    }
    return r;
}

