#ifndef IPLIST_H_INCLUDED
#define IPLIST_H_INCLUDED

extern SYSTEM_INFO siInfo;

#define GET_MAPPING_SIZE() siInfo.dwAllocationGranularity*16

void IPList_Parse(LPCTSTR lpFileName);
bool IPList_IsReading();

#endif // IPLIST_H_INCLUDED
