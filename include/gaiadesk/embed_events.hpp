// gaiadesk-embed-cpp: the session's events, typed. Included by embed.hpp;
// include that, not this.
//
// SPDX-License-Identifier: MIT
#ifndef GAIADESK_EMBED_EVENTS_HPP
#define GAIADESK_EMBED_EVENTS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "detail/json.hpp"

namespace gaiadesk::embed {

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

// ------------------------------------------- the consent step's results --

/// What the customer chose in the consent step (and the session then runs
/// with). libgaiadesk_embed 0.4.0.
struct ConsentChoices {
    bool guided = false;
    bool share_logs = false;
    Capture capture = Capture::window;
};

/// The library's record of a consent answer (0.4.0): store it, let the
/// library post it to the session, or pass it to `Session::start`.
struct ConsentReceipt {
    std::string id;        ///< `cr_…`
    std::string decision;  ///< `accepted`, `declined`, `timeout`
    std::string at;        ///< RFC 3339, UTC, milliseconds
    std::string surface;   ///< `native_dialog` or `embedded`
    std::string locale;
    std::string wording_version;
    std::string wording_sha256;  ///< SHA-256 of the strings actually shown
    ConsentChoices choices;
    std::optional<std::string> os_user;  ///< the login name, never a full name or path
    std::string os;
    std::optional<std::string> app_name;
    std::optional<std::string> app_version;
    std::string sdk;   ///< `libgaiadesk_embed/<version>`
    std::string json;  ///< As the library made it: what `Options::consent_receipt` sends.
};

/// What the consent step decided.
enum class ConsentOutcome {
    accepted,
    declined,
    timeout,      ///< The dialog declined itself after `timeout_seconds`.
    unavailable,  ///< No dialog here: ask with your own UI, then `record_consent`.
};

/// The consent step's answer (`consent`, `record_consent`; 0.4.0).
struct ConsentDecision {
    ConsentOutcome decision = ConsentOutcome::unavailable;
    std::optional<ConsentChoices> choices;  ///< nullopt when unavailable
    std::string consent_token;              ///< accepted only: for `Options::consent_token`
    std::optional<ConsentReceipt> receipt;  ///< every outcome but unavailable
    bool accepted() const noexcept { return decision == ConsentOutcome::accepted; }
};

namespace detail {
inline std::optional<ConsentChoices> choices_of(const json::Value* v) {
    if (v == nullptr || !v->is_object()) return std::nullopt;
    ConsentChoices c;
    c.guided = v->true_at("guided");
    c.share_logs = v->true_at("share_logs");
    auto k = v->string_at("capture").value_or("window");
    c.capture = k == "screen" ? Capture::screen : k == "app" ? Capture::app : Capture::window;
    return c;
}

/// gd_embed_consent's / gd_embed_record_consent_ex's JSON; nullopt when it
/// is not a decision.
inline std::optional<ConsentDecision> parse_decision(std::string_view text) {
    auto doc = json::parse(text);
    if (!doc || !doc->is_object()) return std::nullopt;
    auto d = doc->string_at("decision");
    if (!d) return std::nullopt;
    ConsentDecision out;
    out.decision = *d == "accepted" ? ConsentOutcome::accepted
                   : *d == "declined" ? ConsentOutcome::declined
                   : *d == "timeout" ? ConsentOutcome::timeout
                                     : ConsentOutcome::unavailable;
    out.choices = choices_of(doc->find("choices"));
    out.consent_token = doc->string_at("consent_token").value_or("");
    if (const json::Value* r = doc->find("receipt"); r != nullptr && r->is_object()) {
        auto s = [&](std::string_view k) { return r->string_at(k).value_or(std::string()); };
        out.receipt = ConsentReceipt{s("id"), s("decision"), s("at"), s("surface"), s("locale"), s("wording_version"),
                                     s("wording_sha256"), choices_of(r->find("choices")).value_or(ConsentChoices{}),
                                     r->string_at("os_user"), s("os"), r->string_at("app_name"),
                                     r->string_at("app_version"), s("sdk"), json::dump(*r)};
    }
    return out;
}
}  // namespace detail

// ----------------------------------------------------------------- events --

/// `starting`, `waiting` (registered, no agent yet), `connected`, `ended`.
struct StateEvent {
    std::string state;
    /// On `waiting`, when the consent step's choices went with the join:
    /// what the session runs with. 0.4.0.
    std::optional<ConsentChoices> choices;
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
/// The customer dragged the indicator to (`x`, `y`). Keep it and pass it back
/// as `Options::indicator_origin` next time. Needs libgaiadesk_embed 0.3.0.
struct IndicatorMovedEvent {
    double x = 0;
    double y = 0;
};
/// Always the last event, exactly once: `stopped` (the customer),
/// `app_stopped`, `customer_cancelled` (Cancel on the indicator, or `stop()`,
/// before any agent was admitted: the request left the queue; 0.3.0),
/// `customer_declined` / `consent_timeout` (the consent receipt said no, or
/// the dialog timed out; 0.4.0), `disconnected`, `expired`, `error`.
struct EndedEvent {
    std::string reason;
};
/// `consent()`'s dialog is up, in this locale and wording version (0.4.0).
struct ConsentShownEvent {
    std::string locale;
    std::string wording_version;
};
/// The customer agreed in `consent()`'s dialog (0.4.0).
struct ConsentAcceptedEvent {
    ConsentChoices choices;
};
/// The customer said no in `consent()`'s dialog: `declined` or `timeout` (0.4.0).
struct ConsentDeclinedEvent {
    std::string reason;
};
/// An event type newer than this wrapper: its `type` and the raw JSON.
struct UnknownEvent {
    std::string type;
    std::string json;
};

using Event = std::variant<StateEvent, CodeEvent, JoinedEvent, LeftEvent, ControlEvent, ActionEvent, ErrorEvent,
                           EndedEvent, IndicatorMovedEvent, ConsentShownEvent, ConsentAcceptedEvent,
                           ConsentDeclinedEvent, UnknownEvent>;

/// One event's JSON as an `Event`; nullopt for JSON that is not an event
/// object. Unknown types become `UnknownEvent`. Pure.
inline std::optional<Event> parse_event(std::string_view json) {
    auto doc = detail::json::parse(json);
    if (!doc || !doc->is_object()) return std::nullopt;
    auto type = doc->string_at("type");
    if (!type) return std::nullopt;
    const auto& o = *doc;
    auto str = [&](std::string_view k) { return o.string_at(k).value_or(std::string()); };
    if (*type == "state") return Event{StateEvent{str("state"), detail::choices_of(o.find("choices"))}};
    if (*type == "code")
        return Event{CodeEvent{str("code"), str("session_id"), o.string_at("mode").value_or("view"), o.int64_at("expires_at")}};
    if (*type == "joined") return Event{JoinedEvent{o.string_at("agent")}};
    if (*type == "left") return Event{LeftEvent{}};
    if (*type == "control") return Event{ControlEvent{o.true_at("allowed")}};
    if (*type == "action") return Event{ActionEvent{str("action"), o.true_at("applied"), o.string_at("reason")}};
    if (*type == "error") return Event{ErrorEvent{str("code"), str("message")}};
    if (*type == "ended") return Event{EndedEvent{str("reason")}};
    if (*type == "indicator_moved")
        return Event{IndicatorMovedEvent{o.double_at("x").value_or(0), o.double_at("y").value_or(0)}};
    if (*type == "consent_shown") return Event{ConsentShownEvent{str("locale"), str("wording_version")}};
    if (*type == "consent_accepted")
        return Event{ConsentAcceptedEvent{detail::choices_of(o.find("choices")).value_or(ConsentChoices{})}};
    if (*type == "consent_declined") return Event{ConsentDeclinedEvent{o.string_at("reason").value_or("declined")}};
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
        case 8: return "indicator_moved";
        case 9: return "consent_shown";
        case 10: return "consent_accepted";
        case 11: return "consent_declined";
        default: return std::get_if<UnknownEvent>(&e) ? std::string_view(std::get<UnknownEvent>(e).type) : "";
    }
}

}  // namespace gaiadesk::embed

#endif  // GAIADESK_EMBED_EVENTS_HPP
