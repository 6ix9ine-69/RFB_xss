#include "sys_includes.h"

SYSLIBFUNC(UINT) NetResolveAddress(LPCSTR lpHost)
{
    UINT dwAddr=inet_addr(lpHost);
    if (dwAddr == INADDR_NONE)
    {
        hostent *hp;
        if (hp=gethostbyname(lpHost))
            dwAddr=*(unsigned long *)hp->h_addr;
    }
    return dwAddr;
}

SYSLIBFUNC(LPCSTR) NetNtoA(int iAddr)
{
    in_addr addr;
    addr.s_addr=iAddr;
    return inet_ntoa(addr);
}

SYSLIBFUNC(void) NetCloseSocket(SOCKET hSock)
{
    shutdown(hSock,SD_BOTH);
    closesocket(hSock);
    return;
}

