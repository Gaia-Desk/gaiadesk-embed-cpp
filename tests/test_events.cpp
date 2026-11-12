// Event JSON -> typed events, and how a session delivers them.
// SPDX-License-Identifier: MIT
#include <gaiadesk/embed.hpp>

#include <functional>
#include <string>
#include <vector>

#include "check.hpp"

namespace ge = gaiadesk::embed;

// A copy of the event as T (nullopt when it is not one): never a pointer
// into a temporary.
template <class T>
static std::optional<T> as(const std::optional<ge::Event>& e) {
    if (!e || !std::holds_alternative<T>(*e)) return std::nullopt;
    return std::get<T>(*e);
}

TEST(state) {
    auto e = ge::parse_event(R"({"type":"state","state":"waiting"})");
    CHECK(as<ge::StateEvent>(e) && as<ge::StateEvent>(e)->state == "waiting");
    CHECK_EQ(ge::type_of(*e), std::string_view("state"));
}

TEST(code) {
    auto e = ge::parse_event(
        R"({"type":"code","code":"123456789","session_id":"ss_01ABC","mode":"cobrowse","expires_at":1767225600})");
    auto c = as<ge::CodeEvent>(e);
    CHECK(c.has_value());
    if (!c) return;
    CHECK_EQ(c->code, std::string("123456789"));
    CHECK_EQ(c->session_id, std::string("ss_01ABC"));
    CHECK_EQ(c->mode, std::string("cobrowse"));
    CHECK(c->expires_at.has_value() && *c->expires_at == 1767225600);
    auto bare = as<ge::CodeEvent>(ge::parse_event(R"({"type":"code","code":"1"})"));
    CHECK(bare && bare->mode == "view" && !bare->expires_at && bare->session_id.empty());
}

TEST(joined_and_left) {
    auto j = as<ge::JoinedEvent>(ge::parse_event(R"({"type":"joined","agent":"agent@acme.com"})"));
    CHECK(j && j->agent && *j->agent == "agent@acme.com");
    auto anon = as<ge::JoinedEvent>(ge::parse_event(R"({"type":"joined","agent":null})"));
    CHECK(anon && !anon->agent);
    CHECK(as<ge::LeftEvent>(ge::parse_event(R"({"type":"left"})")).has_value());
}

TEST(control) {
    auto on = as<ge::ControlEvent>(ge::parse_event(R"({"type":"control","allowed":true})"));
    auto off = as<ge::ControlEvent>(ge::parse_event(R"({"type":"control","allowed":false})"));
    auto odd = as<ge::ControlEvent>(ge::parse_event(R"({"type":"control","allowed":"yes"})"));
    CHECK(on && on->allowed);
    CHECK(off && !off->allowed);
    CHECK(odd && !odd->allowed);  // only a literal true allows
}

TEST(action) {
    auto a = as<ge::ActionEvent>(
        ge::parse_event(R"({"type":"action","action":"click","applied":false,"reason":"not_focused"})"));
    CHECK(a && a->action == "click" && !a->applied && a->reason && *a->reason == "not_focused");
    auto ok = as<ge::ActionEvent>(ge::parse_event(R"({"type":"action","action":"key","applied":true})"));
    CHECK(ok && ok->applied && !ok->reason);
}

TEST(error_and_ended) {
    auto err = as<ge::ErrorEvent>(
        ge::parse_event(R"({"type":"error","code":"capture_failed","message":"no window"})"));
    CHECK(err && err->code == "capture_failed" && err->message == "no window");
    auto end = as<ge::EndedEvent>(ge::parse_event(R"({"type":"ended","reason":"app_stopped"})"));
    CHECK(end && end->reason == "app_stopped");
    CHECK_EQ(ge::type_of(ge::Event{ge::EndedEvent{"stopped"}}), std::string_view("ended"));
}

TEST(indicator_moved_and_customer_cancelled) {
    auto m = as<ge::IndicatorMovedEvent>(ge::parse_event(R"({"type":"indicator_moved","x":1180.5,"y":-24})"));
    CHECK(m && m->x == 1180.5 && m->y == -24.0);
    CHECK_EQ(ge::type_of(ge::Event{ge::IndicatorMovedEvent{1, 2}}), std::string_view("indicator_moved"));
    auto end = as<ge::EndedEvent>(ge::parse_event(R"({"type":"ended","reason":"customer_cancelled"})"));
    CHECK(end && end->reason == "customer_cancelled");
}

TEST(unknown_types_keep_their_json) {
    const char* raw = R"({"type":"highlight","x":1})";
    auto u = as<ge::UnknownEvent>(ge::parse_event(raw));
    CHECK(u && u->type == "highlight" && u->json == raw);
    CHECK_EQ(ge::type_of(*ge::parse_event(raw)), std::string_view("highlight"));
}

TEST(non_events_are_nullopt) {
    for (const char* s : {"", "null", "[]", "{}", R"({"type":1})", R"({"state":"x"})", "{\"type\":\"state\"", "garbage"}) {
        if (ge::parse_event(s)) check::fail(__FILE__, __LINE__, std::string("parsed: ") + s);
    }
}

TEST(delivery_reports_only_the_first_ended) {
    std::vector<std::string> seen;
    ge::EventHandler h = [&](const ge::Event& e) { seen.emplace_back(ge::type_of(e)); };
    CHECK(!ge::Session::deliver_for_test(h, R"({"type":"state","state":"starting"})"));
    CHECK(ge::Session::deliver_for_test(h, R"({"type":"ended","reason":"error"})"));
    CHECK(!ge::Session::deliver_for_test(h, nullptr));
    CHECK(!ge::Session::deliver_for_test(h, "not json"));
    CHECK_EQ(seen.size(), 2u);
}

TEST(a_throwing_handler_never_escapes) {
    ge::EventHandler h = [](const ge::Event&) { throw std::runtime_error("app bug"); };
    bool ended = false;
    try {
        ended = ge::Session::deliver_for_test(h, R"({"type":"ended","reason":"stopped"})");
    } catch (...) {
        check::fail(__FILE__, __LINE__, "the handler's exception escaped");
    }
    CHECK(ended);
}

TEST(an_empty_session_is_inert) {
    ge::Session s;
    CHECK(!s.active());
    CHECK(!s);
    CHECK(!s.has_ended());
    CHECK(s.native_handle() == nullptr);
    s.set_paused(true);
    CHECK(s.try_set_mask_rects({{0, 0, 1, 1}}).ok());
    s.stop();
    s.stop();
    ge::Session moved = std::move(s);
    CHECK(!moved.active());
}

TEST(result_holds_a_value_or_an_error) {
    ge::Result<int> good(5);
    CHECK(good.ok() && *good == 5);
    ge::Result<int> bad(ge::ErrorInfo{"bad_token", "nope"});
    CHECK(!bad && bad.error().code == "bad_token");
    ge::Result<void> fine;
    CHECK(fine.ok());
    ge::Error err(ge::ErrorInfo{"consent_required", "ask first"});
    CHECK_EQ(std::string(err.what()), std::string("consent_required: ask first"));
    CHECK_EQ(err.code(), std::string("consent_required"));
}

int main() { return check::run_all(); }
