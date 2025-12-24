#ifndef DBG_LOG_H_INCLUDED
#define DBG_LOG_H_INCLUDED

enum LOG_LEVEL
{
    LOG_LEVEL_INFO=0,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_CONNECTION
};

void DbgLog_SendEvent(PSOCKET_INFO lpSock,LPCSTR lpFunc,LPCSTR lpFile,DWORD dwLine,LOG_LEVEL dwLevel,LPCSTR lpFormat,...);

#define DbgLog_Event(lpSock,dwLevel,...) DbgLog_SendEvent(lpSock,__FUNCTION__,__FILE__,__LINE__,dwLevel,__VA_ARGS__)

#endif // DBG_LOG_H_INCLUDED
