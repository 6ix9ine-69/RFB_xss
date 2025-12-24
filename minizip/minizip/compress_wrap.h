#ifndef COMPRESS_WRAP_H_INCLUDED
#define COMPRESS_WRAP_H_INCLUDED

typedef struct
{
    byte bHandleType;
    zipFile hZip;
    int dwCompLevel;

    bool bEncrypted;
    char szPassword[260];

    bool bInMem;
    byte *lpMem;
    int dwSize;
    char *lpComment;
} ZIPCOMPRESSION;

typedef struct
{
    ZIPCOMPRESSION *lpZipData;
    void *zip;
    DWORD filesCount;
    DWORD cabPathOffset;
    bool bDelete;
} CFFSTRUCT;

#endif // COMPRESS_WRAP_H_INCLUDED
