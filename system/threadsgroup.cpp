#include "sys_includes.h"

#include <syslib\mem.h>
#include <syslib\threadsgroup.h>
#include "threadsgroup.h"

static DWORD WINAPI SafeThreadRoutine(SAFE_THREAD_ROUTINE *lpSafeThreadParams)
{
    DWORD dwResult=0;
    __try {
        dwResult=lpSafeThreadParams->lpRealRoutine(lpSafeThreadParams->lpParam);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {}
    MemFree(lpSafeThreadParams);
    return dwResult;
}

static HANDLE SysCreateThreadSafe(LPSECURITY_ATTRIBUTES lpThreadAttributes,SIZE_T dwStackSize,LPTHREAD_START_ROUTINE lpStartAddress,LPVOID lpParameter,DWORD dwCreationFlags,LPDWORD lpThreadId)
{
    SAFE_THREAD_ROUTINE *lpSafeThreadParams=(SAFE_THREAD_ROUTINE *)MemQuickAlloc(sizeof(SAFE_THREAD_ROUTINE));
    lpSafeThreadParams->lpParam=lpParameter;
    lpSafeThreadParams->lpRealRoutine=lpStartAddress;

    HANDLE hThread=CreateThread(lpThreadAttributes,dwStackSize,(LPTHREAD_START_ROUTINE)SafeThreadRoutine,lpSafeThreadParams,dwCreationFlags,lpThreadId);
    if (!hThread)
        MemFree(lpSafeThreadParams);
    return hThread;
}

SYSLIBFUNC(HANDLE) ThreadsGroup_Create()
{
    THREADS_GROUP *lpGroup=(THREADS_GROUP*)MemAlloc(sizeof(THREADS_GROUP));
    if (lpGroup)
        InitializeCriticalSection(&lpGroup->csGroup);
    return (HANDLE)lpGroup;
}

SYSLIBFUNC(BOOL) ThreadsGroup_CreateThreadEx(HANDLE hGroup,SIZE_T dwStackSize,LPTHREAD_START_ROUTINE lpStartAddress,LPVOID lpParameter,LPDWORD lpThreadId,LPHANDLE lpThreadHandle,DWORD dwFlags)
{
    BOOL bRet=false;
    if (hGroup)
    {
        ThreadsGroup_CloseTerminatedHandles(hGroup);

        THREADS_GROUP *lpGroup=(THREADS_GROUP*)hGroup;
        EnterCriticalSection(&lpGroup->csGroup);
        {
            if (lpStartAddress)
            {
                lpGroup->lphThreads=(PHANDLE)MemRealloc(lpGroup->lphThreads,(lpGroup->dwCount+1)*sizeof(lpGroup->lphThreads[0]));
                if (lpGroup->lphThreads)
                {
                    HANDLE hThread;
                    if (dwFlags & THREADGROUP_SAFETHREAD)
                        hThread=SysCreateThreadSafe(NULL,dwStackSize,lpStartAddress,lpParameter,0,lpThreadId);
                    else
                        hThread=CreateThread(NULL,dwStackSize,lpStartAddress,lpParameter,0,lpThreadId);

                    if (hThread)
                    {
                        lpGroup->lphThreads[lpGroup->dwCount++]=hThread;

                        if (lpThreadHandle)
                            DuplicateHandle(GetCurrentProcess(),hThread,GetCurrentProcess(),lpThreadHandle,0,FALSE,DUPLICATE_SAME_ACCESS);

                        bRet=true;
                    }
                }
            }
        }
        LeaveCriticalSection(&lpGroup->csGroup);
    }
    return bRet;
}

SYSLIBFUNC(BOOL) ThreadsGroup_CreateThread(HANDLE hGroup,SIZE_T dwStackSize,LPTHREAD_START_ROUTINE lpStartAddress,LPVOID lpParameter,LPDWORD lpThreadId,LPHANDLE lpThreadHandle)
{
    return ThreadsGroup_CreateThreadEx(hGroup,dwStackSize,lpStartAddress,lpParameter,lpThreadId,lpThreadHandle,THREADGROUP_SAFETHREAD);
}

SYSLIBFUNC(BOOL) ThreadsGroup_WaitForAllExit(HANDLE hGroup,DWORD dwTimeout)
{
    int iPrevPriority=GetThreadPriority(GetCurrentThread());
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_LOWEST);

    BOOL bRet=false;
    if (hGroup)
    {
        THREADS_GROUP *lpGroup=(THREADS_GROUP*)hGroup;
        if (InterlockedExchangeAdd(&lpGroup->dwCount,0) > 0)
        {
            bool bTimeOut=(dwTimeout != INFINITE);
            while (true)
            {
                DWORD dwObjectsDone=0;
                EnterCriticalSection(&lpGroup->csGroup);
                {
                    for (DWORD i=0; i < lpGroup->dwCount; i++)
                    {
                        if (!lpGroup->lphThreads[i])
                        {
                            dwObjectsDone++;
                            continue;
                        }

                        dwObjectsDone+=(WaitForSingleObject(lpGroup->lphThreads[i],0) == WAIT_OBJECT_0);
                    }
                }
                LeaveCriticalSection(&lpGroup->csGroup);

                if (dwObjectsDone == lpGroup->dwCount)
                {
                    bRet=true;
                    break;
                }

                if (bTimeOut)
                {
                    dwTimeout--;

                    if (!dwTimeout)
                        break;
                }

                Sleep(1);
            }
        }
    }
    SetThreadPriority(GetCurrentThread(),iPrevPriority);
    return bRet;
}

SYSLIBFUNC(void) ThreadsGroup_CloseGroup(HANDLE hGroup)
{
    if (hGroup)
    {
        THREADS_GROUP *lpGroup=(THREADS_GROUP*)hGroup;

        EnterCriticalSection(&lpGroup->csGroup);
        {
            for (DWORD i=0; i < lpGroup->dwCount; i++)
                CloseHandle(lpGroup->lphThreads[i]);
        }
        LeaveCriticalSection(&lpGroup->csGroup);

        DeleteCriticalSection(&lpGroup->csGroup);

        MemFree(lpGroup->lphThreads);
        MemFree(hGroup);
    }
    return;
}

SYSLIBFUNC(void) ThreadsGroup_CloseGroupAndTerminateThreads(HANDLE hGroup)
{
    if (hGroup)
    {
        THREADS_GROUP *lpGroup=(THREADS_GROUP*)hGroup;

        EnterCriticalSection(&lpGroup->csGroup);
        {
            for (DWORD i=0; i < lpGroup->dwCount; i++)
            {
                TerminateThread(lpGroup->lphThreads[i],0xDEAD);
                CloseHandle(lpGroup->lphThreads[i]);
            }
        }
        LeaveCriticalSection(&lpGroup->csGroup);

        DeleteCriticalSection(&lpGroup->csGroup);

        MemFree(lpGroup->lphThreads);
        MemFree(hGroup);
    }
    return;
}

SYSLIBFUNC(void) ThreadsGroup_CloseTerminatedHandles(HANDLE hGroup)
{
    if (hGroup)
    {
        THREADS_GROUP *lpGroup=(THREADS_GROUP*)hGroup;

        EnterCriticalSection(&lpGroup->csGroup);
        {
            for (DWORD i=0; i < lpGroup->dwCount; i++)
            {
                if (!lpGroup->lphThreads[i])
                    continue;

                if (WaitForSingleObject(lpGroup->lphThreads[i],0) != WAIT_OBJECT_0)
                    continue;

                CloseHandle(lpGroup->lphThreads[i]);
                lpGroup->lphThreads[i]=NULL;
            }
        }
        LeaveCriticalSection(&lpGroup->csGroup);
    }
    return;
}

SYSLIBFUNC(DWORD) ThreadsGroup_NumberOfActiveThreads(HANDLE hGroup)
{
    DWORD dwCount=0;
    if (hGroup)
    {
        THREADS_GROUP *lpGroup=(THREADS_GROUP*)hGroup;
        EnterCriticalSection(&lpGroup->csGroup);
        {
            for (DWORD i=0; i < lpGroup->dwCount; i++)
            {
                if ((lpGroup->lphThreads[i] != NULL) && (WaitForSingleObject(lpGroup->lphThreads[i],0) == WAIT_TIMEOUT))
                    dwCount++;
            }
        }
        LeaveCriticalSection(&lpGroup->csGroup);
    }
    return dwCount;
}

