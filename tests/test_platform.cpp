// The platform helper headers compile with strict warnings and behave on
// bad input. (embed_macos.hpp on macOS, embed_win32.hpp on Windows.)
// SPDX-License-Identifier: MIT
#include <gaiadesk/embed.hpp>
#if defined(__APPLE__)
#include <gaiadesk/embed_macos.hpp>
#elif defined(_WIN32)
#include <gaiadesk/embed_win32.hpp>
#endif

#include "check.hpp"

#if defined(__APPLE__)
TEST(macos_nil_has_no_window_id) {
    CHECK(!gaiadesk::embed::macos::window_id_of_nsview(nullptr).has_value());
    CHECK(!gaiadesk::embed::macos::window_id_of_nswindow(nullptr).has_value());
}
#elif defined(_WIN32)
TEST(win32_helpers) {
    namespace w = gaiadesk::embed::win32;
    CHECK_EQ(w::window_id(reinterpret_cast<HWND>(static_cast<std::uintptr_t>(0x1234))), 0x1234ull);
    CHECK(!w::mask_rect_from_screen(nullptr, 0, 0, 1, 1).has_value());
    CHECK(!w::mask_rect_from_client(nullptr, 0, 0, 1, 1).has_value());
    // The desktop window: client area == window rect, so client coords pass through.
    auto r = w::mask_rect_from_client(GetDesktopWindow(), 10, 20, 30, 40);
    CHECK(r.has_value() && r->x == 10 && r->y == 20 && r->w == 30 && r->h == 40);
}
#else
TEST(no_platform_helpers_on_this_os) { CHECK(true); }
#endif

int main() { return check::run_all(); }
