#pragma once

// Minimal dependency-free test harness. Each TEST registers itself; main() runs
// all (or those matching argv[1]) and prints measured values so CI logs double
// as a measurement record.

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace vstest
{
struct Case { std::string name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
inline std::string& current() { static std::string c; return c; }

struct Registrar
{
    Registrar (const char* n, std::function<void()> f) { registry().push_back ({ n, std::move (f) }); }
};

inline void fail (const char* file, int line, const std::string& msg)
{
    ++failures();
    std::printf ("    FAIL %s:%d: %s\n", file, line, msg.c_str());
}

inline int runAll (int argc, char** argv)
{
    const std::string filter = argc > 1 ? argv[1] : "";
    int ran = 0, failedCases = 0;
    for (auto& c : registry())
    {
        if (! filter.empty() && c.name.find (filter) == std::string::npos)
            continue;
        current() = c.name;
        const int before = failures();
        std::printf ("[ RUN  ] %s\n", c.name.c_str());
        std::fflush (stdout);
        c.fn();
        const bool ok = failures() == before;
        std::printf ("[ %s ] %s\n", ok ? " OK " : "FAIL", c.name.c_str());
        ++ran;
        if (! ok) ++failedCases;
    }
    std::printf ("\n%d tests, %d failed\n", ran, failedCases);
    return failedCases == 0 && ran > 0 ? 0 : 1;
}
} // namespace vstest

#define VS_CAT2(a, b) a##b
#define VS_CAT(a, b) VS_CAT2(a, b)
#define TEST(name) \
    static void VS_CAT(test_fn_, __LINE__)(); \
    static vstest::Registrar VS_CAT(test_reg_, __LINE__) (name, VS_CAT(test_fn_, __LINE__)); \
    static void VS_CAT(test_fn_, __LINE__)()

#define CHECK(cond) do { if (! (cond)) vstest::fail (__FILE__, __LINE__, #cond); } while (0)
#define CHECK_MSG(cond, ...) do { if (! (cond)) { char b_[512]; std::snprintf (b_, sizeof b_, __VA_ARGS__); vstest::fail (__FILE__, __LINE__, std::string (#cond) + " | " + b_); } } while (0)
#define MEASURE(...) do { std::printf ("    measure: "); std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)
