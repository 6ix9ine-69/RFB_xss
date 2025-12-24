#include "sys_includes.h"
#include <shlwapi.h>
#include <intrin.h>

#pragma intrinsic(__rdtsc)

SYSLIBFUNC(DWORD) xor128(int val)
{
    if (!val)
        return 0;

    static DWORD y=362436069,
                 z=521288629,
                 w=88675123,
                 x=0x12345678;
    if (x == 0x12345678)
        x=(ULONG)__rdtsc();
    DWORD t;
    t=(x^(x<<11));
    x=y;
    y=z;
    z=w;
    w=(w^(w>>19))^(t^(t>>8));
    return (w%(val*100))/100;
}

SYSLIBFUNC(DWORD) xor128_Between(int iMin,int iMax)
{
    if (iMin == iMax)
        return iMin;

    if (iMin > iMax)
    {
        int iPrevMin=iMin;
        iMin=iMax;
        iMax=iPrevMin;
    }

    DWORD dwRet=0;
    while (true)
    {
        dwRet=xor128((iMax-iMin)+1)+iMin;

        if (dwRet <= iMax)
            break;
    }
    return dwRet;
}

SYSLIBFUNC(DWORD) GetRndDWORD()
{
    union
    {
        struct
        {
            byte b1;
            byte b2;
            byte b3;
            byte b4;
        };
        DWORD dw1;
    } RndDword;

    RndDword.b1=(byte)xor128(MAXBYTE);
    RndDword.b2=(byte)xor128(MAXBYTE);
    RndDword.b3=(byte)xor128(MAXBYTE);
    RndDword.b4=(byte)xor128(MAXBYTE);
    return RndDword.dw1;
}
