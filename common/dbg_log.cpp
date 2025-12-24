#include <windows.h>
#include "json_packet.h"
#include "dbg_log.h"

#include <cjson\cjson.h>
#include <syslib\mem.h>
#include <syslib\str.h>
#include <syslib\time.h>

namespace SYSLIB
{
    DWORD StrFmt_FormatStringA(LPSTR lpDest,LPCSTR lpFormat,va_list args);
}

void DbgLog_SendEvent(PSOCKET_INFO lpSock,LPCSTR lpFunc,LPCSTR lpFile,DWORD dwLine,LOG_LEVEL dwLevel,LPCSTR lpFormat,...)
{
    do
    {
        if (lpSock->hSock == INVALID_SOCKET)
            break;

        if (!lpSock->bConnected)
            break;

        cJSON *jsPacket=cJSON_CreateObject();
        if (!jsPacket)
            break;

        cJSON *jsLog=cJSON_CreateObject();
        if (!jsLog)
        {
            cJSON_Delete(jsPacket);
            break;
        }

        cJSON_AddStringToObject(jsPacket,"type","log");
        cJSON_AddItemToObject(jsPacket,"log",jsLog);

        LPSTR lpBody=(LPSTR)MemQuickAlloc(1024);
        if (!lpBody)
        {
            cJSON_Delete(jsPacket);
            break;
        }

        va_list mylist;
        va_start(mylist,lpFormat);
        SYSLIB::StrFmt_FormatStringA(lpBody,lpFormat,mylist);
        va_end(mylist);

        cJSON_AddNumberToObject(jsLog,"time",Now());
        cJSON_AddStringToObject(jsLog,"func",lpFunc);
        cJSON_AddStringToObject(jsLog,"file",lpFile);
        cJSON_AddNumberToObject(jsLog,"line",dwLine);
        cJSON_AddNumberToObject(jsLog,"level",dwLevel);
        cJSON_AddStringToObject(jsLog,"body",lpBody);

        LPSTR lpDbg;
        if (StrFormatExA(&lpDbg,"%s:%d:%s %s",lpFile,dwLine,lpFunc,lpBody))
        {
            OutputDebugStringA(lpDbg);
            MemFree(lpDbg);
        }

        JSON_SendPacketAndFree(lpSock,jsPacket);
        MemFree(lpBody);
    }
    while (false);

    return;
}

