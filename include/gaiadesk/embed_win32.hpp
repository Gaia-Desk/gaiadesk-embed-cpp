// gaiadesk-embed-cpp, Windows helpers: window ids and mask rects from Win32
// handles. Includes <windows.h>; include it only in Windows code.
//
// Mask rects are relative to the top-left of the shared window AS
// GetWindowRect REPORTS IT (title bar and borders included), in the same
// units: physical pixels for a per-monitor DPI aware app (Qt 6, Electron,
// WinForms with PerMonitorV2), logical ones for a DPI-unaware app. These
// helpers read the window through the same API, so they are right either way.
//
// SPDX-License-Identifier: MIT
#ifndef GAIADESK_EMBED_WIN32_HPP
#define GAIADESK_EMBED_WIN32_HPP

#ifndef _WIN32
#error "gaiadesk/embed_win32.hpp is for Windows"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <optional>

#include "embed.hpp"

namespace gaiadesk::embed::win32 {

/// An HWND as `Options::window_id`.
inline std::uint64_t window_id(HWND hwnd) noexcept {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(hwnd));
}

/// A rect in screen coordinates (e.g. from `GetWindowRect` of a child
/// control, or WinForms' `RectangleToScreen`) as a mask rect of `hwnd`.
/// nullopt when the window is gone.
inline std::optional<Rect> mask_rect_from_screen(HWND hwnd, double x, double y, double w, double h) noexcept {
    RECT wr{};
    if (!GetWindowRect(hwnd, &wr)) return std::nullopt;
    return Rect{x - wr.left, y - wr.top, w, h};
}

/// A rect in `hwnd`'s CLIENT coordinates (what a toolkit lays its widgets
/// out in, once scaled to the window's units) as a mask rect of `hwnd`:
/// moved by the client area's offset inside the window frame.
inline std::optional<Rect> mask_rect_from_client(HWND hwnd, double x, double y, double w, double h) noexcept {
    POINT origin{0, 0};
    RECT wr{};
    if (!ClientToScreen(hwnd, &origin) || !GetWindowRect(hwnd, &wr)) return std::nullopt;
    return Rect{x + (origin.x - wr.left), y + (origin.y - wr.top), w, h};
}

/// A child window's (control's) area as a mask rect of its top-level
/// window `top`.
inline std::optional<Rect> mask_rect_of_child(HWND top, HWND child) noexcept {
    RECT cr{};
    if (!GetWindowRect(child, &cr)) return std::nullopt;
    return mask_rect_from_screen(top, cr.left, cr.top, cr.right - cr.left, cr.bottom - cr.top);
}

}  // namespace gaiadesk::embed::win32

#endif  // GAIADESK_EMBED_WIN32_HPP
