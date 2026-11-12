// gaiadesk-embed-cpp, macOS helpers: the CGWindowID `Options::window_id`
// wants (and the NSWindow* `parent_window` wants), from the NSView or NSWindow pointer a C++ toolkit hands you (Qt's
// `QWidget::winId()` is an NSView*). Plain C++: it talks to the Objective-C
// runtime directly, so no .mm file is needed. Link libobjc (the
// GaiaDesk::embed CMake target does).
//
// Mask rects on macOS are in points, relative to the top-left of the
// window's FRAME (title bar included): add the content area's offset in the
// frame (in Qt: `geometry().topLeft() - frameGeometry().topLeft()`) to
// coordinates in the content view.
//
// SPDX-License-Identifier: MIT
#ifndef GAIADESK_EMBED_MACOS_HPP
#define GAIADESK_EMBED_MACOS_HPP

#ifndef __APPLE__
#error "gaiadesk/embed_macos.hpp is for macOS"
#endif

#include <objc/message.h>
#include <objc/runtime.h>

#include <cstdint>
#include <optional>

namespace gaiadesk::embed::macos {

/// An NSWindow* as `Options::window_id` (its `windowNumber`, which is the
/// CGWindowID); nullopt for nil or a window that is not on screen yet.
inline std::optional<std::uint64_t> window_id_of_nswindow(void* nswindow) noexcept {
    if (nswindow == nullptr) return std::nullopt;
    using NumberFn = long (*)(void*, SEL);
    long n = reinterpret_cast<NumberFn>(objc_msgSend)(nswindow, sel_registerName("windowNumber"));
    if (n <= 0) return std::nullopt;
    return static_cast<std::uint64_t>(n);
}

/// An NSView*'s NSWindow*, as `Options::parent_window` /
/// `ConsentOptions::parent_window` want it (Qt:
/// `nswindow_of_nsview(reinterpret_cast<void*>(widget->window()->winId()))`);
/// nullptr for nil or a view not in a window.
inline void* nswindow_of_nsview(void* nsview) noexcept {
    if (nsview == nullptr) return nullptr;
    using WindowFn = void* (*)(void*, SEL);
    return reinterpret_cast<WindowFn>(objc_msgSend)(nsview, sel_registerName("window"));
}

/// An NSView*'s window as `Options::window_id` (Qt:
/// `window_id_of_nsview(reinterpret_cast<void*>(widget->window()->winId()))`).
inline std::optional<std::uint64_t> window_id_of_nsview(void* nsview) noexcept {
    return window_id_of_nswindow(nswindow_of_nsview(nsview));
}

}  // namespace gaiadesk::embed::macos

#endif  // GAIADESK_EMBED_MACOS_HPP
