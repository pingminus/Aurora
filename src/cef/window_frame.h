#pragma once
#include <windows.h>
#include <optional>
#include <vector>
namespace opengod {
// Native resizing and monitor geometry remain owned by Windows.
int frame_inset(HWND window);
std::optional<LRESULT> handle_frame_message(HWND window,
                                            UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam);
void set_drag_regions(HWND root, HWND shell, const std::vector<RECT>& regions);
}  // namespace opengod
