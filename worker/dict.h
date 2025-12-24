#ifndef DICT_H_INCLUDED
#define DICT_H_INCLUDED

bool Dict_Init(cJSON *jsConfig);

LPCSTR Dict_GetPassword(DWORD ullIdx,LPSTR lpPassword);
DWORD Dict_PasswordsCount();

#endif // DICT_H_INCLUDED
