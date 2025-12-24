#include <windows.h>
#include <syslib\mem.h>

#include "adapters.h"

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

PIP_ADAPTER_INFO Network_GetActiveNetworkAdapters(LPDWORD lpdwIPsCount)
{
    DWORD dwItemsCount=0,dwIPsCount=0;
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
                {
                    IpAddress=IpAddress->Next;
                    dwIPsCount++;
                }

                dwItemsCount++;
            }
            while (lpAdapter);

            MemFree(lpAdapters);
        }
        MemFree(lpTable);
    }

    if (lpdwIPsCount)
        *lpdwIPsCount=dwIPsCount;

    return lpList;
}

