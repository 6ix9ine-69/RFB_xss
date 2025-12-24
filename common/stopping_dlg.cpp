#include <windows.h>
#include <tchar.h>

INT_PTR CALLBACK StoppingDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    if (uMsg == WM_INITDIALOG)
    {
        HWND hParent=GetParent(hDlg);

        EnableWindow(hParent,false);

        SetWindowLong(hDlg,GWL_EXSTYLE,GetWindowLongPtr(hDlg,GWL_EXSTYLE)|WS_EX_LAYERED);
        SetLayeredWindowAttributes(hDlg,GetSysColor(COLOR_MENU),200,LWA_ALPHA);

        RECT rcDlg;
        GetWindowRect(hDlg,&rcDlg);

        RECT rcParent;
        GetWindowRect(hParent,&rcParent);

        int nWidth=rcDlg.right-rcDlg.left,
        nHeight=rcDlg.bottom-rcDlg.top;

        int nX=((rcParent.right-rcParent.left)-nWidth)/2+rcParent.left,
        nY=((rcParent.bottom-rcParent.top)-nHeight)/2+rcParent.top;

        if (nX < 0)
            nX=0;
        if (nY < 0)
            nY=0;

        int nScreenWidth=GetSystemMetrics(SM_CXSCREEN),
        nScreenHeight=GetSystemMetrics(SM_CYSCREEN);

        if (nX+nWidth > nScreenWidth)
            nX=nScreenWidth-nWidth;
        if (nY+nHeight > nScreenHeight)
            nY=nScreenHeight-nHeight;

        MoveWindow(hDlg,nX,nY,nWidth,nHeight,false);
    }
    return FALSE;
}

