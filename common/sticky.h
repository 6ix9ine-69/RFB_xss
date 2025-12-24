#ifndef STICKY_H_INCLUDED
#define STICKY_H_INCLUDED

#define case_WM_SICKY case WM_ENTERSIZEMOVE:\
                      case WM_MOVING:\
                      case WM_SIZING

void HandleStickyMsg(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);

#endif // STICKY_H_INCLUDED
