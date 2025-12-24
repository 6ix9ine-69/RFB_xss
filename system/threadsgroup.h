#ifndef THREADGROUP_H_INCLUDED
#define THREADGROUP_H_INCLUDED

struct THREADS_GROUP
{
    DWORD dwCount;
    CRITICAL_SECTION csGroup;
    PHANDLE lphThreads;
};

struct SAFE_THREAD_ROUTINE
{
    LPTHREAD_START_ROUTINE lpRealRoutine;
    LPVOID lpParam;
};

#endif // THREADGROUP_H_INCLUDED
