#include <winsock2.h>
#include <windows.h>
#include <shlwapi.h>

#include <syslib\mem.h>
#include <syslib\net.h>
#include <syslib\files.h>
#include <syslib\str.h>

#include <cjson\cjson.h>

#include "adapters.h"

static DWORD dwIPsCount;
static PIPS lpIPs;

static DWORD NetApplySubnetMask(DWORD dwSubnet,byte bMask)
{
    return (dwSubnet & htonl(0xFFFFFFFF << (32-bMask)));
}

static cJSON *FindVPN(cJSON *jsRanges)
{
    cJSON *jsVPN=NULL;

    PIP_ADAPTER_INFO lpAdapters=Network_GetActiveNetworkAdapters(NULL);
    if (lpAdapters)
    {
        bool bDone=false;

        PIP_ADAPTER_INFO lpAdapter=lpAdapters;
        while (lpAdapter)
        {
            PIP_ADDR_STRING IpAddress=&lpAdapter->IpAddressList;
            while (IpAddress)
            {
                ULONG ulIP=inet_addr(IpAddress->IpAddress.String),
                      ulSubnet=NetApplySubnetMask(ulIP,12);

                ulIP=ntohl(ulIP);

                for (DWORD i=0; i < dwIPsCount; i++)
                {
                    if (lpIPs[i].ulSubNet != ulSubnet)
                        continue;

                    if ((ulIP < lpIPs[i].ulStart) || (ulIP > lpIPs[i].ulEnd))
                        continue;

                    bDone=true;

                    cJSON *jsRange=cJSON_GetArrayItem(jsRanges,i);
                    if (!jsRange)
                        break;

                    cJSON *jsVPNs=cJSON_GetObjectItem(jsRange,"vpn");
                    if (!jsVPNs)
                        break;

                    DWORD dwIdx=ulIP-lpIPs[i].ulStart,
                          dwVPNs=cJSON_GetArraySize(jsVPNs),
                          dwIPsPerVPN=lpIPs[i].ulCount/dwVPNs+((lpIPs[i].ulCount % dwVPNs) ? 1 : 0);

                    jsVPN=cJSON_GetArrayItem(jsVPNs,dwIdx/dwIPsPerVPN);
                    break;
                }

                if (bDone)
                    break;

                IpAddress=IpAddress->Next;
            }

            if (bDone)
                break;

            lpAdapter=lpAdapter->Next;
        }
        MemFree(lpAdapters);
    }
    return jsVPN;
}

static void CreateVPNFile(LPCSTR lpTemplate,LPCSTR lpIP,LPCSTR lpPort)
{
    do
    {
        WCHAR szTmpDir[MAX_PATH];
        if (!GetTempPathW(ARRAYSIZE(szTmpDir),szTmpDir))
            break;

        lstrcatW(szTmpDir,L"\\vpn.vpn");

        HANDLE hFile=CreateFileA(lpTemplate,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            break;

        HANDLE hMapping=CreateFileMappingA(hFile,NULL,PAGE_READONLY,0,0,NULL);
        if (!hMapping)
        {
            CloseHandle(hFile);
            break;
        }

        LPCSTR lpMap=(LPCSTR)MapViewOfFile(hMapping,FILE_MAP_READ,0,0,0);
        if (!lpMap)
        {
            CloseHandle(hMapping);
            CloseHandle(hFile);
            break;
        }

        HANDLE hVPNFile=CreateFileW(szTmpDir,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
        if (!hVPNFile)
        {
            UnmapViewOfFile(lpMap);
            CloseHandle(hMapping);
            CloseHandle(hFile);
            break;
        }

        LPCSTR lpPtr=lpMap,lpHostName=StrStrA(lpMap,"\r\n\t\tstring HubName");
        DWORD dwTmp;
        WriteFile(hVPNFile,lpMap,lpHostName-lpPtr,&dwTmp,NULL);
        WriteFile(hVPNFile,lpIP,lstrlenA(lpIP),&dwTmp,NULL);

        lpPtr=lpHostName;

        if (lpPort)
        {
            LPCSTR lpPortUDP=StrStrA(lpPtr,"\r\n\t\tuint PortUDP");
            WriteFile(hVPNFile,lpPtr,lpPortUDP-lpPtr,&dwTmp,NULL);
            WriteFile(hVPNFile,lpPort,lstrlenA(lpPort),&dwTmp,NULL);
            lpPtr=lpPortUDP;
        }

        WriteFile(hVPNFile,lpPtr,lstrlenA(lpPtr),&dwTmp,NULL);

        UnmapViewOfFile(lpMap);
        CloseHandle(hMapping);
        CloseHandle(hFile);

        CloseHandle(hVPNFile);
    }
    while (false);
    return;
}

static void Do(cJSON *jsConfig)
{
    do
    {
        cJSON *jsTemplates=cJSON_GetObjectItem(jsConfig,"templates");
        if (!jsTemplates)
            break;

        cJSON *jsRanges=cJSON_GetObjectItem(jsConfig,"ranges");
        if (!jsRanges)
            break;

        DWORD dwCount=cJSON_GetArraySize(jsRanges);
        if (!dwCount)
            break;

        lpIPs=(PIPS)MemAlloc(dwCount*sizeof(*lpIPs));
        if (!lpIPs)
            break;

        bool bDone=true;
        for (DWORD i=0; i < dwCount; i++)
        {
            cJSON *jsRange=cJSON_GetArrayItem(jsRanges,i);
            if (!jsRange)
            {
                bDone=false;
                break;
            }

            cJSON *jsIP=cJSON_GetObjectItem(jsRange,"ip");
            if (!jsIP)
            {
                bDone=false;
                break;
            }

            LPSTR lpIP=StrDuplicateA(cJSON_GetStringValue(jsIP),0);
            if (!lpIP)
            {
                bDone=false;
                break;
            }

            LPSTR lpIP2=StrChrA(lpIP,'-');
            if (!lpIP2)
            {
                MemFree(lpIP);

                bDone=false;
                break;
            }

            *lpIP2=0;
            lpIP2++;

            ULONG ulStart=NetResolveAddress(lpIP),
                  ulEnd=NetResolveAddress(lpIP2);

            MemFree(lpIP);

            lpIPs[dwIPsCount].ulSubNet=NetApplySubnetMask(ulStart,12);
            lpIPs[dwIPsCount].ulStart=ntohl(ulStart);
            lpIPs[dwIPsCount].ulEnd=ntohl(ulEnd);
            lpIPs[dwIPsCount].ulCount=(lpIPs[dwIPsCount].ulEnd-lpIPs[dwIPsCount].ulStart)+1;
            dwIPsCount++;
        }

        if (!bDone)
            break;

        cJSON *jsVPN=FindVPN(jsRanges);
        if (!jsVPN)
            break;

        if (cJSON_GetBoolFromObject(jsVPN,"vpngate"))
        {
            CreateVPNFile(cJSON_GetStringFromObject(jsTemplates,"vpngate"),cJSON_GetStringFromObject(jsVPN,"ip"),NULL);
            break;
        }

        char szPort[100];
        wsprintfA(szPort,"%d",cJSON_GetIntFromObject(jsVPN,"port"));
        CreateVPNFile(cJSON_GetStringFromObject(jsTemplates,"vpn"),cJSON_GetStringFromObject(jsVPN,"ip"),szPort);
    }
    while (false);
    return;
}

void main()
{
    HANDLE hFile=CreateFileA(".\\vpn_mgr.json",GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        HANDLE hMapping=CreateFileMappingA(hFile,NULL,PAGE_READONLY,0,0,NULL);
        if (hMapping)
        {
            LPCSTR lpMap=(LPCSTR)MapViewOfFile(hMapping,FILE_MAP_READ,0,0,0);
            if (lpMap)
            {
                cJSON *jsConfig=cJSON_ParseWithLength(lpMap,GetFileSize(hFile,NULL));
                if (jsConfig)
                {
                    Do(jsConfig);
                    cJSON_free(jsConfig);
                }
                UnmapViewOfFile(lpMap);
            }
            CloseHandle(hMapping);
        }
        CloseHandle(hFile);
    }
    ExitProcess(0);
}

