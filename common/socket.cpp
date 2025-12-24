#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

#include <syslib\net.h>
#include <syslib\mem.h>

static PIP_ADAPTER_INFO GetAdaptersInfoEx()
{
    DWORD dwSize=0;
    PIP_ADAPTER_INFO lpAdapters=NULL;

    if (GetAdaptersInfo(lpAdapters,&dwSize) == ERROR_BUFFER_OVERFLOW)
    {
        lpAdapters=(PIP_ADAPTER_INFO)MemQuickAlloc(dwSize);
        if (lpAdapters)
        {
            if (GetAdaptersInfo(lpAdapters,&dwSize) != NO_ERROR)
            {
                MemFree(lpAdapters);
                lpAdapters=NULL;
            }
        }
    }
    return lpAdapters;
}

static PMIB_IFTABLE GetIfTableEx()
{
    DWORD dwSize=0;
    PMIB_IFTABLE lpTable=NULL;
    if (GetIfTable(lpTable,&dwSize,false) == ERROR_INSUFFICIENT_BUFFER)
    {
        lpTable=(PMIB_IFTABLE)MemQuickAlloc(dwSize);
        if (lpTable)
        {
            if (GetIfTable(lpTable,&dwSize,false) != NO_ERROR)
            {
                MemFree(lpTable);
                lpTable=NULL;
            }
        }
    }
    return lpTable;
}

static PMIB_IFROW FindAdapter(PMIB_IFTABLE lpTable,PIP_ADAPTER_INFO lpAdapter)
{
    PMIB_IFROW lpRow=NULL;

    for (DWORD i=0; i < lpTable->dwNumEntries; i++)
    {
        if (memcmp(lpTable->table[i].bPhysAddr,lpAdapter->Address,sizeof(lpAdapter->Address)))
            continue;

        lpRow=&lpTable->table[i];
        break;
    }
    return lpRow;
}

static PIP_ADAPTER_INFO GetActiveNetworkAdapters()
{
    DWORD dwItemsCount=0;
    PIP_ADAPTER_INFO lpList=NULL;

    PMIB_IFTABLE lpTable=GetIfTableEx();
    if (lpTable)
    {
        PIP_ADAPTER_INFO lpAdapters=GetAdaptersInfoEx();
        if (lpAdapters)
        {
            PIP_ADAPTER_INFO lpAdapter=lpAdapters;
            do
            {
                PIP_ADAPTER_INFO lpCurAdapter=lpAdapter;
                lpAdapter=lpAdapter->Next;

                PMIB_IFROW lpRow=FindAdapter(lpTable,lpCurAdapter);
                if (!lpRow)
                    continue;

                if (!lpRow->dwAdminStatus)
                    continue;

                if ((lpRow->dwOperStatus != IF_OPER_STATUS_CONNECTED) && (lpRow->dwOperStatus != IF_OPER_STATUS_OPERATIONAL))
                    continue;

                lpList=(PIP_ADAPTER_INFO)MemRealloc(lpList,(dwItemsCount+1)*sizeof(*lpList));
                if (!lpList)
                    break;

                PIP_ADAPTER_INFO lpCurItem=lpList;
                if (dwItemsCount)
                {
                    lpCurItem=&lpList[dwItemsCount];
                    lpList[dwItemsCount-1].Next=lpCurItem;
                }

                memcpy(lpCurItem,lpCurAdapter,sizeof(*lpCurAdapter));
                lpCurItem->Next=NULL;

                PIP_ADDR_STRING IpAddress=&lpCurAdapter->IpAddressList;
                while (IpAddress)
                    IpAddress=IpAddress->Next;

                dwItemsCount++;
            }
            while (lpAdapter);

            MemFree(lpAdapters);
        }
        MemFree(lpTable);
    }
    return lpList;
}

static DWORD GetFirstIP(DWORD dwAddr,byte bCIDR)
{
    DWORD dwNewIP=0;

    DWORD dwOldIP=ntohl(dwAddr),dwMask=(1 << (32-bCIDR));
    for (int i=0; i < bCIDR; i++)
    {
        dwNewIP+=(dwOldIP & dwMask);
        dwMask<<=1;
    }
    return ntohl(dwNewIP);
}

static DWORD FindInterface(DWORD dwIP)
{
    DWORD dwInterfaceIP=-1;

    PIP_ADAPTER_INFO lpAdapters=GetActiveNetworkAdapters();
    if (lpAdapters)
    {
        PIP_ADAPTER_INFO lpAdapter=lpAdapters;
        while (lpAdapter)
        {
            DWORD dwMaskIP=inet_addr(lpAdapter->IpAddressList.IpMask.String);
            if (dwMaskIP)
            {
                byte bCIDR=0;
                while (dwMaskIP)
                {
                    bCIDR+=(dwMaskIP & 0x01);
                    dwMaskIP>>=1;
                }

                DWORD dwFirstAddr=inet_addr(lpAdapter->IpAddressList.IpAddress.String);

                if (bCIDR != 32)
                    dwFirstAddr=GetFirstIP(dwFirstAddr,bCIDR);

                if (dwFirstAddr == dwIP)
                {
                    dwInterfaceIP=inet_addr(lpAdapter->IpAddressList.IpAddress.String);
                    break;
                }
            }
            lpAdapter=lpAdapter->Next;
        }
        MemFree(lpAdapters);
    }
    return dwInterfaceIP;
}

SOCKET SocketBindOnPort(WORD wPort,DWORD dwIP)
{
    bool bOk=false;

    SOCKET hSock;
    do
    {
        hSock=WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,NULL,NULL);
        if (hSock == INVALID_SOCKET)
            break;

        if (wPort)
        {
            const char cReUse=1;
            setsockopt(hSock,SOL_SOCKET,SO_REUSEADDR,&cReUse,sizeof(cReUse));
        }

        sockaddr_in addr={0};
        addr.sin_family=AF_INET;
        addr.sin_port=htons(wPort);

        if (dwIP)
            addr.sin_addr.s_addr=FindInterface(dwIP);

        if (bind(hSock,(sockaddr *)&addr,sizeof(addr)))
            break;

        bOk=true;
    }
    while (false);

    if (!bOk)
    {
        if (hSock != INVALID_SOCKET)
        {
            closesocket(hSock);
            hSock=INVALID_SOCKET;
        }
    }
    return hSock;
}

