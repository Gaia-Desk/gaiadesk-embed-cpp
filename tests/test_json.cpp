// The wrapper's JSON reader and writer.
// SPDX-License-Identifier: MIT
#include <gaiadesk/detail/json.hpp>

#include <clocale>
#include <limits>
#include <locale>
#include <string>

#include "check.hpp"

namespace j = gaiadesk::embed::detail::json;

TEST(reads_a_flat_object) {
    auto v = j::parse(R"( {"type":"code","code":"123456789","n":-12,"t":true,"f":false,"z":null,"a":[1,{"b":2}]} )");
    CHECK(v.has_value());
    CHECK(v->is_object());
    CHECK_EQ(v->string_at("code").value_or("?"), std::string("123456789"));
    CHECK_EQ(v->int64_at("n").value_or(0), -12);
    CHECK(v->true_at("t"));
    CHECK(!v->true_at("f"));
    CHECK(!v->true_at("z"));
    CHECK(!v->string_at("z").has_value());
    CHECK(!v->string_at("missing").has_value());
    CHECK(!v->int64_at("code").has_value());
    const j::Value* a = v->find("a");
    CHECK(a != nullptr && a->kind == j::Value::Kind::array && a->items.size() == 2);
}

TEST(reads_escapes_and_unicode) {
    auto v = j::parse(R"({"s":"a\"b\\c\/d\n\t\u00e9\u20ac\ud83d\ude00"})");
    CHECK(v.has_value());
    CHECK_EQ(v->string_at("s").value_or("?"), std::string("a\"b\\c/d\n\t\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80"));
    // Raw UTF-8 passes through.
    auto u = j::parse("{\"agent\":\"z\xC3\xBC\x72ich@acme.com\"}");
    CHECK(u.has_value());
    CHECK_EQ(u->string_at("agent").value_or("?"), std::string("z\xC3\xBC\x72ich@acme.com"));
}

TEST(int64_only_for_exact_integers) {
    auto v = j::parse(R"({"a":1767225600,"b":1.5,"c":1e3,"d":99999999999999999999,"e":-0})");
    CHECK(v.has_value());
    CHECK_EQ(v->int64_at("a").value_or(0), 1767225600);
    CHECK(!v->int64_at("b").has_value());
    CHECK(!v->int64_at("c").has_value());
    CHECK(!v->int64_at("d").has_value());
    CHECK_EQ(v->int64_at("e").value_or(1), 0);
}

TEST(refuses_malformed_input) {
    const char* bad[] = {
        "", "{", "}", "{\"a\":}", "{\"a\" 1}", "{\"a\":1,}", "[1,]", "{\"a\":01}", "{\"a\":1.}", "{\"a\":-}",
        "{\"a\":tru}", "{\"a\":\"x}", "{\"a\":\"\\x\"}", "{\"a\":\"\\ud83d\"}", "{\"a\":\"\\ude00\"}",
        "{\"a\":\"\\u12G4\"}", "{\"a\":\"tab\there\"}", "{} {}", "{a:1}", "nul",
    };
    for (const char* s : bad) {
        if (j::parse(s).has_value()) check::fail(__FILE__, __LINE__, std::string("accepted: ") + s);
    }
}

TEST(nesting_is_bounded) {
    std::string deep(65, '[');
    deep += std::string(65, ']');
    CHECK(!j::parse(deep).has_value());
    std::string ok(60, '[');
    ok += std::string(60, ']');
    CHECK(j::parse(ok).has_value());
}

TEST(repeated_keys_take_the_last) {
    auto v = j::parse(R"({"type":"a","type":"b"})");
    CHECK_EQ(v->string_at("type").value_or("?"), std::string("b"));
}

TEST(quotes_strings) {
    CHECK_EQ(j::quote("plain"), std::string("\"plain\""));
    CHECK_EQ(j::quote("a\"b\\c"), std::string("\"a\\\"b\\\\c\""));
    CHECK_EQ(j::quote("l1\nl2\r\t\b\f"), std::string("\"l1\\nl2\\r\\t\\b\\f\""));
    CHECK_EQ(j::quote(std::string("\x01\x1f", 2)), std::string("\"\\u0001\\u001f\""));
    CHECK_EQ(j::quote("\xC3\xA9"), std::string("\"\xC3\xA9\""));
    // What we write, we read back.
    std::string nasty = "Acme \"Corp\"\\\n\x02 \xE2\x82\xAC";
    auto v = j::parse("{\"s\":" + j::quote(nasty) + "}");
    CHECK(v.has_value());
    CHECK_EQ(v->string_at("s").value_or("?"), nasty);
}

TEST(writes_numbers_shortest_and_locale_free) {
    CHECK_EQ(j::number(0), std::string("0"));
    CHECK_EQ(j::number(-0.0), std::string("0"));
    CHECK_EQ(j::number(10), std::string("10"));
    CHECK_EQ(j::number(12.5), std::string("12.5"));
    CHECK_EQ(j::number(0.1), std::string("0.1"));
    CHECK_EQ(j::number(-3.25), std::string("-3.25"));
    CHECK_EQ(j::number(1.0 / 3.0), std::string("0.3333333333333333"));
    CHECK_EQ(j::number(std::numeric_limits<double>::quiet_NaN()), std::string("null"));
    CHECK_EQ(j::number(std::numeric_limits<double>::infinity()), std::string("null"));
    // A global locale with a decimal comma must not leak into the JSON.
    std::locale old;
    try {
        std::locale::global(std::locale("de_DE.UTF-8"));
    } catch (...) {
        try {
            std::locale::global(std::locale("de_DE"));
        } catch (...) {
        }
    }
    std::setlocale(LC_ALL, "de_DE.UTF-8");
    CHECK_EQ(j::number(1.5), std::string("1.5"));
    CHECK_EQ(j::number(1234567.25), std::string("1234567.25"));
    std::locale::global(old);
    std::setlocale(LC_ALL, "C");
}

int main() { return check::run_all(); }
