// The C++ wrapper against the real libgaiadesk_embed: only refusals the
// library makes before it touches the network or the screen. Nothing here
// starts a session.
// SPDX-License-Identifier: MIT
#include <gaiadesk/embed.hpp>

#include <atomic>
#include <limits>
#include <string>
#include <thread>

#include "check.hpp"

namespace ge = gaiadesk::embed;

static const std::string kToken = "gdemb_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static std::atomic<int> g_events{0};

static ge::Options valid() {
    ge::Options o;
    o.embed_token = kToken;
    o.company = "Acme";
    return o;
}

// The refusal code for `o`, or "started" (which would be a test failure:
// these configs must all be refused).
static std::string refusal(const ge::Options& o, bool consent) {
    auto r = ge::Session::try_start(o, consent, [](const ge::Event&) { ++g_events; });
    if (r) {
        r->stop();
        return "started";
    }
    if (r.error().message.empty()) return "no message";
    return r.error().code;
}

TEST(library_version_and_build) {
    CHECK_EQ(ge::version(), std::string("0.2.0"));
    ge::BuildInfo b = ge::build_info();
    CHECK(!b.json.empty());
    CHECK_EQ(b.version, std::string("0.2.0"));
    CHECK(!b.test_source);  // a test-source build must never ship
    CHECK(!b.codec.empty());
#if defined(__APPLE__)
    CHECK_EQ(b.os, std::string("macos"));
#elif defined(_WIN32)
    CHECK_EQ(b.os, std::string("windows"));
#else
    CHECK_EQ(b.os, std::string("linux"));
#endif
    std::cout << "  build_info: " << b.json << "\n";
}

TEST(consent_false_is_consent_required) {
    CHECK_EQ(refusal(valid(), false), std::string("consent_required"));
    // Consent is checked before anything else, even a bad token.
    ge::Options bad = valid();
    bad.embed_token = "nope";
    CHECK_EQ(refusal(bad, false), std::string("consent_required"));
}

TEST(a_bad_token_is_bad_token) {
    for (const char* t : {"", "gdemb_", "gdemb_xyz", "gd_live_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                          "gdemb_0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef"}) {
        ge::Options o = valid();
        o.embed_token = t;
        CHECK_EQ(refusal(o, true), std::string("bad_token"));
    }
}

TEST(a_bad_mask_is_bad_config) {
    ge::Options o = valid();
    o.masks = {{0, 0, -1, 5}};
    CHECK_EQ(refusal(o, true), std::string("bad_config"));
    o.masks = {{0, 0, std::numeric_limits<double>::quiet_NaN(), 5}};
    CHECK_EQ(refusal(o, true), std::string("bad_config"));
    o.masks.assign(65, ge::Rect{0, 0, 1, 1});
    CHECK_EQ(refusal(o, true), std::string("bad_config"));
}

TEST(other_refusals) {
    ge::Options o = valid();
    o.server = "http://example.com";
    CHECK_EQ(refusal(o, true), std::string("bad_server"));
    o = valid();
    o.fps = 0;
    CHECK_EQ(refusal(o, true), std::string("bad_config"));
    o.fps = 31;
    CHECK_EQ(refusal(o, true), std::string("bad_config"));
    o = valid();
    o.window_id = 0;
    CHECK_EQ(refusal(o, true), std::string("bad_config"));
}

TEST(refusals_send_no_events) { CHECK_EQ(g_events.load(), 0); }

#if GAIADESK_EMBED_EXCEPTIONS
TEST(start_throws_the_refusal) {
    ge::Options o = valid();
    o.embed_token = "gdemb_bad";
    try {
        auto s = ge::Session::start(o, true, [](const ge::Event&) {});
        check::fail(__FILE__, __LINE__, "started with a bad token");
    } catch (const ge::Error& e) {
        CHECK_EQ(e.code(), std::string("bad_token"));
        CHECK(!e.message().empty());
    }
}
#endif

TEST(last_error_is_per_thread) {
    ge::Options o = valid();
    o.embed_token = "gdemb_bad";
    (void)ge::Session::try_start(o, true, nullptr);
    CHECK_EQ(ge::last_error().code, std::string("bad_token"));
    std::string other = "unset";
    std::thread([&] { other = gd_embed_last_error(); }).join();
    CHECK_EQ(other, std::string(""));
}

int main() { return check::run_all(); }
