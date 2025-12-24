#include <windows.h>
#include <tchar.h>

#include <syslib\mem.h>
#include <cjson\cjson.h>

cJSON *Config_ParseFile(LPCSTR lpFileName,bool bSilent)
{
    cJSON *jsConfig=NULL;

    HANDLE hFile=CreateFileA(lpFileName,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        HANDLE hMapping=CreateFileMappingA(hFile,NULL,PAGE_READONLY,0,0,NULL);
        if (hMapping)
        {
            LPCSTR lpMap=(LPCSTR)MapViewOfFile(hMapping,FILE_MAP_READ,0,0,0);
            if (lpMap)
            {
                jsConfig=cJSON_ParseWithLength(lpMap,GetFileSize(hFile,NULL));
                if (!jsConfig)
                {
                    if (!bSilent)
                        MessageBoxA(GetForegroundWindow(),"JSON_Parse() failed!",NULL,MB_ICONERROR);
                }

                UnmapViewOfFile(lpMap);
            }
            else if (!bSilent)
            {
                char szMsg[1024];
                wsprintfA(szMsg,"MapViewOfFile(\"%s\") failed! Error code: %x",lpFileName,GetLastError());
                MessageBoxA(GetForegroundWindow(),szMsg,NULL,MB_ICONERROR);
            }
            CloseHandle(hMapping);
        }
        else if (!bSilent)
        {
            char szMsg[1024];
            wsprintfA(szMsg,"CreateFileMapping(\"%s\") failed! Error code: %x",lpFileName,GetLastError());
            MessageBoxA(GetForegroundWindow(),szMsg,NULL,MB_ICONERROR);
        }
        CloseHandle(hFile);
    }
    else if (!bSilent)
    {
        char szMsg[1024];
        wsprintfA(szMsg,"CreateFile(\"%s\") failed! Error code: %x",lpFileName,GetLastError());
        MessageBoxA(GetForegroundWindow(),szMsg,NULL,MB_ICONERROR);
    }
    return jsConfig;
}

static cJSON *jsConfig;
static char szConfig[1024];
void Config_Dump(LPCSTR lpFileName)
{
    if (jsConfig)
    {
        if (!lpFileName)
            lpFileName=szConfig;

        LPSTR lpConfig=cJSON_PrintUnformatted(jsConfig);
        if (lpConfig)
        {
            HANDLE hFile=CreateFileA(lpFileName,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
            if (hFile)
            {
                DWORD dwTmp;
                WriteFile(hFile,lpConfig,lstrlenA(lpConfig),&dwTmp,NULL);

                CloseHandle(hFile);
            }
            MemFree(lpConfig);
        }
    }
    return;
}

cJSON *Config_Parse()
{
    if (!jsConfig)
    {
        lstrcpyA(szConfig,"config.json");

        int argc=0;
        LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        if ((argc == 3) && (!lstrcmpiW(argv[1],L"-c")))
            wsprintfA(szConfig,"%S",argv[2]);

        jsConfig=Config_ParseFile(szConfig,false);

        LocalFree(argv);
    }
    return jsConfig;
}

