#ifndef CHECKER_H_INCLUDED
#define CHECKER_H_INCLUDED

#include "list.h"

#include <pshpack1.h>
    typedef struct _MASSCAN
    {
        DWORD dwIP;
        WORD wPort;
    } MASSCAN, *PMASSCAN;
#include <poppack.h>

typedef struct _MASSCAN_ITEM:_COMMON_LIST
{
    bool bInit;
    bool bFreshAdded;
    MASSCAN Address;
} MASSCAN_ITEM, *PMASSCAN_ITEM;

void Checker_NullLastFreeItem();

void Checker_FreeConnection(PCONNECTION lpConnection);
void Checker_FreeAddress(PCONNECTION lpConnection,DWORD dwIP,WORD wPort);
void Checker_RemoveAddress(PCONNECTION lpConnection,DWORD dwIP,WORD wPort);
void Checker_AddAddress(DWORD dwIP,WORD wPort);
bool Checker_GetNextAddress(PCONNECTION lpConnection,PMASSCAN lpItem);
void Checker_Init(cJSON *jsConfig);

void Checker_UpdateLastId(LONGLONG llNewValue);

#define FILE_WATCH_FLAGS FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE

extern volatile DWORD dwCheckersCount,dwCheckersTasks,dwItemsToCheck,dwChecked,dwRFB,dwNotRFB,dwUnavailble,dwCheckerItemsUsed;

extern bool bChecker_NewAddedItemsUsed,bChecker_UpdatePercents;

void Checker_UpdateMenuState(HMENU hMenu,DWORD dwItem);

void Checker_PurgeDB_Begin();
bool Checker_PurgeDB_End();

bool Checker_IsReadingNew();
bool Checker_IsPurging();

#endif // CHECKER_H_INCLUDED
