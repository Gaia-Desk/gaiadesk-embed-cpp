# gaiadesk-embed-cpp

[![CI](https://github.com/Gaia-Desk/gaiadesk-embed-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/Gaia-Desk/gaiadesk-embed-cpp/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/Gaia-Desk/gaiadesk-embed-cpp)](https://github.com/Gaia-Desk/gaiadesk-embed-cpp/releases/latest)
[![License: MIT + binary licence](https://img.shields.io/badge/license-MIT%20%2B%20binary%20licence-blue)](#licence)

A **"Get help" button for your desktop app**, in C or C++ (Qt, Win32, Cocoa, GTK): in-app remote
support and a screen sharing SDK for C++ desktop apps on macOS, Windows and Linux, for customer
support with optional guided control. Your customer clicks it and agrees; **your app's window** is
shared with **your** support team, who see it in the GaiaDesk console's Support page and, if you
allow it, guide it: click and type **inside your app only**, while it is in front.

- The C API of the native library `libgaiadesk_embed` (`gaiadesk_embed.h`), and a **header-only
  C++17 wrapper** (`gaiadesk/embed.hpp`): typed options and events, an RAII session, exceptions or
  results. No dependencies besides the prebuilt library.
- **macOS** (universal, Apple silicon and Intel, 12.3+), **Windows x64**, **Linux x86_64** (X11;
  view-only).
- **Consent first**: the library refuses to start until your user has agreed. Use its native
  consent dialog or your own.
- Shares **your app's own window** by default. Other apps and the desktop are never in the picture
  unless you opt into `screen`.
- A visible **"Sharing with Acme · Stop sharing"** indicator, drawn by the library, for as long as
  anything is shared.
- **Your brand**: your name, colour and logo on the consent dialog and the indicator; nothing your
  customer sees names GaiaDesk.
- **Masks**: card numbers, passwords, anything, painted black before a frame is encoded.
- **View-only by default.** Guided input lands only in your app's own windows, and your customer
  can pause it.
- No install, no service, nothing written to disk: the session ends when your app stops it or quits.

## Install

### CMake, FetchContent (recommended)

```cmake
include(FetchContent)
FetchContent_Declare(gaiadesk_embed
  GIT_REPOSITORY https://github.com/Gaia-Desk/gaiadesk-embed-cpp.git
  GIT_TAG v0.3.0)
FetchContent_MakeAvailable(gaiadesk_embed)

target_link_libraries(my_app PRIVATE GaiaDesk::embed)
gaiadesk_embed_deploy(my_app)   # copies the library beside the executable (or into the .app) + RPATH
```

The C++ wrapper needs C++17 or later in your target (`set(CMAKE_CXX_STANDARD 17)` or
`target_compile_features(my_app PRIVATE cxx_std_17)`); the C header needs nothing.

At configure time this downloads the archive for your platform from this repository's release and
**checks its SHA-256 against the hash pinned in `cmake/FetchGaiaDeskEmbed.cmake`**; a mismatch is a
configure error and nothing is unpacked. `-DGAIADESK_EMBED_ROOT=<dir>` uses an unpacked archive you
already have instead; `-DGAIADESK_EMBED_BASE_URL=<url>` downloads from a mirror (a `file://` URL
works), still checked against the pinned hashes.

Prefer not to fetch the whole repository? Copy `cmake/FetchGaiaDeskEmbed.cmake` into your project:

```cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(FetchGaiaDeskEmbed)
gaiadesk_embed_fetch()          # or gaiadesk_embed_fetch(VERSION 0.3.0 SHA256 <hex>)
target_link_libraries(my_app PRIVATE GaiaDesk::embed)
gaiadesk_embed_deploy(my_app)
```

### Manual (release archive)

| Platform | Archive |
|---|---|
| macOS (universal) | `gaiadesk_embed-macos-universal.tar.gz` |
| Windows x64 | `gaiadesk_embed-win-x64.zip` |
| Linux x86_64 (glibc) | `gaiadesk_embed-linux-x64.tar.gz` |

Each has a `.sha256` beside it (`shasum -a 256 -c`). An archive is a complete SDK:

```
include/gaiadesk_embed.h            the C API
include/gaiadesk/embed.hpp          the C++ wrapper (+ embed_macos.hpp, embed_win32.hpp, detail/)
lib/libgaiadesk_embed.dylib | lib/libgaiadesk_embed.so | bin/gaiadesk_embed.dll + lib/gaiadesk_embed.dll.lib
lib/cmake/GaiaDeskEmbed/            find_package(GaiaDeskEmbed)
LICENSE, LICENSE-BINARY
```

With CMake: `cmake -DCMAKE_PREFIX_PATH=<unpacked dir>`, then

```cmake
find_package(GaiaDeskEmbed 0.1 REQUIRED)
target_link_libraries(my_app PRIVATE GaiaDesk::embed)
gaiadesk_embed_deploy(my_app)
```

Without CMake: add `<dir>/include` to the include path and link the library
(`-L<dir>/lib -lgaiadesk_embed`, or `gaiadesk_embed.dll.lib` with MSVC). Ship it beside your app:

- **macOS**: the dylib's install name is `@rpath/libgaiadesk_embed.dylib`. Put it in
  `YourApp.app/Contents/Frameworks/`, give the executable the RPATH `@executable_path/../Frameworks`,
  and sign it with your app. Your app needs the **Screen Recording** permission (macOS asks the user
  the first time).
- **Windows**: `gaiadesk_embed.dll` beside your `.exe`. It needs the Visual C++ runtime
  (`VCRUNTIME140.dll`), which most apps already ship.
- **Linux**: `libgaiadesk_embed.so` has no SONAME; link it by name (`-lgaiadesk_embed`, which CMake
  does) and put it on the RPATH (`$ORIGIN`). It needs `libgbm1`, `libwayland-client0` and `libxcb1`.

`cmake --install build` from a checkout installs the same layout.

## Quick start

Your backend creates a support session with a secret API key (scope `support`) and hands the app
its `embed_token`; the session's `mode` (`view` or `cobrowse`) is your backend's choice:

```sh
curl -s -X POST https://api.gaiadesk.net/v1/support/sessions \
  -H "Authorization: Bearer $GAIADESK_API_KEY" -H "Content-Type: application/json" \
  -d '{"mode": "view", "customer": {"name": "Ada", "plan": "pro"}}'
# → {"id": "ss_…", "join_code": "123456789", "embed_token": "gdemb_…", …}
```

Never ship the secret key in the app: the embed token is all the app needs.

### C++

```cpp
#include <gaiadesk/embed.hpp>
namespace ge = gaiadesk::embed;

ge::Session help;   // a member of your window: ending it (or destroying it) stops sharing

void MyWindow::getHelp(const std::string& token) {
    // 1. Ask. The library's dialog (macOS, Windows; on the UI thread), or yours (Linux).
    std::optional<bool> agreed = ge::request_consent("Acme");
    if (!agreed) agreed = askWithMyOwnDialog();
    if (!*agreed) return;

    // 2. Start with the answer and the masks you already know.
    ge::Options o;
    o.embed_token = token;
    o.company = "Acme";
    o.window_id = myWindowId();                 // see "Capture"
    o.masks = {cardNumberRect()};               // see "Masking"
    try {
        help = ge::Session::start(o, /*consent_granted=*/*agreed,
            [this](const ge::Event& e) {
                if (auto c = std::get_if<ge::CodeEvent>(&e)) showStatus("Support code " + c->code);
                if (auto j = std::get_if<ge::JoinedEvent>(&e)) showStatus(j->agent.value_or("An agent") + " is viewing");
                if (std::holds_alternative<ge::EndedEvent>(e)) help = ge::Session();
            },
            // 3. Run the handler on your UI thread (here: Qt).
            [this](std::function<void()> f) { QMetaObject::invokeMethod(this, std::move(f), Qt::QueuedConnection); });
    } catch (const ge::Error& e) {
        showStatus("Could not start: " + e.message() + " (" + e.code() + ")");
    }
}
```

No exceptions? Use `Session::try_start` (and `try_set_mask_rects`), which return a `Result<T>`:
`if (!r) r.error().code …; else help = std::move(r).value();`. The throwing calls are left out when
exceptions are disabled.

### C

```c
#include <gaiadesk_embed.h>

static void on_event(void *user_data, const char *event_json) {
    /* On the library's threads: hand it to your UI thread, don't block. */
    printf("%s\n", event_json);
}

int agreed = gd_embed_show_consent("Acme", "window", 0);   /* 1, 0, -1 no dialog, -2 wrong thread */
if (agreed < 0) agreed = ask_with_my_own_dialog();
if (!agreed) return;

GdEmbedCallbacks cb = { on_event, NULL };
GdEmbed *help = gd_embed_start(
    "{\"embed_token\":\"gdemb_…\",\"consent_granted\":true,\"company\":\"Acme\","
    "\"masks\":[{\"x\":20,\"y\":80,\"w\":300,\"h\":40}]}", cb);
if (!help) fprintf(stderr, "refused: %s\n", gd_embed_last_error());   /* {"code":…,"message":…} */
/* … later … */
gd_embed_stop(help);
```

**macOS: the app's main run loop must be running** while a session is up (the indicator lives
there), as it is in any Cocoa, Qt or other GUI app.

Your support team opens **Support** in the GaiaDesk console, sees the waiting customer and joins
(or types the nine-digit code).

## Consent

`Session::start` / `gd_embed_start` take `consent_granted`: your user's answer to a consent dialog.
The library refuses to start without it (`consent_required`), before it looks at anything else; in
C the JSON value must be literally `true`. Use the library's dialog:

| | macOS | Windows | Linux |
|---|---|---|---|
| `show_consent` / `gd_embed_show_consent` | native, modal; **main thread only** (else `wrong_thread` / -2) | `MessageBox` | none: `no_native_dialog` / -1 |

`request_consent` folds that into `true` / `false`, or `nullopt` when you must ask yourself. Pass
the same `capture` and `guided` you will start with, so the dialog says what the user agrees to.
Your own dialog should say who will see what, and whether they can act. Never pass `true` without
asking.

## Options

| `Options` (C++) | JSON key (C) | |
|---|---|---|
| `embed_token` | `embed_token` | Required. `gdemb_` + 64 hex, from your backend. |
| (argument) | `consent_granted` | Required, literally `true`. |
| `server` | `server` | Default `https://gaiadesk.net` (`https://` only; `http://` for localhost). |
| `company` | `company` | On the dialog and the indicator (one line, at most 48 characters; default "Support"). |
| `capture` | `capture` | `window` (default), `app`, `screen`. See Capture. |
| `window_id` | `window_id` | A window of **this** app. See Capture. |
| `guided` | `guided` | Allow guided input (default `false`). See Guided mode. |
| `fps` | `fps` | 1 to 30 (default 15). |
| `masks` | `masks` | `[{x,y,w,h}]`, painted out from the first frame. See Masking. |
| `brand_color`, `logo_png_base64` | same | Your colour and logo (0.2.0). See Branding. |
| `parent_window` | `parent_window` | The calling window, NSWindow* / HWND (as an integer in C); default its focused window (0.3.0). See The indicator. |
| `indicator_position` | `indicator_position` | `top-left`, `top-right` (default), `bottom-left`, `bottom-right` (0.3.0). |
| `indicator_origin` | `indicator_origin` | `{x,y}` from an `indicator_moved` event you kept (0.3.0). |

Unknown keys are refused (`bad_config`): there is no switch that turns the indicator or consent
off. `config_json(options, consent)` shows you exactly what the wrapper sends.

## Branding

Your customer sees your brand, not ours: `company` on the consent dialog and the indicator, plus
(library 0.2.0) your colour and logo.

```cpp
ge::ConsentOptions c{"Acme", ge::Capture::window, false, "#0f766e", logo_base64};
if (ge::show_consent(c) != ge::Consent::agreed) return;  // throws Error("bad_config") on a bad colour or logo

ge::Options o;
o.embed_token = token;
o.company = "Acme";
o.brand_color = "#0f766e";          // #RRGGBB: the indicator's stripe and Stop button
o.logo_png_base64 = logo_base64;    // a PNG, at most 256 KiB, shown small beside the text
```

From C: `gd_embed_show_consent_ex("{\"company\":\"Acme\",\"brand_color\":\"#0f766e\"}")` and the same
`brand_color` / `logo_png_base64` keys in the `gd_embed_start` config. Linux has no native consent
dialog (`-1`): show your own, in your look.

## The indicator

Library 0.3.0 places the consent dialog and the indicator by the window that asked for help.

```cpp
void* win = reinterpret_cast<void*>(widget->window()->winId());  // HWND; on macOS an NSView*, so:
// win = ge::macos::nswindow_of_nsview(win);   (gaiadesk/embed_macos.hpp)

ge::ConsentOptions c{"Acme"};
c.masked = true;          // the dialog says private fields stay hidden (you pass masks)
c.parent_window = win;    // a sheet on it (macOS) / owned by it (Windows), never behind it

ge::Options o;
o.parent_window = win;                                     // the indicator keeps clear of it
o.indicator_position = ge::IndicatorPosition::bottom_right;
if (saved) o.indicator_origin = ge::Point{saved->x, saved->y};  // where the customer left it
```

When the customer drags the indicator you get `IndicatorMovedEvent{x, y}`: keep it (the library
writes nothing to disk) and pass it back as `indicator_origin` next time; it is used while it still
fits on a screen. Cancel on the indicator before an agent is admitted ends the session with
`customer_cancelled`, and the request leaves your support queue at once.

## Capture

| `capture` | macOS | Windows | Linux |
|---|---|---|---|
| `window` (default) | `window_id`, or your frontmost window | `window_id`, or your frontmost window | `window_id`, or your frontmost window |
| `app` | every window of your app | your focused window, followed as focus moves | same as Windows |
| `screen` (opt-in) | the display your window is on | the monitor your window is on | the primary screen (X11 only) |

`window_id` must be a window of **your own process**; another app's window is refused when capture
starts. What to pass:

| | `window_id` | Helper |
|---|---|---|
| macOS | the `CGWindowID` (`NSWindow.windowNumber`) | `gaiadesk::embed::macos::window_id_of_nsview(view)` / `window_id_of_nswindow(win)` (`<gaiadesk/embed_macos.hpp>`, plain C++; Qt: `winId()` is the NSView*) |
| Windows | the `HWND` | `gaiadesk::embed::win32::window_id(hwnd)` (`<gaiadesk/embed_win32.hpp>`) |
| Linux | the X11 window id | Qt on xcb: `winId()` |

## Masking

Pass the regions you already know in `masks` when you start: an agent can join at once, and only
start-time masks are guaranteed to cover the first frame. Afterwards `set_mask_rects`
(`gd_embed_set_mask_rects`) replaces the whole set; an empty list clears it. Send them again
whenever your layout changes, the window is resized, or it moves to another display. At most 64
rects; sizes must be finite and non-negative (else `bad_config` at start, `bad_rects` afterwards).
Masks **fail closed**: if the window cannot be located, the whole picture is painted out.

Rects are relative to the **top-left corner of the shared window's frame** (in `app`/`screen` mode,
the window you named or your frontmost one), in these units:

| | Units | Origin | From a widget |
|---|---|---|---|
| **macOS** | points | the window **frame** (title bar included) | content-view coordinates + the content area's offset in the frame (Qt: `geometry().topLeft() - frameGeometry().topLeft()`) |
| **Windows** | `GetWindowRect` units: physical pixels for a per-monitor DPI aware app (Qt 6, Electron, WinForms with PerMonitorV2) | the `GetWindowRect` top-left (title bar and borders included) | `win32::mask_rect_from_client(hwnd, …)` for client coordinates (Qt: logical × `devicePixelRatioF()`), `mask_rect_from_screen` for screen ones, `mask_rect_of_child(top, child)` for a child HWND |
| **Linux** | X11 pixels | your X11 window | logical × `devicePixelRatioF()` |

`examples/qt-helpdesk` (`cardMaskRect()`) does all three.

## Guided mode

Only when your app passes `guided = true` **and** your backend created the session as `cobrowse`.
Then the agent's clicks, scrolling, typing and a few keys are delivered to your app's own windows,
inside your process (never through the system's input stream), and only while your app is in
front, not under a mask, at most 20 a second. Your customer pauses it with **Pause control** on the
indicator; you can with `set_paused(true)`. Every request is reported as an `ActionEvent` with
`applied` and a `reason` (`view_only`, `control_paused`, `rate_limited`, `not_focused`,
`outside_app`, `masked`, `unsupported`). **Linux is view-only** in this release: X11 has no way to
deliver input to one app only.

## Events

| C++ type | JSON (C) | |
|---|---|---|
| `StateEvent{state}` | `{"type":"state","state":…}` | `starting`, `waiting`, `connected`, `ended` |
| `CodeEvent{code, session_id, mode, expires_at}` | `{"type":"code",…}` | The session is up; `code` is the nine-digit support code; `mode` `view` or `cobrowse`; `expires_at` Unix seconds |
| `JoinedEvent{agent}` | `{"type":"joined","agent":…}` | An agent joined (their email, or none) |
| `LeftEvent` | `{"type":"left"}` | The agent left |
| `ControlEvent{allowed}` | `{"type":"control","allowed":…}` | Guided input paused (`false`) or allowed again |
| `ActionEvent{action, applied, reason}` | `{"type":"action",…}` | A guided request and its outcome |
| `ErrorEvent{code, message}` | `{"type":"error",…}` | e.g. `capture_failed`, `connect_failed`, `indicator_unavailable` |
| `IndicatorMovedEvent{x, y}` | `{"type":"indicator_moved","x":…,"y":…}` | The customer dragged the indicator: keep it for `indicator_origin` (0.3.0) |
| `EndedEvent{reason}` | `{"type":"ended","reason":…}` | `stopped` (the customer), `app_stopped`, `customer_cancelled` (cancelled before an agent was admitted, 0.3.0), `disconnected`, `expired`, `error`. **Always last, exactly once** |
| `UnknownEvent{type, json}` | | A type newer than this wrapper |

`parse_event(json)` turns the C callback's JSON into an `Event`; `type_of(e)` gives its type name.

Refusal codes (`Error::code()`, `Result::error().code`, `gd_embed_last_error()`):
`consent_required`, `bad_token`, `bad_server`, `bad_config`, `bad_rects`, `bad_argument`.
`gd_embed_last_error()` is **per thread**: read it right after the call that failed.

## Threading

- The event callback runs on the **library's own threads** (and, for events the indicator causes,
  the main thread). Don't block in it and don't touch UI there. In C++, pass a `Post` function to
  `Session::start` and every event is handed to it (Qt: `QMetaObject::invokeMethod(obj, f,
  Qt::QueuedConnection)`; Win32: `PostMessage`; GLib: `g_idle_add`), so your handler runs on your UI
  thread.
- Events can arrive before `start` returns. `EndedEvent` is the last call.
- Every `Session` member is thread-safe and may be called from an event handler. `stop()` waits for
  the session to wind down, except when called from inside an event handler on the library's
  thread. Destroying a `Session` stops it.
- An exception thrown by your handler is caught and dropped by the wrapper: it never unwinds into
  the C library.
- `show_consent` is modal: call it on your UI thread (on macOS, the main thread).

## Examples

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qtbase)"   # Qt only for qt-helpdesk
cmake --build build
```

- [`examples/c-minimal`](examples/c-minimal/main.c): the C calls in order: consent, start with
  masks, events until `ended` or Ctrl+C, stop. A console program has no window of its own, so a
  real session needs a GUI app; this one shows the calls.
  `GAIADESK_EMBED_TOKEN=gdemb_… build/examples/c-minimal/c-minimal`
- [`examples/qt-helpdesk`](examples/qt-helpdesk): "Acme Desk", a Qt 6 Widgets app: Get help (the
  library's consent dialog, or a `QMessageBox` on Linux), a red "card" widget masked from the first
  frame and re-masked on resize, a Buy button for guided mode, Pause control, Stop sharing, and the
  status from events. `GAIADESK_EMBED_TOKEN=gdemb_… build/examples/qt-helpdesk/qt-helpdesk.app/Contents/MacOS/qt-helpdesk`
  (macOS; `build/examples/qt-helpdesk/qt-helpdesk` elsewhere; Linux under X11 or `QT_QPA_PLATFORM=xcb`).

Both read `GAIADESK_SERVER` and `GAIADESK_DEMO_GUIDED=1`; the Qt app also reads
`GAIADESK_DEMO_AUTOSTART=1`. For automated tests only, `GAIADESK_DEMO_TEST_CONSENT=1` skips the
consent dialog (never do that in an app) and `GAIADESK_DEMO_EXIT_AFTER_MS` quits the Qt app.

## Development

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

- Unit tests (label `unit`): the JSON reader and writer, config generation (masks included), event
  parsing and delivery, the platform helpers.
- Live tests (label `live`) run against the real library and only check refusals it makes before
  touching the network or the screen (`consent_required`, `bad_token`, `bad_config` for a bad mask,
  `bad_server`, …), and that `gd_embed_build_info` reports a release build (`test_source: false`).
  The examples' smoke tests check that a bad token is refused.
- The C++ wrapper hand-parses JSON (`include/gaiadesk/detail/json.hpp`, ~300 lines, strict,
  locale-independent) rather than vendoring a JSON library: a header-only wrapper would otherwise
  put a second copy of, say, nlohmann/json (possibly another version) into your translation units
  beside your own.

### Releasing

`scripts/package-release.sh <embed-out-dir>` builds `dist/` from the library builds (`mac/`,
`windows/`, `linux/`): the three archives, a `.sha256` for each and `SHA256SUMS`, and pins the new
hashes in `cmake/FetchGaiaDeskEmbed.cmake`. It refuses headers that differ between platforms, a
test-source build, a binary of the wrong OS or CPU, and a macOS library that links libvpx or lacks a
slice. The archives are reproducible. Commit the pinned hashes, tag `v<version>`, and attach
everything in `dist/` to the GitHub release; CI builds against those assets.

## Links

- Releases: [prebuilt archives for macOS, Windows and Linux](https://github.com/Gaia-Desk/gaiadesk-embed-cpp/releases/latest) (CMake `FetchContent` downloads them for you)
- Guide: [Embedding GaiaDesk](https://gaiadesk.net/docs/embedding-gaiadesk) on gaiadesk.net
- Embed SDKs: [Web](https://github.com/Gaia-Desk/gaiadesk-embed) · [Electron](https://github.com/Gaia-Desk/gaiadesk-embed-electron) · [macOS (Swift)](https://github.com/Gaia-Desk/gaiadesk-embed-swift) · [C/C++ and Qt](https://github.com/Gaia-Desk/gaiadesk-embed-cpp) · [.NET](https://github.com/Gaia-Desk/gaiadesk-embed-dotnet) · [iOS](https://github.com/Gaia-Desk/gaiadesk-embed-ios) · [Android](https://github.com/Gaia-Desk/gaiadesk-embed-android) · [React Native](https://github.com/Gaia-Desk/gaiadesk-embed-react-native) · [Flutter](https://github.com/Gaia-Desk/gaiadesk-embed-flutter)
- [Changelog](CHANGELOG.md) · [Security policy](https://github.com/Gaia-Desk/.github/blob/main/SECURITY.md)

## Licence

The C++ headers, CMake files, scripts and examples here are MIT ([LICENSE](LICENSE)). The prebuilt
`libgaiadesk_embed` binary (and its C header) is licensed under the GaiaDesk Native Binary Licence
([LICENSE-BINARY](LICENSE-BINARY)): free to use in your apps, linked into them, unmodified.
