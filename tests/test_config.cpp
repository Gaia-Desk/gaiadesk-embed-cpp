// Options -> the library's config JSON.
// SPDX-License-Identifier: MIT
#include <gaiadesk/embed.hpp>

#include <limits>
#include <set>
#include <string>

#include "check.hpp"

namespace ge = gaiadesk::embed;
namespace j = gaiadesk::embed::detail::json;

static const std::string kToken = "gdemb_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

TEST(minimal_config_has_only_the_safe_defaults) {
    ge::Options o;
    o.embed_token = kToken;
    CHECK_EQ(ge::config_json(o, true),
             "{\"embed_token\":\"" + kToken + "\",\"consent_granted\":true,\"capture\":\"window\",\"guided\":false}");
}

TEST(consent_is_written_as_given) {
    ge::Options o;
    o.embed_token = kToken;
    auto v = j::parse(ge::config_json(o, false));
    CHECK(v.has_value());
    const j::Value* c = v->find("consent_granted");
    CHECK(c != nullptr && c->kind == j::Value::Kind::boolean && !c->boolean);
}

TEST(full_config_with_masks) {
    ge::Options o;
    o.embed_token = kToken;
    o.server = "https://support.acme.test";
    o.company = "Acme \"Desk\"";
    o.capture = ge::Capture::app;
    o.window_id = 18446744073709551615ull;  // the largest HWND-sized id survives exactly
    o.guided = true;
    o.fps = 30;
    o.masks = {{10, 20, 300, 40}, {0.5, 1.25, 2, 3}};
    CHECK_EQ(ge::config_json(o, true),
             "{\"embed_token\":\"" + kToken +
                 "\",\"consent_granted\":true,\"server\":\"https://support.acme.test\","
                 "\"company\":\"Acme \\\"Desk\\\"\",\"capture\":\"app\",\"window_id\":18446744073709551615,"
                 "\"guided\":true,\"fps\":30,\"masks\":[{\"x\":10,\"y\":20,\"w\":300,\"h\":40},"
                 "{\"x\":0.5,\"y\":1.25,\"w\":2,\"h\":3}]}");
}

TEST(output_is_valid_json_with_only_known_keys) {
    const std::set<std::string> known = {"embed_token", "consent_granted", "server", "company", "capture",
                                         "window_id",   "guided",          "fps",    "masks"};
    ge::Options o;
    o.embed_token = "line\nbreak \x01";
    o.company = "Z\xC3\xBCrich & Co";
    o.capture = ge::Capture::screen;
    o.window_id = 42;
    o.fps = 5;
    o.masks = {{1, 2, 3, 4}};
    auto v = j::parse(ge::config_json(o, true));
    CHECK(v.has_value() && v->is_object());
    for (const auto& m : v->members) {
        if (!known.count(m.first)) check::fail(__FILE__, __LINE__, "unknown key " + m.first);
    }
    CHECK_EQ(v->string_at("embed_token").value_or("?"), o.embed_token);
    CHECK_EQ(v->string_at("company").value_or("?"), o.company);
    CHECK_EQ(v->string_at("capture").value_or("?"), std::string("screen"));
    CHECK_EQ(v->int64_at("window_id").value_or(0), 42);
    CHECK_EQ(v->int64_at("fps").value_or(0), 5);
    const j::Value* masks = v->find("masks");
    CHECK(masks != nullptr && masks->items.size() == 1);
    if (masks && masks->items.size() == 1) {
        const auto& r = masks->items[0];
        CHECK_EQ(r.members.size(), 4u);
        CHECK_EQ(r.int64_at("x").value_or(0), 1);
        CHECK_EQ(r.int64_at("h").value_or(0), 4);
    }
}

TEST(empty_masks_and_empty_strings_are_left_out) {
    ge::Options o;
    o.embed_token = kToken;
    o.masks = {};
    auto v = j::parse(ge::config_json(o, true));
    CHECK(v->find("masks") == nullptr);
    CHECK(v->find("server") == nullptr);
    CHECK(v->find("company") == nullptr);
    CHECK(v->find("window_id") == nullptr);
    CHECK(v->find("fps") == nullptr);
}

TEST(a_non_finite_mask_becomes_null_for_the_library_to_refuse) {
    ge::Options o;
    o.embed_token = kToken;
    o.masks = {{0, 0, std::numeric_limits<double>::infinity(), 1}};
    auto v = j::parse(ge::config_json(o, true));
    CHECK(v.has_value());
    CHECK(v->find("masks")->items[0].find("w")->kind == j::Value::Kind::null);
}

TEST(capture_names) {
    CHECK_EQ(std::string(ge::to_string(ge::Capture::window)), std::string("window"));
    CHECK_EQ(std::string(ge::to_string(ge::Capture::app)), std::string("app"));
    CHECK_EQ(std::string(ge::to_string(ge::Capture::screen)), std::string("screen"));
}

TEST(rect_matches_the_c_struct) {
    static_assert(sizeof(ge::Rect) == sizeof(GdRect), "layout");
    ge::Rect r{1, 2, 3, 4};
    CHECK_EQ(r.x + r.y + r.w + r.h, 10.0);
}

int main() { return check::run_all(); }

TEST(brand_keys_are_written_only_when_set) {
    ge::Options o;
    o.embed_token = kToken;
    o.brand_color = "#0f766e";
    o.logo_png_base64 = "iVBORw0KGgo=";
    auto v = j::parse(ge::config_json(o, true));
    CHECK(v.has_value());
    CHECK(v->string_at("brand_color").value_or("") == "#0f766e");
    CHECK(v->string_at("logo_png_base64").value_or("") == "iVBORw0KGgo=");
    ge::Options plain;
    plain.embed_token = kToken;
    CHECK(ge::config_json(plain, true).find("brand_") == std::string::npos);
}

TEST(consent_options_json) {
    ge::ConsentOptions c;
    CHECK_EQ(ge::consent_json(c), "{\"capture\":\"window\",\"guided\":false}");
    c.company = "Acme";
    c.capture = ge::Capture::screen;
    c.guided = true;
    c.brand_color = "#112233";
    CHECK_EQ(ge::consent_json(c), "{\"capture\":\"screen\",\"guided\":true,\"company\":\"Acme\",\"brand_color\":\"#112233\"}");
}

TEST(placement_keys_are_written_only_when_set) {
    ge::Options o;
    o.embed_token = kToken;
    o.parent_window = reinterpret_cast<void*>(std::uintptr_t{140230000});
    o.indicator_position = ge::IndicatorPosition::bottom_left;
    o.indicator_origin = ge::Point{10.5, -20};
    auto v = j::parse(ge::config_json(o, true));
    CHECK(v.has_value());
    CHECK_EQ(v->int64_at("parent_window").value_or(0), 140230000);
    CHECK(v->string_at("indicator_position").value_or("") == "bottom-left");
    const j::Value* origin = v->find("indicator_origin");
    CHECK(origin != nullptr && origin->is_object());
    if (origin) CHECK(origin->double_at("x") == 10.5 && origin->double_at("y") == -20.0);
    ge::Options plain;
    plain.embed_token = kToken;
    std::string p = ge::config_json(plain, true);
    CHECK(p.find("parent_window") == std::string::npos && p.find("indicator_") == std::string::npos);
}

TEST(every_indicator_position_has_its_name) {
    CHECK_EQ(std::string(ge::to_string(ge::IndicatorPosition::top_left)), std::string("top-left"));
    CHECK_EQ(std::string(ge::to_string(ge::IndicatorPosition::top_right)), std::string("top-right"));
    CHECK_EQ(std::string(ge::to_string(ge::IndicatorPosition::bottom_left)), std::string("bottom-left"));
    CHECK_EQ(std::string(ge::to_string(ge::IndicatorPosition::bottom_right)), std::string("bottom-right"));
}

TEST(consent_masked_and_parent_window) {
    ge::ConsentOptions c;
    c.masked = true;
    c.parent_window = reinterpret_cast<void*>(std::uintptr_t{42});
    CHECK_EQ(ge::consent_json(c), "{\"capture\":\"window\",\"guided\":false,\"masked\":true,\"parent_window\":42}");
}
