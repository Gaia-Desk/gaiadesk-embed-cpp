// gaiadesk-embed-cpp: a small JSON reader and writer, just enough for
// libgaiadesk_embed's config and events. Internal: the API may change.
//
// Why not a JSON library: the wrapper is header-only and lands in your
// translation units. Vendoring nlohmann/json (or any other) would put a second
// copy, possibly of another version, next to the one your app may already
// use, under the same names. The library's JSON is small and flat (one object
// of strings, numbers, booleans and null per event; a config we write
// ourselves), so a strict reader and writer of ~300 lines does the job with
// no dependency.
//
// Locale-independent throughout: numbers are never read or written through
// the C locale (a German locale would otherwise write 1,5 for 1.5).
//
// SPDX-License-Identifier: MIT
#ifndef GAIADESK_EMBED_DETAIL_JSON_HPP
#define GAIADESK_EMBED_DETAIL_JSON_HPP

#include <charconv>
#include <cmath>
#include <cstdint>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gaiadesk::embed::detail::json {

/// One JSON value. Numbers keep their text: the wrapper only ever reads
/// integers from them (`expires_at`), which `as_int64` parses exactly.
struct Value {
    enum class Kind { null, boolean, number, string, array, object };
    Kind kind = Kind::null;
    bool boolean = false;
    std::string text;  // string value, or a number's literal text
    std::vector<Value> items;
    std::vector<std::pair<std::string, Value>> members;

    bool is_object() const noexcept { return kind == Kind::object; }

    /// The member named `key`, or nullptr (the last one if repeated).
    const Value* find(std::string_view key) const noexcept {
        const Value* found = nullptr;
        for (const auto& m : members) {
            if (m.first == key) found = &m.second;
        }
        return found;
    }

    std::optional<std::string> string_at(std::string_view key) const {
        const Value* v = find(key);
        if (v == nullptr || v->kind != Kind::string) return std::nullopt;
        return v->text;
    }

    /// True only for a literal `true`.
    bool true_at(std::string_view key) const noexcept {
        const Value* v = find(key);
        return v != nullptr && v->kind == Kind::boolean && v->boolean;
    }

    std::optional<std::int64_t> int64_at(std::string_view key) const noexcept {
        const Value* v = find(key);
        if (v == nullptr || v->kind != Kind::number) return std::nullopt;
        std::int64_t out = 0;
        const char* b = v->text.data();
        const char* e = b + v->text.size();
        auto r = std::from_chars(b, e, out);
        if (r.ec != std::errc() || r.ptr != e) return std::nullopt;  // fractional, exponent, overflow
        return out;
    }
};

class Reader {
public:
    explicit Reader(std::string_view s) noexcept : s_(s) {}

    /// The whole input as one value, or nullopt for anything that is not
    /// exactly one well-formed JSON value.
    std::optional<Value> document() {
        Value v;
        if (!value(v, 0)) return std::nullopt;
        ws();
        if (i_ != s_.size()) return std::nullopt;
        return v;
    }

private:
    static constexpr int max_depth = 64;  // arrays and objects nested in each other
    std::string_view s_;
    std::size_t i_ = 0;

    void ws() noexcept {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }

    bool lit(std::string_view w) noexcept {
        if (s_.substr(i_, w.size()) != w) return false;
        i_ += w.size();
        return true;
    }

    bool value(Value& out, int depth) {
        if (depth >= max_depth) return false;  // at most 64 levels
        ws();
        if (i_ >= s_.size()) return false;
        char c = s_[i_];
        if (c == '{') return object(out, depth);
        if (c == '[') return array(out, depth);
        if (c == '"') {
            out.kind = Value::Kind::string;
            return string(out.text);
        }
        if (c == 't' || c == 'f') {
            out.kind = Value::Kind::boolean;
            out.boolean = c == 't';
            return lit(c == 't' ? "true" : "false");
        }
        if (c == 'n') {
            out.kind = Value::Kind::null;
            return lit("null");
        }
        return number(out);
    }

    bool object(Value& out, int depth) {
        out.kind = Value::Kind::object;
        ++i_;  // {
        ws();
        if (i_ < s_.size() && s_[i_] == '}') {
            ++i_;
            return true;
        }
        for (;;) {
            ws();
            std::string key;
            if (i_ >= s_.size() || s_[i_] != '"' || !string(key)) return false;
            ws();
            if (i_ >= s_.size() || s_[i_] != ':') return false;
            ++i_;
            Value v;
            if (!value(v, depth + 1)) return false;
            out.members.emplace_back(std::move(key), std::move(v));
            ws();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == '}') {
                ++i_;
                return true;
            }
            return false;
        }
    }

    bool array(Value& out, int depth) {
        out.kind = Value::Kind::array;
        ++i_;  // [
        ws();
        if (i_ < s_.size() && s_[i_] == ']') {
            ++i_;
            return true;
        }
        for (;;) {
            Value v;
            if (!value(v, depth + 1)) return false;
            out.items.push_back(std::move(v));
            ws();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == ']') {
                ++i_;
                return true;
            }
            return false;
        }
    }

    bool number(Value& out) {
        std::size_t start = i_;
        auto digits = [&] {
            std::size_t d = i_;
            while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
            return i_ - d;
        };
        if (i_ < s_.size() && s_[i_] == '-') ++i_;
        if (i_ < s_.size() && s_[i_] == '0') {
            ++i_;
        } else if (digits() == 0) {
            return false;
        }
        if (i_ < s_.size() && s_[i_] == '.') {
            ++i_;
            if (digits() == 0) return false;
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) ++i_;
            if (digits() == 0) return false;
        }
        out.kind = Value::Kind::number;
        out.text.assign(s_.substr(start, i_ - start));
        return true;
    }

    static void utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(std::uint32_t& out) noexcept {
        if (s_.size() - i_ < 4) return false;
        out = 0;
        for (int k = 0; k < 4; ++k) {
            char h = s_[i_++];
            std::uint32_t d;
            if (h >= '0' && h <= '9') d = static_cast<std::uint32_t>(h - '0');
            else if (h >= 'a' && h <= 'f') d = static_cast<std::uint32_t>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') d = static_cast<std::uint32_t>(h - 'A' + 10);
            else return false;
            out = (out << 4) | d;
        }
        return true;
    }

    bool string(std::string& out) {
        ++i_;  // opening quote
        while (i_ < s_.size()) {
            char c = s_[i_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i_ >= s_.size()) return false;
            char e = s_[i_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    std::uint32_t cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        std::uint32_t lo = 0;
                        if (!lit("\\u") || !hex4(lo) || lo < 0xDC00 || lo > 0xDFFF) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return false;  // a lone low surrogate
                    }
                    utf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;  // unterminated
    }
};

inline std::optional<Value> parse(std::string_view s) { return Reader(s).document(); }

/// `s` as a JSON string literal, quotes included. Bytes >= 0x80 pass through
/// unchanged (the library expects UTF-8).
inline std::string quote(std::string_view s) {
    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        auto u = static_cast<unsigned char>(c);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (u < 0x20) {
                    out += "\\u00";
                    out += hex[u >> 4];
                    out += hex[u & 0xF];
                } else {
                    out += c;
                }
        }
    }
    out += '"';
    return out;
}

/// A double as the shortest JSON number that reads back as the same value
/// ("10", "12.5"). JSON has no NaN or infinity: those become `null`, which
/// the library refuses (`bad_config`) rather than guess a size.
inline std::string number(double v) {
    if (!std::isfinite(v)) return "null";
    if (v == 0) return "0";  // also -0
    std::string text;
    for (int precision = 15; precision <= 17; ++precision) {
        std::ostringstream w;
        w.imbue(std::locale::classic());
        w.precision(precision);
        w << v;
        text = w.str();
        std::istringstream r(text);
        r.imbue(std::locale::classic());
        double back = 0;
        r >> back;
        if (back == v) break;
    }
    return text;
}

}  // namespace gaiadesk::embed::detail::json

#endif  // GAIADESK_EMBED_DETAIL_JSON_HPP
