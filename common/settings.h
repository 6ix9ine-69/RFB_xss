#ifndef SETTINGS_H_INCLUDED
#define SETTINGS_H_INCLUDED

#include <cjson\cjson.h>

void Settings_Save(cJSON *jsSettings);
cJSON *Settings_Init(cJSON *jsConfig);
void Setting_ChangeValue(cJSON *jsGroup,LPCSTR lpItem,int iValue);

#endif // SETTINGS_H_INCLUDED
