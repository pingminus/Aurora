#include "window_frame.h"
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
namespace opengod {
int frame_inset(HWND window) {
  return IsZoomed(window) ? 0 : MulDiv(5, static_cast<int>(GetDpiForWindow(window)), 96);
}
std::optional<LRESULT> handle_frame_message(HWND window,
                                            UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam) {
  if (message == WM_NCCALCSIZE && wparam) {
    if (IsZoomed(window)) {
      MONITORINFO monitor{sizeof(MONITORINFO)};
      if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor))
        reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam)->rgrc[0] = monitor.rcWork;
    }
    return 0;
  }
  if (message == WM_GETMINMAXINFO) {
    auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
    info->ptMinTrackSize = {MulDiv(640, static_cast<int>(GetDpiForWindow(window)), 96),
                            MulDiv(420, static_cast<int>(GetDpiForWindow(window)), 96)};
    return 0;
  }
  if (message == WM_NCHITTEST && !IsZoomed(window)) {
    POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    ScreenToClient(window, &point);
    RECT rect{};
    GetClientRect(window, &rect);
    const int edge = frame_inset(window);
    const bool left = point.x < edge, right = point.x >= rect.right - edge;
    const bool top = point.y < edge, bottom = point.y >= rect.bottom - edge;
    if (top && left)
      return HTTOPLEFT;
    if (top && right)
      return HTTOPRIGHT;
    if (bottom && left)
      return HTBOTTOMLEFT;
    if (bottom && right)
      return HTBOTTOMRIGHT;
    if (left)
      return HTLEFT;
    if (right)
      return HTRIGHT;
    if (top)
      return HTTOP;
    if (bottom)
      return HTBOTTOM;
    return HTCLIENT;
  }
  return std::nullopt;
}
namespace {
constexpr UINT_PTR kDragSubclass = 0x415552;
LRESULT CALLBACK
drag_proc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data) {
  auto region = reinterpret_cast<HRGN>(data);
  if (message == WM_NCDESTROY) {
    RemoveWindowSubclass(window, drag_proc, id);
    DeleteObject(region);
  } else if (message == WM_NCHITTEST) {
    POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ScreenToClient(GetAncestor(window, GA_ROOT), &point);
    if (PtInRegion(region, point.x, point.y))
      return window == GetAncestor(window, GA_ROOT) ? HTCAPTION : HTTRANSPARENT;
  }
  return DefSubclassProc(window, message, wp, lp);
}
BOOL CALLBACK install_drag(HWND window, LPARAM data) {
  DWORD_PTR previous = 0;
  if (GetWindowSubclass(window, drag_proc, kDragSubclass, &previous)) {
    CombineRgn(reinterpret_cast<HRGN>(previous), reinterpret_cast<HRGN>(data), nullptr, RGN_COPY);
  } else {
    HRGN copy = CreateRectRgn(0, 0, 0, 0);
    CombineRgn(copy, reinterpret_cast<HRGN>(data), nullptr, RGN_COPY);
    if (!SetWindowSubclass(window, drag_proc, kDragSubclass, reinterpret_cast<DWORD_PTR>(copy)))
      DeleteObject(copy);
  }
  return TRUE;
}
}  // namespace
void set_drag_regions(HWND root, HWND shell, const std::vector<RECT>& regions) {
  HRGN combined = CreateRectRgn(0, 0, 0, 0);
  for (const auto& rect : regions) {
    HRGN part = CreateRectRgnIndirect(&rect);
    CombineRgn(combined, combined, part, RGN_OR);
    DeleteObject(part);
  }
  install_drag(root, reinterpret_cast<LPARAM>(combined));
  install_drag(shell, reinterpret_cast<LPARAM>(combined));
  EnumChildWindows(shell, install_drag, reinterpret_cast<LPARAM>(combined));
  DeleteObject(combined);
}
}  // namespace opengod
