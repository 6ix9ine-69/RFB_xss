#ifndef RFB_PROTO_H_INCLUDED
#define RFB_PROTO_H_INCLUDED

#include <stdint.h>

#define RFB_BANNER_SIZE 12

struct _IOCP_CLIENT;

enum RFB_STATE
{
    VNC_HANDSHAKE,
    VNC_SECURITY_TYPE,
    VNC_AUTH,
    VNC_SECURITY_RESULT,
    VNC_SERVER_INFO,
    VNC_AUTH_DONE,
};

enum RFB_ERROR_CODE
{
    OK,
    NEED_MORE_DATA,
    ERR_CONN_FAILED,
    ERR_NOT_RFB,
    ERR_AUTH_UNSUPPORTED,
    ERR_TOO_MANY_AUTH,
    ERR_AUTH_FAILED,
};

typedef struct _RFB_CLIENT
{
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwAuthScheme;

    RFB_STATE dwState;
    RFB_ERROR_CODE dwErr;

    DWORD dwIdx;
    bool bGetOutOfHere;
    bool bUpdated;

    char szDesktop[512];

    _IOCP_CLIENT *lpIOCP;
} RFB_CLIENT, *PRFB_CLIENT;

bool RFB_HandleAnswer(_IOCP_CLIENT *lpClient);


#define rfbConnFailed 0
#define rfbNoAuth 1
#define rfbVncAuth 2


#define rfbVncAuthOK 0
#define rfbVncAuthFailed 1
#define rfbVncAuthTooMany 2


typedef unsigned long CARD32;
typedef unsigned short CARD16;
typedef unsigned char  CARD8;

#define rfbClientSwap32IfLE(l) \
    ((CARD32) ((((l) & 0xff000000) >> 24) | \
     (((l) & 0x00ff0000) >> 8)  | \
	 (((l) & 0x0000ff00) << 8)  | \
	 (((l) & 0x000000ff) << 24)))

#define CHALLENGESIZE 16
#define MAXPWLEN 8

typedef struct {

    uint8_t bitsPerPixel;		/* 8,16,32 only */

    uint8_t depth;		/* 8 to 32 */

    uint8_t bigEndian;		/* True if multi-byte pixels are interpreted
				   as big endian, or if single-bit-per-pixel
				   has most significant bit of the byte
				   corresponding to first (leftmost) pixel. Of
				   course this is meaningless for 8 bits/pix */

    uint8_t trueColour;		/* If false then we need a "colour map" to
				   convert pixels to RGB.  If true, xxxMax and
				   xxxShift specify bits used for red, green
				   and blue */

    /* the following fields are only meaningful if trueColour is true */

    uint16_t redMax;		/* maximum red value (= 2^n - 1 where n is the
				   number of bits used for red). Note this
				   value is always in big endian order. */

    uint16_t greenMax;		/* similar for green */

    uint16_t blueMax;		/* and blue */

    uint8_t redShift;		/* number of shifts needed to get the red
				   value in a pixel to the least significant
				   bit. To find the red value from a given
				   pixel, do the following:
				   1) Swap pixel value according to bigEndian
				      (e.g. if bigEndian is false and host byte
				      order is big endian, then swap).
				   2) Shift right by redShift.
				   3) AND with redMax (in host byte order).
				   4) You now have the red value between 0 and
				      redMax. */

    uint8_t greenShift;		/* similar for green */

    uint8_t blueShift;		/* and blue */

    uint8_t pad1;
    uint16_t pad2;

} rfbPixelFormat;

typedef struct _rfbServerInitMsg {
    CARD16 framebufferWidth;
    CARD16 framebufferHeight;
    rfbPixelFormat format;
    CARD32 nameLength;
} rfbServerInitMsg;

#endif // RFB_PROTO_H_INCLUDED
