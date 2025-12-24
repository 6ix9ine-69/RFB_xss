#include <windows.h>
#include <tchar.h>

#include <syslib\osenv.h>
#include <syslib\mem.h>

#include "settings.h"
#include "config.h"

void Setting_ChangeValue(cJSON *jsGroup,LPCSTR lpItem,int iValue)
{
    cJSON *jsItem=cJSON_GetObjectItem(jsGroup,lpItem);
    if (!jsItem)
    {
        jsItem=cJSON_CreateNumber(0);
        cJSON_AddItemToObject(jsGroup,lpItem,jsItem);
    }

    cJSON_SetNumberValue(jsItem,iValue);
    return;
}

static LPSTR lpSettingsFile;
void Settings_Save(cJSON *jsSettings)
{
    do
    {
        if (!jsSettings)
            break;

        LPSTR lpSettings=cJSON_Print(jsSettings);
        if (!lpSettings)
            break;

        HANDLE hFile=CreateFileA(lpSettingsFile,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            MemFree(lpSettings);
            break;
        }

        DWORD dwTmp;
        WriteFile(hFile,lpSettings,lstrlenA(lpSettings),&dwTmp,NULL);
        CloseHandle(hFile);

        MemFree(lpSettings);
    }
    while (false);
    return;
}

cJSON *Settings_Init(cJSON *jsConfig)
{
    cJSON *jsSettings=NULL;
    do
    {
        LPCSTR lpFile=cJSON_GetStringFromObject(jsConfig,"settings");
        if (!lpFile)
            break;

        lpSettingsFile=SysExpandEnvironmentStringsExA(lpFile);
        if (!lpSettingsFile)
            break;

        jsSettings=Config_ParseFile(lpSettingsFile);
        if (jsSettings)
            break;

        jsSettings=cJSON_CreateObject();
    }
    while (false);
    return jsSettings;
}

