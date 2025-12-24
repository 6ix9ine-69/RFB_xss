#ifndef CONNECTION_CHECK_H_INCLUDED
#define CONNECTION_CHECK_H_INCLUDED

void InetCheck_Start(HWND hDlg);
void InetCheck_Stop();

bool IsOnline();

#define CHECK_DEFAULT_TIMEOUT 90

enum INTERNET_STATE
{
    INET_FAILED,
    INET_OK,
    INET_TIMEOUT,
    INET_NO_INET
};

#endif // CONNECTION_CHECK_H_INCLUDED
