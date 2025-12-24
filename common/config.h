#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include <cjson\cjson.h>

cJSON *Config_Parse();
cJSON *Config_ParseFile(LPCSTR lpFileName,bool bSilent=true);

void Config_Dump(LPCSTR lpFile);

#define Config_Update() Config_Dump(NULL)

#endif // CONFIG_H_INCLUDED
