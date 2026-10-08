// gaiadesk-embed-cpp: a "Get help" button for your desktop app, in C++17.
//
// A header-only layer over libgaiadesk_embed's C API (gaiadesk_embed.h):
// typed options and events, an RAII session, exceptions or results.
//
// What the library itself enforces (no wrapper can skip it): it refuses to
// start without consent; its own "Sharing with <company>" indicator is up for
// as long as anything is shared; only this app's window is captured unless
// you ask for the screen; the rects you mask are painted out of every frame;
// input is off unless you allow `guided` AND the session is "cobrowse", and
// then lands only in this app's own windows while it is in front.
//
// Threading: events are delivered on the library's own threads (and, for
// events the indicator causes, the main thread), unless you pass a `Post`
// function to `Session::start` that moves them to your UI thread. Every
// `Session` member is safe to call from any thread, including from inside
// an event handler.
//
// SPDX-License-Identifier: MIT
#ifndef GAIADESK_EMBED_HPP
#define GAIADESK_EMBED_HPP

#if defined(_MSVC_LANG) ? (_MSVC_LANG < 201703L) : (__cplusplus < 201703L)
#error "gaiadesk/embed.hpp needs C++17 or later"
#endif

#include <gaiadesk_embed.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "detail/json.hpp"

#if defined(__cpp_exceptions) || defined(_CPPUNWIND) || defined(__EXCEPTIONS)
#define GAIADESK_EMBED_EXCEPTIONS 1
#else
#define GAIADESK_EMBED_EXCEPTIONS 0
#endif

/// This wrapper's version (the native library has its own: `version()`).
#define GAIADESK_EMBED_CPP_VERSION "0.1.0"

namespace gaiadesk::embed {

// ---------------------------------------------------------------- options --

/// What is shared.
enum class Capture {
    window,  ///< One window of this app (the default).
    app,     ///< Every window of this app (macOS; elsewhere its focused window, followed).
    screen,  ///< The whole display this app's window is on. Opt-in.
};

inline const char* to_string(Capture c) noexcept {
    switch (c) {
        case Capture::app: return "app";
        case Capture::screen: return "screen";
        case Capture::window: break;
    }
    return "window";
}

/// A masked region, relative to the top-left corner of the shared window's
/// frame: points on macOS (title bar included), `GetWindowRect` units on
/// Windows (physical pixels for a per-monitor DPI aware app), X11 pixels on
/// Linux. See the README's "Masking" section, and embed_win32.hpp.
struct Rect {
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;
};

/// How a support session starts. Only `embed_token` is required.
struct Options {
    /// `gdemb_…`, from your backend (`POST /v1/support/sessions`).
    std::string embed_token;
    /// The GaiaDesk server origin; empty means the library's default,
    /// `https://gaiadesk.net` (`http://` only for localhost).
    std::string server;
    /// Your company's name, on the consent dialog and the indicator; empty
    /// means "Support".
    std::string company;
    /// What is shared; default one window of this app.
    Capture capture = Capture::window;
    /// A window of THIS app (CGWindowID on macOS, HWND on Windows, X11
    /// window id on Linux); default: the app's frontmost. See
    /// embed_macos.hpp / embed_win32.hpp for converting your handle.
    std::optional<std::uint64_t> window_id;
    /// Allow guided input (the session must also be "cobrowse").
    bool guided = false;
    /// Frames per second, 1 to 30; default (nullopt) the library's 15.
    std::optional<int> fps;
    /// Regions painted out from the very first frame. Pass the ones you
    /// already know here: an agent can join straight away, and only
    /// start-time masks are guaranteed to cover the first frame.
    std::vector<Rect> masks;
};

/// The library's config JSON for `options`. Keys are exactly the ones it
/// accepts (it refuses unknown keys); empty or unset optional ones are left
/// out. Pure.
inline std::string config_json(const Options& o, bool consent_granted) {
    namespace j = detail::json;
    std::string s = "{\"embed_token\":" + j::quote(o.embed_token);
    s += ",\"consent_granted\":";
    s += consent_granted ? "true" : "false";
    if (!o.server.empty()) s += ",\"server\":" + j::quote(o.server);
    if (!o.company.empty()) s += ",\"company\":" + j::quote(o.company);
    s += ",\"capture\":" + j::quote(to_string(o.capture));
    if (o.window_id) s += ",\"window_id\":" + std::to_string(*o.window_id);
    s += ",\"guided\":";
    s += o.guided ? "true" : "false";
    if (o.fps) s += ",\"fps\":" + std::to_string(*o.fps);
    if (!o.masks.empty()) {
        s += ",\"masks\":[";
        for (std::size_t k = 0; k < o.masks.size(); ++k) {
            const Rect& r = o.masks[k];
            if (k) s += ',';
            s += "{\"x\":" + j::number(r.x) + ",\"y\":" + j::number(r.y) + ",\"w\":" + j::number(r.w) +
                 ",\"h\":" + j::number(r.h) + '}';
        }
        s += ']';
    }
    s += '}';
    return s;
}

// ----------------------------------------------------------------- events --

/// `starting`, `waiting` (registered, no agent yet), `connected`, `ended`.
struct StateEvent {
    std::string state;
};
/// The session is up: `code` is the nine-digit support code your customer
/// reads to the agent; `mode` is `view` or `cobrowse` (your backend's choice).
struct CodeEvent {
    std::string code;
    std::string session_id;
    std::string mode;
    std::optional<std::int64_t> expires_at;  ///< Unix seconds.
};
/// An agent joined (their email, when known).
struct JoinedEvent {
    std::optional<std::string> agent;
};
/// The agent left.
struct LeftEvent {};
/// Whether guided input is allowed right now (paused / resumed).
struct ControlEvent {
    bool allowed = false;
};
/// A guided request and what became of it. `reason` when not applied:
/// `view_only`, `control_paused`, `rate_limited`, `not_focused`,
/// `outside_app`, `masked`, `unsupported`, …
struct ActionEvent {
    std::string action;
    bool applied = false;
    std::optional<std::string> reason;
};
/// A problem during the session (`capture_failed`, `connect_failed`, …).
struct ErrorEvent {
    std::string code;
    std::string message;
};
/// Always the last event, exactly once: `stopped` (the customer),
/// `app_stopped`, `disconnected`, `expired`, `error`.
struct EndedEvent {
    std::string reason;
};
/// An event type newer than this wrapper: its `type` and the raw JSON.
struct UnknownEvent {
    std::string type;
    std::string json;
};

using Event = std::variant<StateEvent, CodeEvent, JoinedEvent, LeftEvent, ControlEvent, ActionEvent, ErrorEvent,
                           EndedEvent, UnknownEvent>;

/// One event's JSON as an `Event`; nullopt for JSON that is not an event
/// object. Unknown types become `UnknownEvent`. Pure.
inline std::optional<Event> parse_event(std::string_view json) {
    auto doc = detail::json::parse(json);
    if (!doc || !doc->is_object()) return std::nullopt;
    auto type = doc->string_at("type");
    if (!type) return std::nullopt;
    const auto& o = *doc;
    auto str = [&](std::string_view k) { return o.string_at(k).value_or(std::string()); };
    if (*type == "state") return Event{StateEvent{str("state")}};
    if (*type == "code")
        return Event{CodeEvent{str("code"), str("session_id"), o.string_at("mode").value_or("view"), o.int64_at("expires_at")}};
    if (*type == "joined") return Event{JoinedEvent{o.string_at("agent")}};
    if (*type == "left") return Event{LeftEvent{}};
    if (*type == "control") return Event{ControlEvent{o.true_at("allowed")}};
    if (*type == "action") return Event{ActionEvent{str("action"), o.true_at("applied"), o.string_at("reason")}};
    if (*type == "error") return Event{ErrorEvent{str("code"), str("message")}};
    if (*type == "ended") return Event{EndedEvent{str("reason")}};
    return Event{UnknownEvent{*type, std::string(json)}};
}

/// The event's `type` as the library sent it ("state", "code", …).
inline std::string_view type_of(const Event& e) noexcept {
    switch (e.index()) {
        case 0: return "state";
        case 1: return "code";
        case 2: return "joined";
        case 3: return "left";
        case 4: return "control";
        case 5: return "action";
        case 6: return "error";
        case 7: return "ended";
        default: return std::get_if<UnknownEvent>(&e) ? std::string_view(std::get<UnknownEvent>(e).type) : "";
    }
}

// ----------------------------------------------------------------- errors --

/// Why the library refused. `code` is stable: `consent_required`,
/// `bad_token`, `bad_config`, `bad_server`, `bad_rects`, `bad_argument`, …
struct ErrorInfo {
    std::string code;
    std::string message;
};

/// The last refusal on THIS thread (read it right after the failing call).
inline ErrorInfo last_error() {
    const char* raw = gd_embed_last_error();
    if (raw == nullptr || *raw == '\0') return {"unknown", "the library gave no reason"};
    auto doc = detail::json::parse(raw);
    if (!doc || !doc->is_object()) return {"unknown", raw};
    return {doc->string_at("code").value_or("unknown"), doc->string_at("message").value_or(std::string())};
}

/// A refusal, thrown by the throwing API.
class Error : public std::runtime_error {
public:
    explicit Error(ErrorInfo info)
        : std::runtime_error(info.code + ": " + info.message), info_(std::move(info)) {}
    const std::string& code() const noexcept { return info_.code; }
    const std::string& message() const noexcept { return info_.message; }
    const ErrorInfo& info() const noexcept { return info_; }

private:
    ErrorInfo info_;
};

/// A value or the reason there is none (like C++23's std::expected).
template <class T>
class [[nodiscard]] Result {
public:
    Result(T value) : v_(std::in_place_index<0>, std::move(value)) {}
    Result(ErrorInfo error) : v_(std::in_place_index<1>, std::move(error)) {}
    bool ok() const noexcept { return v_.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }
    T& value() & { return std::get<0>(v_); }
    T&& value() && { return std::get<0>(std::move(v_)); }
    T* operator->() { return &std::get<0>(v_); }
    T& operator*() & { return std::get<0>(v_); }
    const ErrorInfo& error() const { return std::get<1>(v_); }

private:
    std::variant<T, ErrorInfo> v_;
};

template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(ErrorInfo error) : e_(std::move(error)) {}
    bool ok() const noexcept { return !e_.has_value(); }
    explicit operator bool() const noexcept { return ok(); }
    const ErrorInfo& error() const { return *e_; }

private:
    std::optional<ErrorInfo> e_;
};

// ------------------------------------------------------- library, consent --

/// The native library's version ("0.1.0").
inline std::string version() {
    const char* v = gd_embed_version();
    return v ? v : "";
}

/// What the loaded native binary is.
struct BuildInfo {
    std::string version;
    bool test_source = false;  ///< A test build: never ship one.
    std::string codec;
    std::string os;
    std::string arch;
    std::string json;  ///< As the library reported it.
};

inline BuildInfo build_info() {
    const char* raw = gd_embed_build_info();
    BuildInfo b;
    b.json = raw ? raw : "";
    if (auto doc = detail::json::parse(b.json); doc && doc->is_object()) {
        b.version = doc->string_at("version").value_or("");
        b.test_source = doc->true_at("test_source");
        b.codec = doc->string_at("codec").value_or("");
        b.os = doc->string_at("os").value_or("");
        b.arch = doc->string_at("arch").value_or("");
    }
    return b;
}

/// The default consent dialog's answer.
enum class Consent {
    agreed,
    declined,
    no_native_dialog,  ///< Linux: show your own.
    wrong_thread,      ///< macOS: call it on the main thread.
};

/// The library's consent dialog (modal; call it on your UI / main thread).
/// Pass the same `capture` and `guided` you will start with, so it tells
/// your user what they agree to.
inline Consent show_consent(const std::string& company, Capture capture = Capture::window, bool guided = false) {
    switch (gd_embed_show_consent(company.c_str(), to_string(capture), guided ? 1 : 0)) {
        case 1: return Consent::agreed;
        case 0: return Consent::declined;
        case -2: return Consent::wrong_thread;
        default: return Consent::no_native_dialog;
    }
}

/// `show_consent` as true / false, or nullopt when there is no native dialog
/// here (or it was the wrong thread): then ask with your own and pass its
/// answer to `Session::start`.
inline std::optional<bool> request_consent(const std::string& company, Capture capture = Capture::window,
                                           bool guided = false) {
    switch (show_consent(company, capture, guided)) {
        case Consent::agreed: return true;
        case Consent::declined: return false;
        default: return std::nullopt;
    }
}

// ---------------------------------------------------------------- session --

/// Called for every event. Do not block: on the library's threads, a slow
/// handler delays the session.
using EventHandler = std::function<void(const Event&)>;
/// Runs a function on your UI thread (e.g. Qt's
/// `QMetaObject::invokeMethod(obj, std::move(f), Qt::QueuedConnection)`).
using Post = std::function<void(std::function<void()>)>;

/// One support session: start it with `Session::start`; it ends with
/// `stop()` or when the object is destroyed (RAII). Move-only.
class Session {
public:
    Session() noexcept = default;
    Session(Session&& other) noexcept : state_(std::move(other.state_)) {}
    Session& operator=(Session&& other) noexcept {
        if (this != &other) {
            stop();
            state_ = std::move(other.state_);
        }
        return *this;
    }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() { stop(); }

    /// Start sharing, or the library's refusal. `consent_granted` must be
    /// your user's answer to a consent dialog (`request_consent` or yours):
    /// the library refuses without it (`consent_required`). Events may start
    /// arriving before this returns. With `post`, every event is handed to
    /// it (to run your handler on your UI thread); without, the handler runs
    /// on the library's threads.
    static Result<Session> try_start(const Options& options, bool consent_granted, EventHandler on_event,
                                     Post post = {}) {
        auto state = std::make_shared<State>();
        state->handler = std::move(on_event);
        state->post = std::move(post);
        // The library's user_data: one strong reference, released on the
        // "ended" event (its last call) or right here if it refuses.
        auto* keep = new std::shared_ptr<State>(state);
        const std::string json = config_json(options, consent_granted);
        GdEmbed* h = gd_embed_start(json.c_str(), GdEmbedCallbacks{&Session::trampoline, keep});
        if (h == nullptr) {
            ErrorInfo why = last_error();  // thread-local: read it now
            delete keep;                   // refused: the library keeps no reference
            return why;
        }
        {
            std::lock_guard<std::mutex> lock(state->m);
            state->handle = h;
        }
        Session s;
        s.state_ = std::move(state);
        return Result<Session>(std::move(s));
    }

#if GAIADESK_EMBED_EXCEPTIONS
    /// `try_start`, throwing `Error` on a refusal.
    static Session start(const Options& options, bool consent_granted, EventHandler on_event, Post post = {}) {
        auto r = try_start(options, consent_granted, std::move(on_event), std::move(post));
        if (!r) throw Error(r.error());
        return std::move(r).value();
    }
#endif

    /// End the session and free the library's handle; `EndedEvent`
    /// (`app_stopped`) follows unless it already ended. Safe to call more
    /// than once, from any thread and from an event handler. Waits for the
    /// session to wind down, except from inside an event handler.
    void stop() noexcept {
        if (!state_) return;
        GdEmbed* h = nullptr;
        {
            std::lock_guard<std::mutex> lock(state_->m);
            std::swap(h, state_->handle);
        }
        if (h != nullptr) gd_embed_stop(h);
    }

    /// Replace the masked rects (see `Rect` for their units); an empty list
    /// clears them. Call again when your layout changes. Does nothing once
    /// stopped.
    Result<void> try_set_mask_rects(const std::vector<Rect>& rects) {
        static_assert(sizeof(Rect) == sizeof(GdRect) && std::is_standard_layout<Rect>::value,
                      "Rect must match GdRect");
        if (!state_) return {};
        std::vector<GdRect> list;
        list.reserve(rects.size());
        for (const Rect& r : rects) list.push_back(GdRect{r.x, r.y, r.w, r.h});
        std::lock_guard<std::mutex> lock(state_->m);
        if (state_->handle == nullptr) return {};
        if (gd_embed_set_mask_rects(state_->handle, list.empty() ? nullptr : list.data(), list.size()) != 0)
            return last_error();
        return {};
    }

#if GAIADESK_EMBED_EXCEPTIONS
    /// `try_set_mask_rects`, throwing `Error` (`bad_rects`) on a refusal.
    void set_mask_rects(const std::vector<Rect>& rects) {
        auto r = try_set_mask_rects(rects);
        if (!r) throw Error(r.error());
    }
#endif

    /// Pause (or resume) guided input, as the indicator's own button does.
    void set_paused(bool paused) noexcept {
        if (!state_) return;
        std::lock_guard<std::mutex> lock(state_->m);
        if (state_->handle != nullptr) (void)gd_embed_set_paused(state_->handle, paused ? 1 : 0);
    }

    /// Whether this object holds a session that `stop()` has not ended.
    bool active() const noexcept {
        if (!state_) return false;
        std::lock_guard<std::mutex> lock(state_->m);
        return state_->handle != nullptr;
    }
    explicit operator bool() const noexcept { return active(); }

    /// Whether the library has sent `EndedEvent`.
    bool has_ended() const noexcept { return state_ && state_->ended.load(std::memory_order_acquire); }

    /// The library's handle, for calls this wrapper does not cover (do not
    /// pass it to gd_embed_stop: `stop()` does).
    GdEmbed* native_handle() const noexcept {
        if (!state_) return nullptr;
        std::lock_guard<std::mutex> lock(state_->m);
        return state_->handle;
    }

    /// The trampoline's work, exposed for tests: deliver one event's JSON
    /// to a handler as `Session` does. Returns whether it was an `ended`.
    static bool deliver_for_test(const EventHandler& handler, const char* json) {
        auto st = std::make_shared<State>();
        st->handler = handler;
        return st->deliver(json);
    }

private:
    struct State {
        std::mutex m;
        GdEmbed* handle = nullptr;
        EventHandler handler;
        Post post;
        std::atomic<bool> ended{false};

        // Returns true for the first "ended" (the library's last call).
        bool deliver(const char* json) {
            std::optional<Event> ev = parse_event(json ? json : "");
            if (!ev) return false;
            bool is_end = std::holds_alternative<EndedEvent>(*ev);
            if (is_end && ended.exchange(true, std::memory_order_acq_rel)) return false;
            if (!handler) return is_end;
            // An exception must never unwind into the C library: handlers
            // that throw have their exception dropped here (as the library
            // does with a panicking callback).
            try {
                if (post) {
                    post([h = handler, e = std::move(*ev)]() {
                        try {
                            h(e);
                        } catch (...) {
                        }
                    });
                } else {
                    handler(*ev);
                }
            } catch (...) {
            }
            return is_end;
        }
    };

    static void trampoline(void* user_data, const char* event_json) noexcept {
        if (user_data == nullptr) return;
        auto* keep = static_cast<std::shared_ptr<State>*>(user_data);
        std::shared_ptr<State> st = *keep;  // alive for this call
        try {
            if (st->deliver(event_json)) delete keep;  // nothing follows "ended"
        } catch (...) {
        }
    }

    std::shared_ptr<State> state_;
};

}  // namespace gaiadesk::embed

#endif  // GAIADESK_EMBED_HPP
