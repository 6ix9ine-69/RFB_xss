#ifndef DB_H_INCLUDED
#define DB_H_INCLUDED

int sqlite3_step_ex(sqlite3_stmt *hStmt);

extern sqlite3 *hWorkerDB,*hCheckerDB;

#define DB_LogErr(lpConnection,hDB,dwLevel,lpFunc) DebugLog_AddItem2(lpConnection,dwLevel,"%s() failed, %d: \"%s\"",lpFunc,iErr,sqlite3_errmsg(hDB))
#define DB_Exec(hDB,lpSql) {\
                                int iErr=sqlite3_exec(hDB,lpSql,NULL,NULL,NULL);\
                                if (iErr != SQLITE_OK)\
                                    DB_LogErr(NULL,hDB,LOG_LEVEL_ERROR,"sqlite3_exec");\
                            }

typedef struct _DB_TRANS
{
    sqlite3 *hDB;
    CRITICAL_SECTION csTrans;
    DWORD dwRef;
} DB_TRANS, *PDB_TRANS;

sqlite3_stmt *DB_Masscan_Append_Begin();
bool DB_Masscan_Append(sqlite3_stmt *hStmt,DWORD dwIP,WORD wPort);
void DB_Masscan_Append_End(sqlite3_stmt *hStmt);
DWORD DB_Masscan_CalcItemsToCheck();
LONGLONG DB_Masscan_GetLastItemId();
bool DB_Masscan_Purge();

bool DB_Worker_IsGoodAddressPresent(LPCSTR lpAddress,LPDWORD lpdwIdx);
void DB_Worker_MoveToIgnored(LPCSTR lpAddress);
bool DB_Worker_IsIgnored(LPCSTR lpDesk);
void DB_Worker_AddIgnoreFilter(LPCSTR lpFilter);
void DB_Worker_Rescan(LPCSTR lpAddress,DWORD dwIdx);
void DB_Worker_ChangeComment(LPCSTR lpAddress,LPCSTR lpComment);
void DB_Worker_Read();
DWORD DB_Worker_RescanUnsupported(PRFB_SERVER *lppUnsupportedItems,PRFB_SERVER *lppLastUnsupportedItem);

bool DB_TotalIPs_IsPresent(DWORD dwIP,WORD wPort);
void DB_TotalIPs_Append(DWORD dwIP,WORD wPort);

bool DB_Init(cJSON *jsConfig);
void DB_Cleanup();

#define DB_FLG_UNSUPPORTED 1
#define DB_FLG_NOT_RFB 2
#define DB_FLG_OUT_OF_DICT 4

enum
{
    TM_DB_UPDATE=WM_USER+10,
    TM_DB_MOVE_TO_IGNORED,
    TM_DB_MOVE_TO_IGNORED_FINISH,
    TM_DB_ADD_IGNORE_FILTER,
    TM_DB_RESCAN_ITEM,
    TM_DB_ITEM_COMMENT,
    TM_DB_MAINTANCE_RESCAN_INGORED,
    TM_DB_MAINTANCE_REBUILD_BAD,
    TM_DB_MAINTANCE_REBUILD_IGNORED,
    TM_DB_MAINTANCE_REBUILD_GOOD,
    TM_DB_MAINTANCE_REBUILD_IGNORE_MASKS,
    TM_DB_MAINTANCE_RESCAN_UNSUPPORTED,
    TM_DB_MAINTANCE_REBUILD_TOTAL_IPS,
    IM_DB_MAINTANCE_PURGE_CHECKER_DB_BEGIN,
    IM_DB_MAINTANCE_PURGE_CHECKER_DB_END,
};

typedef struct _BAD_ITEM_INFO
{
    DWORD dwIP;
    WORD wPort;
    DWORD dwIdx;
    DWORD dwFlg;
} BAD_ITEM_INFO, *PBAD_ITEM_INFO;

typedef struct _GOOD_ITEM_INFO
{
    DWORD dwIP;
    WORD wPort;
    DWORD dwIdx;
    LPSTR lpDesktop;
    bool bNoAuth;
    char szPassword[9];
} GOOD_ITEM_INFO, *PGOOD_ITEM_INFO;

enum UPDATE_TYPE
{
    UT_RFB_FOUND=100,
    UT_NON_RFB_FOUND,
    UT_BAD_UPDATE,
    UT_GOOD_FOUND,
};

typedef struct _UPDATE_QUEUE
{
    PCONNECTION lpConnection;
    UPDATE_TYPE ut;

    union
    {
        MASSCAN mii;
        BAD_ITEM_INFO bii;
        GOOD_ITEM_INFO gii;
    };

    _UPDATE_QUEUE *lpNext;
} UPDATE_QUEUE, *PUPDATE_QUEUE;

void DB_PostUpdateQueue(PUPDATE_QUEUE lpQueue);

extern CRITICAL_SECTION csMasscanAccess,csWorkerAccess;

extern DWORD dwDBUpdateThreadId;

typedef struct _RESCAN_ITEM_PARAM
{
    char szAddress[MAX_PATH];
    DWORD dwIdx;
} RESCAN_ITEM_PARAM, *PRESCAN_ITEM_PARAM;

typedef struct _CHANGE_ITEM_COMMENT_PARAM
{
    char szAddress[MAX_PATH];
    char szComment[MAX_PATH];
} CHANGE_ITEM_COMMENT_PARAM, *PCHANGE_ITEM_COMMENT_PARAM;

void DB_UpdateMenuState(HMENU hMenu,DWORD dwItem);

#define REBUILD_REASONS_COUNT 1000

extern HANDLE hDBThreadsGroup;

#endif // DB_H_INCLUDED
