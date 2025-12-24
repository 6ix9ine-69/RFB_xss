#include <windows.h>
#include <dwmapi.h>

static DWORD dwSnapX,dwSnapY;
static RECT rcWork;
void HandleStickyMsg(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    POINT ptCurPos;
    GetCursorPos(&ptCurPos);

    RECT rcWindow;
    GetWindowRect(hDlg,&rcWindow);

    DWORD dwX=rcWindow.right-rcWindow.left,
          dwY=rcWindow.bottom-rcWindow.top;

    LPRECT lpRect=(LPRECT)lParam;

    switch (uMsg)
    {
        case WM_ENTERSIZEMOVE:
        {
            dwSnapX=ptCurPos.x-rcWindow.left;
            dwSnapY=ptCurPos.y-rcWindow.top;

            SystemParametersInfo(SPI_GETWORKAREA,0,&rcWork,0);

            RECT rcInvisibleBorders;
            if (SUCCEEDED(DwmGetWindowAttribute(hDlg,DWMWA_EXTENDED_FRAME_BOUNDS,&rcInvisibleBorders,sizeof(rcInvisibleBorders))))
            {
                rcWork.left-=rcInvisibleBorders.left-rcWindow.left;
                rcWork.top-=rcInvisibleBorders.top-rcWindow.top;
                rcWork.right+=rcWindow.right-rcInvisibleBorders.right;
                rcWork.bottom+=rcWindow.bottom-rcInvisibleBorders.bottom;
            }
            break;
        }
        case WM_MOVING:
        {
            OffsetRect(lpRect,ptCurPos.x-(lpRect->left+dwSnapX),ptCurPos.y-(lpRect->top+dwSnapY));

            if (lpRect->left < rcWork.left)
            {
                lpRect->left=rcWork.left;
                lpRect->right=rcWork.left+dwX;
            }

            if (lpRect->right > rcWork.right)
            {
                lpRect->right=rcWork.right;
                lpRect->left=lpRect->right-dwX;
            }

            if (lpRect->top < rcWork.top)
            {
                lpRect->top=rcWork.top;
                lpRect->bottom=dwY;
            }

            if (lpRect->bottom > rcWork.bottom)
            {
                lpRect->bottom=rcWork.bottom;
                lpRect->top=lpRect->bottom-dwY;
            }
            break;
        }
        case WM_SIZING:
        {
            if (lpRect->left < rcWork.left)
                lpRect->left=rcWork.left;

            if (lpRect->right > rcWork.right)
                lpRect->right=rcWork.right;

            if (lpRect->top < rcWork.top)
                lpRect->top=rcWork.top;

            if (lpRect->bottom > rcWork.bottom)
                lpRect->bottom=rcWork.bottom;

            break;
        }
    }
    return;
}

