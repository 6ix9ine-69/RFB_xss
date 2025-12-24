#ifndef STRFMT_H_INCLUDED
#define STRFMT_H_INCLUDED

#define MAX_SPRINTF_STRING_SIZE 1024

namespace SYSLIB
{
    DWORD wsprintfExW(LPWSTR *lppBuffer,DWORD dwOffset,LPCWSTR lpFormat,va_list args);
    DWORD wsprintfExA(LPSTR *lppBuffer,DWORD dwOffset,LPCSTR lpFormat,va_list args);
};

#endif // STRFMT_H_INCLUDED
