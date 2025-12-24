#ifndef COMMANDS_H_INCLUDED
#define COMMANDS_H_INCLUDED

void Command_StopCmd(PCONNECTION lpConnection);
void Command_ResumeCmd(PCONNECTION lpConnection);

void Command_StopAllCmd(HWND hDlg,CONNECTION_TYPE dwType);

typedef struct _PARSE_PACKET_PARAMS
{
    cJSON *jsPacket;
    PCONNECTION lpConnection;
} PARSE_PACKET_PARAMS, *PPARSE_PACKET_PARAMS;

void Command_ParsePacket(PCONNECTION lpConnection,cJSON *jsPacket);

extern bool bHelloStat;
extern bool bStopping;

#endif // COMMANDS_H_INCLUDED
