#include "sys_includes.h"

DWORD Align(DWORD Size, DWORD Alignment)
{
	div_t DivRes;

	DivRes.quot = Size / Alignment;
	DivRes.rem = Size - DivRes.quot * Alignment;

	if(DivRes.rem == 0)
		return Size;

	return ((DivRes.quot + 1) * Alignment);
}

SYSLIBFUNC(BOOL) SysIsWindows64()
{
    SYSTEM_INFO si={0};
    GetNativeSystemInfo(&si);
    return (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
}

