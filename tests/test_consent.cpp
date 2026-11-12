// The consent step's JSON both ways (libgaiadesk_embed 0.4.0). Pure.
// SPDX-License-Identifier: MIT
#include <gaiadesk/embed.hpp>

#include <cstdint>
#include <string>

#include "check.hpp"

namespace ge = gaiadesk::embed;
namespace j = gaiadesk::embed::detail::json;

TEST(unset_options_are_left_out) { CHECK_EQ(ge::consent_step_json({}), std::string("{}")); }

TEST(options_use_the_library_keys) {
    ge::ConsentStepOptions o;
    o.company = "Acme \"Desk\"";
    o.capture = ge::Capture::screen;
    o.guided = true;
    o.masked = false;
    o.brand_color = "#112233";
    o.parent_window = reinterpret_cast<void*>(std::uintptr_t{42});
    o.theme = ge::ConsentTheme{};
    o.theme->appearance = "dark";
    o.theme->radius = 12;
    o.theme->button_style = "pill";
    o.locale = "fr";
    o.wording = {{"title", "Partager ?"}, {"accept", ""}};
    o.wording_version = "acme-1";
    o.choices = ge::ConsentOffers{true, false, std::nullopt};
    o.privacy_url = "https://acme.test/privacy";
    o.timeout_seconds = 60;
    o.preview = false;
    o.embed_token = "gdemb_x";
    o.session_id = "ss_1";
    o.window_id = 7;
    o.masks = {{1, 2, 3, 4.5}};
    CHECK_EQ(ge::consent_step_json(o),
             std::string(R"({"company":"Acme \"Desk\"","capture":"screen","guided":true,"masked":false,"brand_color":"#112233",)"
                         R"("parent_window":42,"theme":{"appearance":"dark","radius":12,"button_style":"pill"},"locale":"fr",)"
                         R"("wording":{"accept":"","title":"Partager ?"},"wording_version":"acme-1",)"
                         R"("choices":{"offer_guided":true,"offer_logs":false},"privacy_url":"https://acme.test/privacy",)"
                         R"("timeout_seconds":60,"preview":false,"embed_token":"gdemb_x","session_id":"ss_1","window_id":7,)"
                         R"("masks":[{"x":1,"y":2,"w":3,"h":4.5}]})"));
    CHECK_EQ(ge::preview_json(o),
             std::string(R"({"capture":"screen","parent_window":42,"window_id":7,"masks":[{"x":1,"y":2,"w":3,"h":4.5}]})"));
}

TEST(answers_are_written_whole) {
    ge::ConsentAnswer a;
    a.decision = ge::ConsentOutcome::accepted;
    a.choices = {true, false, ge::Capture::window};
    a.wording = {{"title", "Share?"}};
    a.capture = ge::Capture::window;
    CHECK_EQ(ge::consent_answer_json(a),
             std::string(R"({"decision":"accepted","choices":{"guided":true,"share_logs":false,"capture":"window"},)"
                         R"("wording":{"title":"Share?"},"capture":"window"})"));
}

static const char* kDecision =
    R"({"decision":"accepted","choices":{"guided":false,"share_logs":true,"capture":"window"},"consent_token":"gdc_1",)"
    R"("receipt":{"id":"cr_01","decision":"accepted","at":"2026-11-12T16:04:05.123Z","surface":"embedded","locale":"en",)"
    R"("wording_version":"v1","wording_sha256":"ab","choices":{"guided":false,"share_logs":true,"capture":"window"},)"
    R"("os_user":null,"os":"macos","app_name":"Horus","app_version":null,"sdk":"libgaiadesk_embed/0.4.0"}})";

TEST(decisions_parse_and_keep_the_receipt_as_made) {
    auto d = ge::detail::parse_decision(kDecision);
    CHECK(d.has_value());
    if (!d) return;
    CHECK(d->accepted());
    CHECK(d->choices && d->choices->share_logs && !d->choices->guided && d->choices->capture == ge::Capture::window);
    CHECK_EQ(d->consent_token, std::string("gdc_1"));
    CHECK(d->receipt.has_value());
    CHECK_EQ(d->receipt->id, std::string("cr_01"));
    CHECK(!d->receipt->os_user && !d->receipt->app_version);
    CHECK(d->receipt->app_name && *d->receipt->app_name == "Horus");
    // The receipt goes to start exactly as the library made it.
    auto again = j::parse(d->receipt->json);
    CHECK(again && j::dump(*again) == d->receipt->json);
    CHECK(d->receipt->json.find("\"os_user\":null") != std::string::npos);

    auto none = ge::detail::parse_decision(R"({"decision":"unavailable"})");
    CHECK(none && none->decision == ge::ConsentOutcome::unavailable && !none->choices && !none->receipt);
    CHECK(!ge::detail::parse_decision("{}"));
    CHECK(!ge::detail::parse_decision("nope"));
}

TEST(start_with_a_decision_runs_with_the_customers_choices) {
    auto d = *ge::detail::parse_decision(kDecision);
    ge::Options o;
    o.embed_token = "gdemb_x";
    o.capture = ge::Capture::screen;
    o.guided = true;
    ge::Options a = ge::apply_consent(o, d);
    CHECK(a.capture == ge::Capture::window);
    CHECK(!a.guided);
    const std::string json = ge::config_json(a, true);
    CHECK(json.find(",\"consent_token\":\"gdc_1\"") != std::string::npos);
    CHECK(json.find(",\"consent_receipt\":" + d.receipt->json + "}") != std::string::npos);
    d.choices->guided = true;
    CHECK(ge::apply_consent(o, d).guided);
    CHECK(ge::config_json(o, true).find("consent_t") == std::string::npos);
}

int main() { return check::run_all(); }
