#ifndef ADAPTERS_H_INCLUDED
#define ADAPTERS_H_INCLUDED

#include <iphlpapi.h>

PIP_ADAPTER_INFO Network_GetActiveNetworkAdapters(LPDWORD lpdwIPsCount);

typedef struct _IPS
{
    ULONG ulSubNet;
    ULONG ulStart;
    ULONG ulEnd;
    ULONG ulCount;
} IPS, *PIPS;

#endif // ADAPTERS_H_INCLUDED
