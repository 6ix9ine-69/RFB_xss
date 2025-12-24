#include "includes.h"
#include <shlwapi.h>

#include <sqlite\sqlite3.h>

#include "dict.h"

static DWORD dwExternalPasswordsCount=0;
DWORD Dict_PasswordsCount()
{
    return dwExternalPasswordsCount;
}

static int sqlite3_step_ex(sqlite3_stmt *hStmt)
{
    int iErr;
    while (true)
    {
        iErr=sqlite3_step(hStmt);

        if ((iErr == SQLITE_BUSY) || (iErr == SQLITE_LOCKED))
        {
            if (WaitForSingleObject(hShutdownEvent,1) == WAIT_TIMEOUT)
                continue;
        }

        break;
    }
    return iErr;
}

static sqlite3 *hDB;
static DWORD dwTls;
LPCSTR Dict_GetPassword(DWORD dwIdx,LPSTR lpPassword)
{
    LPCSTR lpOut=NULL;
    do
    {
        if (dwIdx >= Dict_PasswordsCount())
            break;

        sqlite3_stmt *hStmt=(sqlite3_stmt*)TlsGetValue(dwTls);
        if (!hStmt)
        {
            int iErr=sqlite3_prepare_v2(hDB,"SELECT password FROM list WHERE id=?",-1,&hStmt,0);
            if (iErr != SQLITE_OK)
            {
                DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"sqlite3_prepare_v2() failed, %d",iErr);
                break;
            }

            TlsSetValue(dwTls,hStmt);
        }

        sqlite3_reset(hStmt);

        sqlite3_bind_int(hStmt,1,dwIdx+1);

        int iErr=sqlite3_step_ex(hStmt);
        if (iErr != SQLITE_ROW)
        {
            DbgLog_Event(&siManager,LOG_LEVEL_ERROR,"sqlite3_step(%d) failed, %d",dwIdx,iErr);
            break;
        }

        lstrcpynA(lpPassword,(LPCSTR)sqlite3_column_text(hStmt,0),9);
        lpOut=lpPassword;
    }
    while (false);

    return lpOut;
}

bool Dict_Init(cJSON *jsConfig)
{
    bool bRet=false;
    do
    {
        if (sqlite3_initialize() != SQLITE_OK)
            break;

        if (sqlite3_open_v2(cJSON_GetStringFromObject(jsConfig,"list"),&hDB,SQLITE_OPEN_READONLY,NULL) != SQLITE_OK)
            break;

        sqlite3_stmt *hStmt2;
        if (sqlite3_prepare_v2(hDB,"SELECT seq FROM sqlite_sequence WHERE name=\"list\"",-1,&hStmt2,0) != SQLITE_OK)
            break;

        if (sqlite3_step_ex(hStmt2) != SQLITE_ROW)
            break;

        dwExternalPasswordsCount=sqlite3_column_int(hStmt2,0);
        if (!dwExternalPasswordsCount)
            break;

        sqlite3_finalize(hStmt2);

        dwTls=TlsAlloc();

        bRet=true;
    }
    while (false);
    return bRet;
}
