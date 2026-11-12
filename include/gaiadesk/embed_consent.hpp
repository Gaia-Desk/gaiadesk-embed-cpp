// gaiadesk-embed-cpp: the consent step (libgaiadesk_embed 0.4.0). Included
// by embed.hpp; include that, not this.
//
// The library's own dialog with a live preview of what support will see,
// per-session choices, localized wording, and a consent receipt the library
// makes (and, given the session's server, token and id, stores on it).
//
// SPDX-License-Identifier: MIT
#ifndef GAIADESK_EMBED_CONSENT_HPP
#define GAIADESK_EMBED_CONSENT_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace gaiadesk::embed {

/// The consent dialog's look. Empty / unset: the library's default.
struct ConsentTheme {
    std::string appearance;  ///< `auto` (the system's), `light`, `dark`
    std::string background;  ///< `#RRGGBB`
    std::string surface;
    std::string text;
    std::string accent;
    std::optional<int> radius;  ///< corner radius, 0 to 24
    std::string font_family;    ///< falls back to the system UI font
    std::string button_style;   ///< `filled`, `outline`, `pill`
};

/// What the customer may choose in the dialog.
struct ConsentOffers {
    std::optional<bool> offer_guided;   ///< "Allow support to click and type" (only when `guided`)
    std::optional<bool> offer_logs;     ///< "Also share app logs"
    std::optional<bool> offer_capture;  ///< one window or the whole screen
};

/// The consent step's options (`consent`, `preview`). Empty / unset ones are
/// left out and take the library's default; unknown keys cannot happen.
struct ConsentStepOptions {
    std::string company;             ///< `{company}` in the wording
    std::optional<Capture> capture;  ///< what is shared by default
    std::optional<bool> guided;      ///< the app wants click-and-type (the customer may still say no)
    std::optional<bool> masked;      ///< say private fields stay hidden (pass `masks` too)
    std::string brand_color;         ///< `#RRGGBB`
    std::string logo_png_base64;     ///< a PNG, at most 256 KiB
    void* parent_window = nullptr;   ///< NSWindow* / HWND: the dialog belongs to it
    std::optional<ConsentTheme> theme;
    std::string locale;  ///< `auto` or a BCP-47 tag
    /// Per-key overrides of the wording (`title`, `accept`, …; placeholders
    /// `{company}` `{team}` `{minutes}` `{count}`).
    std::map<std::string, std::string> wording;
    std::string wording_version;  ///< your id for your wording
    std::optional<ConsentOffers> choices;
    std::string privacy_url;  ///< https only; shown as a link
    std::string terms_url;
    std::optional<int> timeout_seconds;  ///< 0 never; else the dialog declines itself (`timeout`)
    std::optional<bool> preview;         ///< the live preview (default on)
    /// With `embed_token` (and `session_id`): the library posts the receipt
    /// itself and shows who is waiting.
    std::string server;
    std::string embed_token;
    std::string session_id;
    std::string app_version;  ///< into the receipt; default the executable's
    std::optional<std::uint64_t> window_id;  ///< the preview's window, as `Options::window_id`
    std::vector<Rect> masks;                 ///< the preview's masks, as `Options::masks`
};

namespace detail {
inline void put(std::string& s, const char* k, const std::string& v) {
    if (!v.empty()) s += std::string(s.size() > 1 ? "," : "") + '"' + k + "\":" + json::quote(v);
}
inline void put(std::string& s, const char* k, std::optional<bool> v) {
    if (v) s += std::string(s.size() > 1 ? "," : "") + '"' + k + "\":" + (*v ? "true" : "false");
}
inline void put_raw(std::string& s, const char* k, const std::string& raw) {
    s += std::string(s.size() > 1 ? "," : "") + '"' + k + "\":" + raw;
}
inline std::string masks_json(const std::vector<Rect>& masks) {
    std::string s = "[";
    for (std::size_t k = 0; k < masks.size(); ++k) {
        const Rect& r = masks[k];
        s += std::string(k ? "," : "") + "{\"x\":" + json::number(r.x) + ",\"y\":" + json::number(r.y) +
             ",\"w\":" + json::number(r.w) + ",\"h\":" + json::number(r.h) + '}';
    }
    return s + ']';
}
inline std::string choices_json(const ConsentChoices& c) {
    return std::string("{\"guided\":") + (c.guided ? "true" : "false") + ",\"share_logs\":" +
           (c.share_logs ? "true" : "false") + ",\"capture\":" + json::quote(to_string(c.capture)) + '}';
}
inline std::string wording_json(const std::map<std::string, std::string>& w) {
    std::string s = "{";
    for (const auto& kv : w) put_raw(s, kv.first.c_str(), json::quote(kv.second));
    return s + '}';
}
}  // namespace detail

/// `o` as the JSON `gd_embed_consent` takes. Pure.
inline std::string consent_step_json(const ConsentStepOptions& o) {
    namespace d = detail;
    std::string s = "{";
    d::put(s, "company", o.company);
    if (o.capture) d::put(s, "capture", std::string(to_string(*o.capture)));
    d::put(s, "guided", o.guided);
    d::put(s, "masked", o.masked);
    d::put(s, "brand_color", o.brand_color);
    d::put(s, "logo_png_base64", o.logo_png_base64);
    if (o.parent_window) d::put_raw(s, "parent_window", std::to_string(reinterpret_cast<std::uintptr_t>(o.parent_window)));
    if (o.theme) {
        std::string t = "{";
        d::put(t, "appearance", o.theme->appearance);
        d::put(t, "background", o.theme->background);
        d::put(t, "surface", o.theme->surface);
        d::put(t, "text", o.theme->text);
        d::put(t, "accent", o.theme->accent);
        if (o.theme->radius) d::put_raw(t, "radius", std::to_string(*o.theme->radius));
        d::put(t, "font_family", o.theme->font_family);
        d::put(t, "button_style", o.theme->button_style);
        d::put_raw(s, "theme", t + '}');
    }
    d::put(s, "locale", o.locale);
    if (!o.wording.empty()) d::put_raw(s, "wording", d::wording_json(o.wording));
    d::put(s, "wording_version", o.wording_version);
    if (o.choices) {
        std::string c = "{";
        d::put(c, "offer_guided", o.choices->offer_guided);
        d::put(c, "offer_logs", o.choices->offer_logs);
        d::put(c, "offer_capture", o.choices->offer_capture);
        d::put_raw(s, "choices", c + '}');
    }
    d::put(s, "privacy_url", o.privacy_url);
    d::put(s, "terms_url", o.terms_url);
    if (o.timeout_seconds) d::put_raw(s, "timeout_seconds", std::to_string(*o.timeout_seconds));
    d::put(s, "preview", o.preview);
    d::put(s, "server", o.server);
    d::put(s, "embed_token", o.embed_token);
    d::put(s, "session_id", o.session_id);
    d::put(s, "app_version", o.app_version);
    if (o.window_id) d::put_raw(s, "window_id", std::to_string(*o.window_id));
    if (!o.masks.empty()) d::put_raw(s, "masks", d::masks_json(o.masks));
    return s + '}';
}

/// Only what `gd_embed_preview` reads (capture, window, parent window,
/// masks). Pure.
inline std::string preview_json(const ConsentStepOptions& o) {
    ConsentStepOptions only;
    only.capture = o.capture;
    only.window_id = o.window_id;
    only.parent_window = o.parent_window;
    only.masks = o.masks;
    return consent_step_json(only);
}

/// Your own (or an embedded) consent UI's answer, for `record_consent`.
struct ConsentAnswer {
    ConsentOutcome decision = ConsentOutcome::declined;  ///< accepted, declined or timeout
    ConsentChoices choices;
    /// The strings your UI showed, by key: hashed into the receipt.
    std::map<std::string, std::string> wording;
    std::string wording_version;
    std::string locale;
    std::string company;
    std::optional<Capture> capture;  ///< must equal `choices.capture` when set
    std::string app_version;
    /// With `embed_token` (and `session_id`): the library posts the receipt too.
    std::string server;
    std::string embed_token;
    std::string session_id;
};

inline const char* to_string(ConsentOutcome d) noexcept {
    switch (d) {
        case ConsentOutcome::accepted: return "accepted";
        case ConsentOutcome::timeout: return "timeout";
        case ConsentOutcome::unavailable: return "unavailable";
        case ConsentOutcome::declined: break;
    }
    return "declined";
}

/// `a` as the JSON `gd_embed_record_consent_ex` takes. Pure.
inline std::string consent_answer_json(const ConsentAnswer& a) {
    namespace d = detail;
    std::string s = "{";
    d::put(s, "decision", std::string(to_string(a.decision)));
    d::put_raw(s, "choices", d::choices_json(a.choices));
    d::put_raw(s, "wording", d::wording_json(a.wording));
    d::put(s, "wording_version", a.wording_version);
    d::put(s, "locale", a.locale);
    d::put(s, "company", a.company);
    if (a.capture) d::put(s, "capture", std::string(to_string(*a.capture)));
    d::put(s, "app_version", a.app_version);
    d::put(s, "server", a.server);
    d::put(s, "embed_token", a.embed_token);
    d::put(s, "session_id", a.session_id);
    return s + '}';
}

/// One frame of exactly what support would see (masks filled black), at
/// most 480x300.
struct ConsentPreview {
    std::string png_base64;
    int width = 0;
    int height = 0;
    int masked_count = 0;
};

namespace detail {
inline void consent_trampoline(void* user_data, const char* event_json) noexcept {
    try {
        const auto* h = static_cast<const EventHandler*>(user_data);
        if (h && *h && event_json)
            if (auto ev = parse_event(event_json)) (*h)(*ev);
    } catch (...) {  // never unwind into the C library
    }
}
inline Result<ConsentDecision> decision_or_error(const char* raw) {
    if (raw == nullptr) return last_error();  // thread-local: read it now
    if (auto d = parse_decision(raw)) return *d;
    return ErrorInfo{"unknown", "the library returned no decision"};
}
}  // namespace detail

/// The consent step: the library's modal dialog (call it on your UI thread;
/// macOS: the main thread, else `wrong_thread`). `on_event` gets
/// `ConsentShownEvent` and `ConsentAcceptedEvent` / `ConsentDeclinedEvent`
/// on this thread before it returns. Refused options (`bad_config`) show no
/// dialog. `unavailable`: ask with your own UI, then `record_consent`.
inline Result<ConsentDecision> try_consent(const ConsentStepOptions& options = {}, EventHandler on_event = {}) {
    const std::string json = consent_step_json(options);
    return detail::decision_or_error(
        gd_embed_consent(json.c_str(), on_event ? &detail::consent_trampoline : nullptr, &on_event));
}

/// One frame of what support would see; `capture_unavailable` when there is
/// no permission or no window.
inline Result<ConsentPreview> try_preview(const ConsentStepOptions& options = {}) {
    const std::string json = preview_json(options);
    const char* raw = gd_embed_preview(json.c_str());
    if (raw == nullptr) return last_error();
    auto doc = detail::json::parse(raw);
    if (!doc || !doc->is_object()) return ErrorInfo{"unknown", "the library returned no preview"};
    auto n = [&](const char* k) { return static_cast<int>(doc->int64_at(k).value_or(0)); };
    return ConsentPreview{doc->string_at("png_base64").value_or(""), n("width"), n("height"), n("masked_count")};
}

/// Your own UI's answer made a receipt by the library (surface `embedded`);
/// an acceptance carries a `consent_token` for `Session::start`.
inline Result<ConsentDecision> try_record_consent(const ConsentAnswer& answer) {
    const std::string json = consent_answer_json(answer);
    return detail::decision_or_error(gd_embed_record_consent_ex(json.c_str()));
}

#if GAIADESK_EMBED_EXCEPTIONS
/// `try_consent`, throwing `Error` on a refusal.
inline ConsentDecision consent(const ConsentStepOptions& options = {}, EventHandler on_event = {}) {
    auto r = try_consent(options, std::move(on_event));
    if (!r) throw Error(r.error());
    return std::move(r).value();
}
/// `try_preview`, throwing `Error` on a refusal.
inline ConsentPreview preview(const ConsentStepOptions& options = {}) {
    auto r = try_preview(options);
    if (!r) throw Error(r.error());
    return std::move(r).value();
}
/// `try_record_consent`, throwing `Error` on a refusal.
inline ConsentDecision record_consent(const ConsentAnswer& answer) {
    auto r = try_record_consent(answer);
    if (!r) throw Error(r.error());
    return std::move(r).value();
}
#endif

}  // namespace gaiadesk::embed

#endif  // GAIADESK_EMBED_CONSENT_HPP
