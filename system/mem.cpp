#include "sys_includes.h"

#include "mem.h"
#include <syslib\mem.h>

DWORD Align(DWORD Size, DWORD Alignment);

static HANDLE hHeap;
static DWORD dwInit;

static bool IsInit()
{
    return (dwInit == GetCurrentProcessId());
}

static bool CheckBlock(LPCVOID lpMem)
{
	bool bRet=false;
	if (lpMem)
    {
        LPBYTE p=(LPBYTE)lpMem-sizeof(DWORD)*2;
        if (HeapValidate(hHeap,0,p))
        {
            __try
            {
                DWORD dwSize=*(LPDWORD)p,
                      *b=(LPDWORD)&p[sizeof(DWORD)],
                      *e=(LPDWORD)&p[sizeof(DWORD)*2+dwSize];

                if ((*b == BLOCK_ALLOCED) && (*e == BLOCK_ALLOCED))
                    bRet=true;
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
	return bRet;
}

static bool IsHeap(HANDLE hHeap)
{
    bool bRet=false;

    HANDLE hHeaps[256];
    DWORD dwCount=GetProcessHeaps(255,hHeaps);
    if ((dwCount) && (dwCount < 256))
    {
        for (DWORD i=0; i < dwCount; i++)
        {
            if (hHeaps[i] == hHeap)
            {
                bRet=true;
                break;
            }
        }
    }
    return bRet;
}

static void MemInit()
{
    if ((!IsInit()) || (!IsHeap(hHeap)))
    {
        hHeap=HeapCreate(0,0,0);
        if (hHeap)
        {
            DWORD dwHeapInfo=HEAP_LFH;
            HeapSetInformation(hHeap,(HEAP_INFORMATION_CLASS)HeapCompatibilityInformation,&dwHeapInfo,sizeof(dwHeapInfo));
            dwInit=GetCurrentProcessId();
        }
    }
    return;
}

static LPVOID InitMemBlock(LPBYTE lpMem,size_t dwSize)
{
    *(LPDWORD)lpMem=(DWORD)dwSize;
    *(LPDWORD)&lpMem[sizeof(DWORD)]=BLOCK_ALLOCED;
    *(LPDWORD)&lpMem[dwSize+sizeof(DWORD)*2]=BLOCK_ALLOCED;
    return (LPVOID)(lpMem+sizeof(DWORD)*2);
}

static LPVOID MemAllocEx(size_t dwSize,DWORD dwFlags)
{
    DWORD dwGLE=GetLastError();

    if (!IsInit())
        MemInit();

    LPVOID lpMem=NULL;
	if (dwSize)
	{
	    DWORD dwRealSize=Align(dwSize+MEM_SAFE_BYTES,sizeof(DWORD_PTR));
		LPBYTE lpTmp=(LPBYTE)HeapAlloc(hHeap,dwFlags,dwRealSize+sizeof(DWORD)*3);
		if (lpTmp)
        {
            lpMem=InitMemBlock(lpTmp,dwRealSize);

            if (!(dwFlags & HEAP_ZERO_MEMORY))
                memset(&((LPBYTE)lpMem)[dwSize],0,dwRealSize-dwSize);
        }
    }

    if (lpMem)
        SetLastError(dwGLE);
    return lpMem;
}

SYSLIBFUNC(LPVOID) MemAlloc(size_t dwSize)
{
    return MemAllocEx(dwSize,HEAP_ZERO_MEMORY);
}

SYSLIBFUNC(LPVOID) MemQuickAlloc(size_t dwSize)
{
    return MemAllocEx(dwSize,0);
}

static size_t MemGetBlockSize(LPVOID lpMem)
{
    LPBYTE p=(LPBYTE)lpMem-sizeof(DWORD)*2;
    return *(LPDWORD)p;
}

SYSLIBFUNC(size_t) MemGetSize(LPVOID lpMem)
{
    DWORD dwGLE=GetLastError();

    if (!IsInit())
        MemInit();

    size_t dwSize=MemGetBlockSize(lpMem);

    SetLastError(dwGLE);
    return dwSize;
}

SYSLIBFUNC(void) MemFree(LPVOID lpMem)
{
    if (!IsInit())
        return;

    DWORD dwGLE=GetLastError();

    if (CheckBlock(lpMem))
    {
        LPBYTE lpTmp=(LPBYTE)lpMem-sizeof(DWORD)*2;
        DWORD dwSize=*(LPDWORD)lpTmp;
        *(LPDWORD)&lpTmp[sizeof(DWORD)]=BLOCK_FREED;
        *(LPDWORD)&lpTmp[dwSize+sizeof(DWORD)*2]=BLOCK_FREED;
        HeapFree(hHeap,0,lpTmp);
    }

    SetLastError(dwGLE);
    return;
}

SYSLIBFUNC(LPVOID) MemRealloc(LPVOID lpMem,size_t dwSize)
{
    DWORD dwGLE=GetLastError();

    if (!IsInit())
        MemInit();

    LPVOID lpNewMem=NULL;
    do
    {
        if (!dwSize)
            break;

        dwSize=Align(dwSize+MEM_SAFE_BYTES,sizeof(DWORD_PTR));

        LPBYTE lpTmp=NULL;
        if (lpMem)
        {
            if (!CheckBlock(lpMem))
                break;

            if (dwSize <= MemGetBlockSize(lpMem))
            {
                lpNewMem=lpMem;
                break;
            }

            lpTmp=(LPBYTE)lpMem-sizeof(DWORD)*2;
        }

        if (lpTmp)
        {
            LPBYTE lpTmpNew=(LPBYTE)HeapReAlloc(hHeap,HEAP_ZERO_MEMORY,lpTmp,dwSize+sizeof(DWORD)*3);

            if (!lpTmpNew)
                MemFree(lpTmp);

            lpTmp=lpTmpNew;
        }
        else
            lpTmp=(LPBYTE)HeapAlloc(hHeap,HEAP_ZERO_MEMORY,dwSize+sizeof(DWORD)*3);

        if (lpTmp)
            lpNewMem=InitMemBlock(lpTmp,dwSize);
    }
    while (false);

    SetLastError(dwGLE);
    return lpNewMem;
}

SYSLIBFUNC(void) MemZeroAndFree(LPVOID lpMem)
{
    if (!IsInit())
        return;

    DWORD dwGLE=GetLastError();

    if (CheckBlock(lpMem))
    {
        DWORD dwSize=*(LPDWORD)((LPBYTE)lpMem-sizeof(DWORD)*2);
        memset(lpMem,0,dwSize);
        MemFree(lpMem);
    }

    SetLastError(dwGLE);
    return;
}

SYSLIBFUNC(LPVOID) MemCopyEx(LPCVOID lpMem,size_t dwSize)
{
    LPVOID lpNewMem=NULL;
    do
    {
        lpNewMem=MemQuickAlloc(dwSize);
        if (!lpNewMem)
            break;

        __try {
            memcpy(lpNewMem,lpMem,dwSize);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            MemFree(lpNewMem);
            lpNewMem=NULL;
        }
    }
    while (false);
    return lpNewMem;
}

SYSLIBFUNC(void) MemFreeArrayOfPointers(LPVOID *lppMem,DWORD dwCount)
{
    do
    {
        if (!dwCount)
            break;

        for (DWORD i=0; i < dwCount; i++)
            MemFree(lppMem[i]);

        MemFree(lppMem);
    }
    while (false);
    return;
}

static int MemCmp(LPCVOID lpMem1,LPCVOID lpMem2,SIZE_T dwSize)
{
    int iRet=0;
    register BYTE lpM1,lpM2;
    for (register SIZE_T i = 0; i < dwSize; i++)
    {
        lpM1=((LPBYTE)lpMem1)[i];
        lpM2=((LPBYTE)lpMem2)[i];

        if (lpM1 != lpM2)
        {
            iRet=(int)(lpM1-lpM2);
            break;
        }
    }
    return iRet;
}

SYSLIBFUNC(LPVOID) MemMem(LPVOID lpMem,SIZE_T dwMemSize,LPCVOID lpData,SIZE_T dwDataSize)
{
    LPVOID lpPtr=NULL;
    if (dwMemSize >= dwDataSize)
    {
        dwMemSize-=dwDataSize;

        for (register SIZE_T i=0; i <= dwMemSize; i++)
        {
            register LPBYTE p=(LPBYTE)lpMem+i;
            if (MemCmp(p,lpData,dwDataSize))
                continue;

            lpPtr=(LPVOID)p;
            break;
        }
    }
    return lpPtr;
}

SYSLIBFUNC(LPVOID) MemStrA(LPVOID lpMem,SIZE_T dwMemSize,LPCSTR lpData)
{
    return MemMem(lpMem,dwMemSize,(LPCVOID)lpData,lstrlenA(lpData));
}

