#pragma once

// Tiny self-contained test harness (no external framework, so the tests also
// build on a Raspberry Pi without network access).

#include <cmath>
#include <cstdio>

namespace test {

inline int& failures()
{
    static int count = 0;
    return count;
}

} // namespace test

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            ++test::failures();                                                       \
        }                                                                             \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                         \
    do {                                                                              \
        const double va_ = static_cast<double>(a);                                    \
        const double vb_ = static_cast<double>(b);                                    \
        if (std::fabs(va_ - vb_) > static_cast<double>(tol)) {                        \
            std::printf("  FAIL %s:%d: %s = %f, expected %f +/- %f\n", __FILE__,      \
                        __LINE__, #a, va_, vb_, static_cast<double>(tol));            \
            ++test::failures();                                                       \
        }                                                                             \
    } while (0)

#define RUN(fn)                                                                       \
    do {                                                                              \
        std::printf("[ RUN ] %s\n", #fn);                                             \
        const int before_ = test::failures();                                         \
        fn();                                                                         \
        std::printf("[ %s ] %s\n", test::failures() == before_ ? " OK " : "FAIL", #fn); \
    } while (0)

#define TEST_RESULT() (test::failures() == 0 ? 0 : 1)
