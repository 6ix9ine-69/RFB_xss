#include <windows.h>

#include "syslib\mem.h"

SYSTEM_INFO siInfo;

void IPList_Parse(LPCWSTR lpTxtFileName,LPCWSTR lpDatFileName);

#include <pshpack1.h>
    typedef struct _MASSCAN
    {
        DWORD dwIP;
        WORD wPort;
    } MASSCAN, *PMASSCAN;
#include <poppack.h>

void swap(char* a, char* b, size_t size)
{
    if (a != b)
    {
        char tmp, *p1 = a, *p2 = b;
        for (int i = 0; i < size; i++, p1++, p2++)
        {
            tmp = *p1;
            *p1 = *p2;
            *p2 = tmp;
        }
    }
}

char* median_of_three(char* a, char* b, char* c, int (*cmp)(const void*, const void*))
{
    if (cmp(a, b) < 0)
    {
        if (cmp(b, c) < 0)
            return b;
        else if (cmp(a, c) < 0)
            return c;
        else
            return a;
    }
    else
    {
        if (cmp(a, c) < 0)
            return a;
        else if (cmp(b, c) < 0)
            return c;
        else
            return b;
    }
}

void quicksort(char* array, int left, int right, size_t size, int (*cmp)(const void*, const void*))
{
    if (left >= right)
        return;

    if (right - left == 1)
    {
        if (cmp(array + left * size, array + right * size) > 0)
            swap(array + left * size, array + right * size, size);
        return;
    }

    char* pivot = median_of_three(array + left * size, array + (left + right) / 2 * size, array + right * size, cmp);
    int i = left, j = right;

    while (i <= j)
    {
        while (cmp(array + i * size, pivot) < 0)
            i++;

        while (cmp(array + j * size, pivot) > 0)
            j--;

        if (i <= j)
        {
            swap(array + i * size, array + j * size, size);
            i++;
            j--;
        }
    }

    if (left < j)
        quicksort(array, left, j, size, cmp);

    if (i < right)
        quicksort(array, i, right, size, cmp);
}

int cmp(const void* a, const void* b)
{
    return memcmp(a, b, sizeof(MASSCAN));
}

static void Masscan_Rescan(LPCWSTR lpFileName)
{
    HANDLE hFile=CreateFileW(lpFileName,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER liFileSize={0};
        GetFileSizeEx(hFile,&liFileSize);

        HANDLE hFileMapping=CreateFileMapping(hFile,NULL,PAGE_READWRITE,0,0,NULL);
        if (hFileMapping)
        {
            PMASSCAN elems=(PMASSCAN)MapViewOfFile(hFileMapping,FILE_MAP_READ|FILE_MAP_WRITE,0,0,0);
            if (elems)
            {
                size_t num_elems=liFileSize.QuadPart/sizeof(*elems);
                quicksort((PCHAR)elems,0,num_elems-1,sizeof(*elems),cmp);

                UnmapViewOfFile(elems);
            }
            CloseHandle(hFileMapping);
        }

        CloseHandle(hFile);
    }
    return;
}

void main()
{
/// massscan_prepare -p masscan.txt masscan.dat

    int argc=0;
    LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if ((argc == 4) && (!lstrcmpiW(L"-p",argv[1])))
    {
        HANDLE hFile=CreateFileW(argv[2],GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD dwTmp;
            WriteFile(hFile,"\n",1,&dwTmp,NULL);

            LARGE_INTEGER liSize={0};
            GetFileSizeEx(hFile,&liSize);
            liSize.QuadPart-=3;

            SetFilePointerEx(hFile,liSize,NULL,FILE_BEGIN);
            SetEndOfFile(hFile);

            CloseHandle(hFile);
        }

        GetSystemInfo(&siInfo);
        IPList_Parse(argv[2],argv[3]);
    }
    else if ((argc == 3) && (!lstrcmpiW(L"-s",argv[1])))
        Masscan_Rescan(argv[2]);

    ExitProcess(0);
}
