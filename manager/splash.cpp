#include "includes.h"
#include <wingdi.h>
#include <gdiplus.h>

#define RDHDR sizeof(RGNDATAHEADER)
#define MAXBUF 40

using namespace Gdiplus::DllExports;
static HRGN CreateRgnFromBitmap(HBITMAP hBmp,COLORREF crColor)
{
    HRGN hRgn=NULL;
    HDC hDC=NULL,hCompDC=NULL;
    HGDIOBJ hOldBmp=NULL;
    PRGNDATAHEADER lpRgnData=NULL;

    do
    {
        if (!hBmp)
            break;

        BITMAP bmp;
        GetObject(hBmp,sizeof(bmp),&bmp);

        hDC=GetDC(NULL);
        if (!hDC)
            break;

        hCompDC=CreateCompatibleDC(hDC);
        if (!hCompDC)
            break;

        hOldBmp=SelectObject(hCompDC,hBmp);
        if (!hOldBmp)
            break;

        DWORD dwBlocksCount=1;

        lpRgnData=(PRGNDATAHEADER)MemAlloc(RDHDR+dwBlocksCount*MAXBUF*sizeof(RECT));
        if (!lpRgnData)
            break;

        int iFirst=0;
        bool bWasfirst=false;

        lpRgnData->dwSize=RDHDR;
        lpRgnData->iType=RDH_RECTANGLES;
        lpRgnData->nCount=1;

        for (int i=0; i < bmp.bmHeight; i++)
        {
            for (int j=0; j < bmp.bmWidth; j++)
            {
                bool bIsMask=(GetPixel(hCompDC,j,bmp.bmHeight-i-1) != crColor);

                if ((bWasfirst) && (((bIsMask) && (j == (bmp.bmWidth-1))) || ((bIsMask^(j < bmp.bmWidth)))))
                {
                    LPRECT lpRects=(LPRECT)((LPBYTE)lpRgnData+RDHDR),
                           lpRect=&lpRects[lpRgnData->nCount];

                    lpRect->left=iFirst;
                    lpRect->top=bmp.bmHeight-i-1;
                    lpRect->right=j+(j == (bmp.bmWidth-1));
                    lpRect->bottom=bmp.bmHeight-i;

                    lpRgnData->nCount++;
                    if (lpRgnData->nCount >= dwBlocksCount*MAXBUF)
                    {
                        dwBlocksCount++;
                        lpRgnData=(PRGNDATAHEADER)MemRealloc(lpRgnData,RDHDR+dwBlocksCount*MAXBUF*sizeof(RECT));

                        if (!lpRgnData)
                            break;
                    }
                    bWasfirst=false;
                }
                else if ((!bWasfirst) && (bIsMask))
                {
                    iFirst=j;
                    bWasfirst=true;
                }
            }
        }

        hRgn=CreateRectRgn(0,0,0,0);
        if (!hRgn)
            break;

        LPRECT lpRects=(LPRECT)((LPBYTE)lpRgnData+RDHDR);
        for(int i=0; i < (int)lpRgnData->nCount; i++)
        {
            HRGN hr=CreateRectRgn(lpRects[i].left,lpRects[i].top,lpRects[i].right,lpRects[i].bottom);
            CombineRgn(hRgn,hRgn,hr,RGN_OR);
            if (hr)
                DeleteObject(hr);
        }
    }
    while (false);

    if (hOldBmp)
        SelectObject(hCompDC,hOldBmp);

    if (hCompDC)
        DeleteDC(hCompDC);

    if (hDC)
        ReleaseDC(NULL,hDC);

    if (lpRgnData)
        MemFree(lpRgnData);

    return hRgn;
}

static HBITMAP LoadBitmap(DWORD dwID)
{
    HBITMAP hBmp=NULL;

    HRSRC hRes=FindResourceW(hInstance,MAKEINTRESOURCE(dwID),RT_RCDATA);
    if (hRes)
    {
        HGLOBAL hResLoaded=LoadResource(hInstance,hRes);
        if (hResLoaded)
        {
            LPVOID lpData=LockResource(hResLoaded);
            if (lpData)
            {
                DWORD dwDataSize=SizeofResource(hInstance,hRes);
                HGLOBAL hGlobal=GlobalAlloc(GMEM_MOVEABLE,dwDataSize);
                if (hGlobal)
                {
                    LPVOID lpGlobal=GlobalLock(hGlobal);
                    if (lpGlobal)
                    {
                        memcpy(lpGlobal,lpData,dwDataSize);
                        GlobalUnlock(hGlobal);

                        LPSTREAM lpStream;
                        if (!CreateStreamOnHGlobal(hGlobal,true,&lpStream))
                        {
                            Gdiplus::GpBitmap *gpBitmap=NULL;
                            if (GdipCreateBitmapFromStream(lpStream,&gpBitmap) == Gdiplus::Ok)
                            {
                                GdipCreateHBITMAPFromBitmap(gpBitmap,&hBmp,0);
                                GdipDisposeImage(gpBitmap);
                            }
                            lpStream->Release();
                        }
                        else
                            GlobalFree(hGlobal);
                    }
                    else
                        GlobalFree(hGlobal);
                }
                UnlockResource(hResLoaded);
            }
            FreeResource(hResLoaded);
        }
    }
    return hBmp;
}

static HANDLE hThread;
static HWND hSplashDlg;
static HBITMAP hBgBmp;
static HRGN hRgn;
static INT_PTR CALLBACK SplashDlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    INT_PTR dwRet=FALSE;

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            Gdiplus::GdiplusStartupInput gsi;
            memset(&gsi,0,sizeof(gsi));
            gsi.GdiplusVersion=1;

            ULONG_PTR dwToken;
            if (GdiplusStartup(&dwToken,&gsi,NULL) == Gdiplus::Ok)
            {
                hBgBmp=LoadBitmap(104);
                if (hBgBmp)
                    hRgn=CreateRgnFromBitmap(hBgBmp,RGB(255,0,255));

                Gdiplus::GdiplusShutdown(dwToken);
            }

            SendDlgItemMessage(hDlg,IDC_SPLASH_BG,STM_SETIMAGE,IMAGE_BITMAP,(LPARAM)hBgBmp);
            SetWindowRgn(hDlg,hRgn,true);
            hSplashDlg=hDlg;
            break;
        }
        case WM_CTLCOLORSTATIC:
        {
            if (GetWindowLongPtr((HWND)lParam,GWLP_ID) != IDC_SPLASH_TEXT)
                break;

            HDC hDC=(HDC)wParam;

            SetTextColor(hDC,RGB(255,255,255));
            SetBkMode(hDC,TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        case WM_CLOSE:
        {
            SetWindowRgn(hDlg,NULL,true);
            SendDlgItemMessage(hDlg,IDC_SPLASH_BG,STM_SETIMAGE,IMAGE_BITMAP,NULL);

            DeleteObject(hRgn);
            DeleteObject(hBgBmp);
            EndDialog(hDlg,0);
            break;
        }
    }
    return dwRet;
}

static void WINAPI Splash_DlgThread(LPVOID)
{
    DialogBoxParamW(hInstance,MAKEINTRESOURCE(IDD_SPLASH),NULL,(DLGPROC)SplashDlgProc,NULL);
    hSplashDlg=NULL;
    return;
}

void Splash_Start()
{
    hSplashDlg=NULL;
    CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)Splash_DlgThread,NULL,0,NULL);

    while (!hSplashDlg)
        Sleep(1);
    return;
}

void Splash_Stop()
{
    PostMessage(hSplashDlg,WM_CLOSE,NULL,NULL);
    WaitForSingleObject(hThread,INFINITE);
    CloseHandle(hThread);
    return;
}

void Splash_SetText(LPTSTR lpText)
{
    do
    {
        if (!lpText)
            break;

        if (!hSplashDlg)
            break;

        SetDlgItemText(hSplashDlg,IDC_SPLASH_TEXT,lpText);

        InvalidateRect(GetDlgItem(hSplashDlg,IDC_SPLASH_BG),NULL,true);
        InvalidateRect(GetDlgItem(hSplashDlg,IDC_SPLASH_TEXT),NULL,true);
    }
    while (false);
    return;
}
