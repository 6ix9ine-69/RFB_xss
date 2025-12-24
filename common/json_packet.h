#ifndef JSON_PACKET_H_INCLUDED
#define JSON_PACKET_H_INCLUDED

#include <cjson\cjson.h>
#include "socket.h"

void JSON_SendPacket(PSOCKET_INFO lpSock,cJSON *jsPacket);
void JSON_SendPacketAndFree(PSOCKET_INFO lpSock,cJSON *jsPacket);

enum CHECKER_RESULT
{
    CKR_RESULT_RFB=1,
    CKR_RESULT_NOT_RFB,
    CKR_RESULT_FAILED
};

enum WORKER_RESULT
{
    WKR_RESULT_DONE=1,
    WKR_RESULT_FAILED,
    WKR_RESULT_OUT_OF_DICT,
    WKR_RESULT_NOT_RFB,
    WKR_RESULT_UNSUPPORTED,
    WKR_RESULT_IN_USE
};

#endif // JSON_PACKET_H_INCLUDED
