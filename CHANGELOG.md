# Changelog

## 0.1.0

- `include/gaiadesk/embed.hpp`: a header-only C++17 layer over `libgaiadesk_embed` 0.1.0 —
  typed `Options` (capture, window, guided, fps, start-time `masks`), typed events
  (`std::variant`), an RAII move-only `Session` (exceptions, or `Result<T>` with the `try_` calls),
  optional delivery of events on your UI thread (`Post`), consent and build-info helpers.
- `embed_macos.hpp` (CGWindowID from an NSView/NSWindow, no Objective-C++ needed) and
  `embed_win32.hpp` (HWND window ids, mask rects from screen or client coordinates).
- CMake: `GaiaDesk::embed` and `gaiadesk_embed_deploy()`, via `FetchContent` of this repository
  (downloads the platform's release archive, SHA-256 pinned), `find_package(GaiaDeskEmbed)` on an
  unpacked archive or an installed copy, or the standalone `cmake/FetchGaiaDeskEmbed.cmake`.
- Release archives for macOS (universal), Windows x64 and Linux x86_64
  (`scripts/package-release.sh`).
- Examples: `c-minimal` (plain C) and `qt-helpdesk` (Qt 6 Widgets "Acme Desk").
