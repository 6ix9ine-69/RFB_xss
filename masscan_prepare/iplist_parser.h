#ifndef IPLIST_PARSER_H_INCLUDED
#define IPLIST_PARSER_H_INCLUDED

#define MAX_IP_LIST_ITEMS 10000

#pragma pack(push,1)
typedef struct _IP_LIST_ITEM
{
    u_long S_addr;
    short sin_port;
} IP_LIST_ITEM, *PIP_LIST_ITEM;
#pragma pack(pop)

bool IPList_Parse(LPCWSTR lpFileName);

bool IPList_GetNextIP_Initial(PIP_LIST_ITEM lpItem);
bool IPList_GetNextIP(PIP_LIST_ITEM lpItem);

void IPList_ReInit();

#endif // IPLIST_PARSER_H_INCLUDED
