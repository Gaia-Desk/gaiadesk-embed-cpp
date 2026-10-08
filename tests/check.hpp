// A minimal test harness (no dependency to download): CHECK / CHECK_EQ
// record failures, TEST registers a case, run_all() reports and returns the
// exit code.
// SPDX-License-Identifier: MIT
#pragma once
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace check {
struct Case {
    const char* name;
    std::function<void()> fn;
};
inline std::vector<Case>& cases() {
    static std::vector<Case> c;
    return c;
}
inline int& failures() {
    static int f = 0;
    return f;
}
struct Register {
    Register(const char* name, std::function<void()> fn) { cases().push_back({name, std::move(fn)}); }
};
inline void fail(const char* file, int line, const std::string& what) {
    ++failures();
    std::cerr << file << ":" << line << ": FAILED: " << what << "\n";
}
template <class A, class B>
void eq(const char* file, int line, const char* ea, const char* eb, const A& a, const B& b) {
    if (!(a == b)) {
        std::ostringstream m;
        m << ea << " == " << eb << "\n    left:  " << a << "\n    right: " << b;
        fail(file, line, m.str());
    }
}
inline int run_all() {
    int cases_failed = 0;
    for (auto& c : cases()) {
        int before = failures();
        try {
            c.fn();
        } catch (const std::exception& e) {
            fail(c.name, 0, std::string("threw: ") + e.what());
        }
        bool ok = failures() == before;
        if (!ok) ++cases_failed;
        std::cout << (ok ? "ok    " : "FAIL  ") << c.name << "\n";
    }
    std::cout << cases().size() - static_cast<std::size_t>(cases_failed) << "/" << cases().size() << " cases passed\n";
    return cases_failed == 0 ? 0 : 1;
}
}  // namespace check

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2(a, b)
#define TEST(name)                                                                   \
    static void CHECK_CAT(test_, name)();                                            \
    static check::Register CHECK_CAT(reg_, name)(#name, &CHECK_CAT(test_, name));    \
    static void CHECK_CAT(test_, name)()
#define CHECK(cond) \
    do { if (!(cond)) check::fail(__FILE__, __LINE__, #cond); } while (0)
#define CHECK_EQ(a, b) check::eq(__FILE__, __LINE__, #a, #b, (a), (b))
