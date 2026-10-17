#ifndef BLACKHOLE_TESTS_CHECK_H
#define BLACKHOLE_TESTS_CHECK_H

// Minimal, dependency-free assertion harness for the test suite.
// See docs/standards/testing-and-performance.md. The vendored `lib/` is
// gitignored, so the harness lives here in `tests/` and is committed.

#include <cmath>
#include <cstdio>
#include <string>

namespace check {

inline int& failure_count() {
    static int n = 0;
    return n;
}

inline void fail(const char* file, int line, const std::string& what) {
    std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, what.c_str());
    ++failure_count();
}

inline int report(const char* name) {
    if (failure_count() == 0) {
        std::printf("PASS %s\n", name);
        return 0;
    }
    std::fprintf(stderr, "FAIL %s (%d check(s) failed)\n", name, failure_count());
    return 1;
}

} // namespace check

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) ::check::fail(__FILE__, __LINE__, "CHECK(" #cond ")");    \
    } while (0)

#define CHECK_CLOSE(a, b, tol)                                                 \
    do {                                                                       \
        const double _a = (a), _b = (b), _t = (tol);                           \
        if (!(std::fabs(_a - _b) <= _t))                                       \
            ::check::fail(__FILE__, __LINE__,                                  \
                          "CHECK_CLOSE(" #a ", " #b ")");                      \
    } while (0)

#endif // BLACKHOLE_TESTS_CHECK_H
