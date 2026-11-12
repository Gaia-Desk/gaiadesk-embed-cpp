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
/// The customer dragged the indicator to (`x`, `y`). Keep it and pass it back
/// as `Options::indicator_origin` next time. Needs libgaiadesk_embed 0.3.0.
struct IndicatorMovedEvent {
    double x = 0;
    double y = 0;
};
/// Always the last event, exactly once: `stopped` (the customer),
/// `app_stopped`, `customer_cancelled` (Cancel on the indicator, or `stop()`,
/// before any agent was admitted: the request left the queue; 0.3.0),
/// `disconnected`, `expired`, `error`.
struct EndedEvent {
    std::string reason;
};
/// An event type newer than this wrapper: its `type` and the raw JSON.
struct UnknownEvent {
    std::string type;
    std::string json;
};

using Event = std::variant<StateEvent, CodeEvent, JoinedEvent, LeftEvent, ControlEvent, ActionEvent, ErrorEvent,
                           EndedEvent, IndicatorMovedEvent, UnknownEvent>;

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
    if (*type == "indicator_moved")
        return Event{IndicatorMovedEvent{o.double_at("x").value_or(0), o.double_at("y").value_or(0)}};
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
        default: return std::get_if<UnknownEvent>(&e) ? std::string_view(std::get<UnknownEvent>(e).type) : "";
    }
}

}  // namespace gaiadesk::embed

#endif  // GAIADESK_EMBED_EVENTS_HPP
